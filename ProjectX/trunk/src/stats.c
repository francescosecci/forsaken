/*******************************************************************\
*																	
*	stats.c														
*																
*	Deals with all statistical related proceedures				
*																
********************************************************************\
*
*	List of places where UpdateKillStats() is called:
*		
*		networking.c	- update stats 1 (stats.c) -- somebody killed me
*		networking.c	- update stats 2 (stats.c) -- you killed someone 
*		networking.c	- update stats 3 (stats.c) -- somebody killed someone
*		models.c	- update stats 4 (stats.c) -- killed yourself with missile splash damage
*		models.c	- update stats 5 (stats.c) -- i killed myself
*		models.c	- update stats 6 (stats.c) -- somebody killed me
*		networking.c	- update stats 7 (stats.c) -- you killed someone on your own team
*		networking.c	- update stats 8 (stats.c) -- you killed someone in a team bounty game
*		networking.c	- update stats 9 (stats.c) -- you killed someone in a team game
*		networking.c	- update stats 10 (stats.c) -- you killed someone in a bounty game
*
*	List of places where UpdateBonusStats() is called:
*
*		Ships.c		- update bonus 2 (stats.c) -- flag chase scored
*		Ships.c		- update bonus 3 (stats.c) -- CTF scored
*		Ships.c		- update bonus 4 (stats.c) -- bounty hunt points
*		networking.c	- update bonus 6 (stats.c) -- someone else scored with flag
*		Ships.c		- update bonus 7 (stats.c) -- someone else's bounty hunt points -- Notes: there is a lag of 1 point if person has it when u join
*
\*******************************************************************/

#include "stats.h"
#include <stdio.h>
#include "new3d.h"
#include "quat.h"
#include "compobjects.h"
#include "object.h"
#include "networking.h"
#include "2dtextures.h"
#include "primary.h"
#include "secondary.h"
#include "visi.h"
#include "text.h"
#include "xmem.h"

/* external variables */
extern  BYTE  TeamNumber[MAX_PLAYERS];										// which team each player is on
extern char* GetName(int Player);															// returns player's short name from Ships.c
extern BOOL TeamGame;																// team game? (from Title.c)
extern int MilestoneMessagesColour;													// colour to display messages (from Title.c)

/* internal variables */
int	PrimaryStats[MAX_PLAYERS+1][MAXPRIMARYWEAPONS+1];				// PrimaryStats[Killer][PrimaryWeaponType];
int	SecondaryStats[MAX_PLAYERS+1][TOTALSECONDARYWEAPONS];		// SecondaryStats[Killer][SecondaryWeaponType];
int	KillStats[MAX_PLAYERS+1][MAX_PLAYERS+1];								// each player's KillStats[Killer][Victim];
int BonusStats[MAX_PLAYERS+1];													// each player's ctf/bounty points
int KillCounter[MAX_PLAYERS+1];													// number of kills made during this life
int x, z;																						// index counters
int	ScoreSortTab[MAX_PLAYERS + 1];												// player numbers in order of highest score to lowest 								
char *PrimaryWeaponName[MAXPRIMARYWEAPONS+1]			= { "PULSAR", "TROJAX", "PYROLITE", "TRANSPULSE", "SUSS-GUN", "LASER", "ORBITAL" };
char *SecondaryWeaponName[TOTALSECONDARYWEAPONS]	= { "MUG", "SOLARIS", "THIEF", "SCATTER", "GRAVGON", "MFRL", "TITAN", "PURGE MINE", "PINE MINE", "QUANTUM MINE", "SPIDER MINE", "PINE MISSILE", "TITAN SHRAPNEL", "ENEMY SPIRAL MISSILE", "ENEMY HOMING MISSILE", "ENEMY BLUE HOMING MISSILE", "ENEMY FIREBALL", "ENEMY TENTACLE", "ENEMY DEPTH CHARGE" };
BOOL FirstBlood = FALSE;
BOOL FirstToFifty = FALSE;

/*===================================================================
  Procedure :   Get Weapon Name...
  Input   :   int: Weapon Type, int: Weapon ID
  Output    :   char*: name of weapon
===================================================================*/
/* Get weapon name */
char* GetWeaponName(int WeaponType, int Weapon)
{
	if(WeaponType == WEPTYPE_Primary)
		return PrimaryWeaponName[Weapon];
	else if(WeaponType == WEPTYPE_Secondary)
		return SecondaryWeaponName[Weapon];
	else
		return "GetWeaponName():: INVALID WEAPON TYPE";
}

/*===================================================================
  Procedure :   Get Weapon Kill Stats...
  Input   :   int: Player ID, int: Weapon Type, int Weapon ID
  Output    :   int: number of kills
===================================================================*/
/* Get Weapon Kill Stats */
int GetWeaponKillStats(int PlayerID, int WeaponType, int Weapon)
{
	if(WeaponType == WEPTYPE_Primary)
		return PrimaryStats[PlayerID][Weapon];
	else if (WeaponType == WEPTYPE_Secondary)
		return SecondaryStats[PlayerID][Weapon];
	else
		return -1;
}

/*===================================================================
  Procedure :   Reset all statistics...
  Input   :   nothing
  Output    :   nothing
===================================================================*/
/* Reset All Statistics */
void ResetAllStats()
{
	// for every player
	for(x = 0; x < MAX_PLAYERS; x++)
	{
		// reset all player's individual kill stats
		for(z = 0; z < MAX_PLAYERS; z++)
			KillStats[x][z] = 0;

		// reset all player's primary weapon kill stats
		for(z = 0; z < MAXPRIMARYWEAPONS+1; z++)
			PrimaryStats[x][z] = 0;

		// reset all player's secondary weapon kill stats
		for(z= 0; z < TOTALSECONDARYWEAPONS; z++)
			SecondaryStats[x][z] = 0;

		// reset all player's bonus stats
		BonusStats[x] = 0;

		// reset all player's sequential kill counters
		KillCounter[x] = 0;
	}
}


/*===================================================================
  Procedure :   Reset one player's statistics...
  Input   :   player id
  Output    :   nothing
===================================================================*/
/* Reset All Statistics */
void ResetIndividualStats(int Player)
{
	// reset player's individual kill stats
	for(z = 0; z < MAX_PLAYERS; z++)
		KillStats[Player][z] = 0;

	// reset player's primary weapon kill stats
	for(z = 0; z < MAXPRIMARYWEAPONS+1; z++)
		PrimaryStats[Player][z] = 0;

	// reset player's secondary weapon kill stats
	for(z= 0; z < TOTALSECONDARYWEAPONS; z++)
		SecondaryStats[Player][z] = 0;

	// reset player's bonus stats
	BonusStats[Player] = 0;

	// reset player's sequential kill counter
	KillCounter[Player] = 0;
}
/*===================================================================
  Procedure :   Update Kill Statistics...
  Input   :   killer id, victim id, weapon type, weapon used
  Output    :   nothing
===================================================================*/
/* Update Individual Kill and Weapon Kill Statistics */
void UpdateKillStats(int Killer, int Victim, int WeaponType, int Weapon)
{
	char tempstr[256];
	int i=0;
	// should we check for the first kill of the game?
	if(!FirstBlood)
	{
			for(i=0; i<MAX_PLAYERS; i++)
			{
					if(GetTotalKills(i) > 0)
					{
						FirstBlood = TRUE;
						break;
					}
			}
			
			// this is the first kill
			if(!FirstBlood)
			{
					FirstBlood = TRUE;					
					if(Killer == WhoIAm)
					{
						AddColourMessageToQue(MilestoneMessagesColour, "YOU GOT FIRST BLOOD");
						PlaySfx( SFX_FIRSTBLOOD, 1.0F );
					}
					else
					{
						sprintf( (char*)&tempstr[0], "%s %s", (const char *)GetName(Killer), "GOT FIRST BLOOD" );
						AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					}
			}
	}
	// should we check for first to fifty kills?
	if(!FirstToFifty)
	{
			for(i=0; i<MAX_PLAYERS; i++)
			{
					if(GetTotalKills(i) >= 50)
					{
						FirstToFifty = TRUE;
						break;
					}
			}
			
			// this is the person to 50 kills
			if(!FirstToFifty && Killer!=Victim && GetTotalKills(Killer) == 49)
			{
					FirstToFifty = TRUE;
					if(Killer == WhoIAm)
					{
						AddColourMessageToQue( MilestoneMessagesColour, "YOU ARE FIRST TO 50 KILLS" );
						PlaySfx( SFX_IMPRESSIVE, 1.0F );
					}
					else
					{
						sprintf( (char*)&tempstr[0], "%s %s", (const char *)GetName(Killer), "IS FIRST TO 50 KILLS" );
						AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					}
			}
	}

	// killed with a direct hit of a titan
	if(WeaponType == WEPTYPE_Secondary && Weapon == 6)
	{
			PlaySfx( SFX_HOLYSHIT, 1.0F );
			AddColourMessageToQue( MilestoneMessagesColour, "HOLY SHIT" );
			return;				
	}
	// killed with a gravgon
	if(WeaponType == WEPTYPE_Secondary && Weapon == 4)
	{
			if(Victim == WhoIAm) PlaySfx( SFX_HUMILIATION, 1.0F );
			AddColourMessageToQue( MilestoneMessagesColour, "HUMILIATION" );
			return;		
	}

	// note who killed whom
	KillStats[Killer][Victim]++;
	// note weapon used
	if(WeaponType == WEPTYPE_Primary)
		PrimaryStats[Killer][Weapon]++;
	else if (WeaponType == WEPTYPE_Secondary)
		SecondaryStats[Killer][Weapon]++;

	// update the current kills this life for the killer - but not if player killed himself or his team mate
	if((Killer == Victim) || ((TeamNumber[Killer] ==TeamNumber[Victim]) && TeamGame))
	{ }
	else
		UpdateKillCount(Killer);

	// reset the current kills this life for the victim
	KillCounter[Victim] = 0;

}

/*===================================================================
  Procedure :   Update Sequential Kill Count and displays messages as appropriate...
  Input   :   killer id
  Output    :   nothing
===================================================================*/
/* Update number of kills made during this life */
void UpdateKillCount(int Killer)
{
	char	tempstr[256];
	char	prefix[256];
	BOOL PlaySound = FALSE;

	// update counter
	KillCounter[Killer]++;

	// name of killer
	if(Killer == WhoIAm)
	{
		strcpy(prefix, "YOU ARE");
		PlaySound = TRUE;
	}
	else
	{
		strcpy(prefix, (const char *)GetName(Killer));
		strcat(prefix, " IS");
	}

	// check for milestone achievements
	switch(KillCounter[Killer])
	{
		case 3:	
					if(PlaySound) PlaySfx( SFX_KILLINGSPREE, 1.0F );
					sprintf( (char*)&tempstr[0], "%s %s", prefix, "ON A KILLING SPREE (3 KILLS)" );
   					AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					break;
		case 5:
					if(PlaySound) PlaySfx( SFX_RAMPAGE, 1.0F );
					sprintf( (char*)&tempstr[0], "%s %s", prefix, "ON A RAMPAGE (5 KILLS)" );
   					AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					break;
		case 8:
					if(PlaySound) PlaySfx( SFX_DOMINATING, 1.0F );
					sprintf( (char*)&tempstr[0], "%s %s", prefix, "DOMINATING (8 KILLS)" );
   					AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					break;
		case 11:
					if(PlaySound) PlaySfx( SFX_UNSTOPPABLE, 1.0F );
					sprintf( (char*)&tempstr[0], "%s %s", prefix, "UNSTOPPABLE (11 KILLS)" );
   					AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					break;
		case 15:
					if(PlaySound) PlaySfx( SFX_WICKEDSICK, 1.0F );
					sprintf( (char*)&tempstr[0], "%s %s", prefix, "WICKED SICK (15 KILLS)" );
   					AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					break;
		case 20:
					if(PlaySound) PlaySfx( SFX_GODLIKE, 1.0F );
					sprintf( (char*)&tempstr[0], "%s %s", prefix, "GOD LIKE (20 KILLS)" );
   					AddColourMessageToQue( MilestoneMessagesColour, (char*)&tempstr[0] );
					break;
	}
}

/*===================================================================
  Procedure :   Update Individual bonus statistics...
  Input   :   player id, points scored
  Output    :   nothing
===================================================================*/
/* Update Individual bonus statistics */
void UpdateBonusStats(int Player, int Points)
{
	BonusStats[Player] += Points;
}
/*===================================================================
  Procedure :   Get Individual Kill Statistics...
  Input   :   killer id, victim id
  Output    :   amount of kills
===================================================================*/
/* Get An Individual Kill Statistic */
int GetKillStats(int Killer, int Victim)
{
	// return the amount of kills on victim
	return KillStats[Killer][Victim];	
}

/*===================================================================
  Procedure :   Get a player's score...
  Input   :   player id
  Output    :   overall score
===================================================================*/
/* Get An Individual Score */
int GetScoreStats(int Player)
{
	int score = GetKills( Player ); // kills - suacides - friendly

	// add bonus points
	score += GetBonusStats(Player);

	// return an individual player's score
	return score;
}

/*===================================================================
  Procedure :   Get an individual's bonus statistics (e.g. flag or bounty points)...
  Input   :   player id
  Output    :   bonus points
===================================================================*/
/* Get an individual's bonus statistics */
int GetBonusStats(int Player)
{
	// return an individual player's bonus points
	return BonusStats[Player];	
}

/*===================================================================
  Procedure :   Get Team Kills...
  Input   :   killer id
  Output    :   total kills achieved by all players on the same team (minuses suicides and 'friendly kills')
===================================================================*/
/* Get A Player's Team's Total Kills */
int GetTeamKills(int Killer)
{
	int TeamKills = 0; // total kills team made

	// search all players
	for(x = 0; x < MAX_PLAYERS; x++)
	{
		// found a team mate or myself
		if(TeamNumber[Killer] == TeamNumber[x])
		{
			// search all the players that my team mate or i killed
			for(z = 0; z < MAX_PLAYERS; z++)
			{
				// add kills
				if(x!=z && TeamNumber[x] != TeamNumber[z])
					TeamKills += GetKillStats(x,z);
				// minus suicides and friendly kills
				else
					TeamKills -= GetKillStats(x,z);
			}
		}
	}

	return TeamKills;
}

/*===================================================================
  Procedure :   Get Team Score (includes kills)...
  Input   :   killer id
  Output    :   total score achieved by all players on the same team (minuses suicides and 'friendly kills')
===================================================================*/
/* Get A Team's Total Score */
int GetTeamScore(int Player)
{
	int TeamScore = 0; // total score team achieved

	// add player's team's scores
	for(z = 0; z < MAX_PLAYERS; z++)
	{
		if(TeamNumber[Player] == TeamNumber[z])
			TeamScore += GetScoreStats(z);
	}

	return TeamScore;
}

/*===================================================================
  Procedure :   Get Total Kills...
  Input   :   killer id
  Output    :   total kills on all players (excludes suicides)
===================================================================*/
/* Get A Player's Total Kills */
int GetTotalKills(int Killer)
{
	int kills = 0;
	char tempstr[256];
	char prefix[256];

	// add kills achieved on all players
	for(x = 0; x < MAX_PLAYERS; x++)
	{
		// don't add suicides
		if(Killer!=x)
			kills += GetKillStats(Killer,x);	// add kills
	}

	return kills;
}

/*===================================================================
  Procedure :   Get Total Deaths...
  Input   :   victim id
  Output    :   total deaths (include suicides)
===================================================================*/
/* Get A Player's Total Deaths */
int GetTotalDeaths(int Victim)
{
	int deaths = 0; // total number of deaths

	// sum deaths
	for(x = 0; x < MAX_PLAYERS; x++)
		deaths += GetKillStats(x,Victim);

	return deaths;
}

/*===================================================================
	Procedure	:		Sort players according to Score.....
	Input		:		nothing
	Output		:		nothing
===================================================================*/
extern BYTE	GameStatus[MAX_PLAYERS + 1];
void ScoreSort()
{
	int i;
	int temp;
	int players = 0;

	// we need to know number of players because scores can be negative
	// meaning that uninitialized scores will be better than a negative score...
	// so you'll disapear off the player list...
	for (i = 0; i < MAX_PLAYERS; i++)
		if ( scoreable_status(i) )
			players++;

	while( TRUE )
	{
		int swapped = FALSE;
		for( i = 0; i < (players-1); i++ ) // -1 because we need to compare with next valid player
		{
			// if my score is worse than player bellow me
			if(	GetRealScore(ScoreSortTab[i]) < GetRealScore(ScoreSortTab[i+1])	) 
			{
				// swap places
				temp				= ScoreSortTab[i];
				ScoreSortTab[i]		= ScoreSortTab[i+1];
				ScoreSortTab[i+1]	= temp;
				swapped				= TRUE;
			}
		}
		// no players needed swapping
		if ( swapped == FALSE )
			break;
	}

}

/*===================================================================
	Procedure	:		PlayerRank
	Input		:		Position e.g. 1st, 2nd, 3rd.. (0 = 1st)
	Output		:	Player ID
===================================================================*/

int GetPlayerByRank( int rank )
{
	return ScoreSortTab[ rank ];
}

/*===================================================================
	Procedure	:		Initiate Score Sort Array
	Input		:		Player ID
	Output		:	nothing
===================================================================*/
void InitScoreSortTab(int Player)
{
	ScoreSortTab[Player] = Player;
}

/*===================================================================
  Procedure :   Get Team Score (includes kills)...
  Input   :   team number
  Output    :   total score achieved by all players on the same team (minuses suicides and 'friendly kills')
===================================================================*/
/* Get A Team's Total Score */
int GetTeamScoreByTeamNumber(int Team)
{
	int TeamScore = 0;

	// add selected team's scores
	for(z = 0; z < MAX_PLAYERS; z++)
	{
		if(Team == TeamNumber[z])
			TeamScore += GetScoreStats(z);
	}

	return TeamScore;

}

/////////////////////////////////////////////////////////////////////////////////////////
// NEW FUNCTIONS
/////////////////////////////////////////////////////////////////////////////////////////

// + bonus + kills - suacides - friendly - deaths
int GetRealScore(int Player)
{
	return	GetScoreStats(Player) -		// + kills - suacides - friendly + bonus
			GetTotalDeaths(Player) +	// - deaths - suicides
			GetSuicides(Player);		// suicides was minused twice
}

// kills - suacides - friendly
int GetKills(int Player)
{
	int score = 0;
	int x = 0;

	// search all players
	for(x = 0; x < MAX_PLAYERS; x++)
	{
		// minus suicides and friendly kills
		if((Player==x) || ((TeamNumber[x] == TeamNumber[Player]) && TeamGame))
			score -= GetKillStats(Player,x);
		// add kills
		else
			score += GetKillStats(Player,x);
	}

	return score;
}

int GetFriendlyKills( int Player )
{
	int kills = 0;
	int x = 0;
	if(!TeamGame)return 0;
	for(x = 0; x < MAX_PLAYERS; x++)
	{
		if(Player==x) continue; // no suacides
		if(TeamNumber[x] == TeamNumber[Player])
			kills += GetKillStats(Player,x);
	}
	return kills;
}

int GetSuicides( int Player )
{
	return GetKillStats( Player, Player );
}

// positives points / negatives points
int GetEffeciency( int Player )
{
	// positive points == + kills - suacides - friendly
	float positives = (float)GetKills(Player);
	// negative points == + deaths + suicides + friendly
	float negatives = (float)(GetTotalDeaths(Player)+GetFriendlyKills(Player));
	// worst possible case
	if(!positives) return 0;
	// perfection
	if(!negatives) return 100;
	// calculate percentage
	return (int)(positives / (positives + negatives) * 100.0F);
}

int player_left( int player )
{
	return (GameStatus[player] == STATUS_Left || GameStatus[player] ==  STATUS_LeftCrashed);
}

int scoreable_status( int player )
{
	return ( GameStatus[player] == STATUS_Normal || GameStatus[player] == STATUS_ViewingScore || player_left(player) );
}
