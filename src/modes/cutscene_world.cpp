//  SuperTuxKart - a fun racing game with go-kart
//  Copyright (C) 2006 SuperTuxKart-Team
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 3
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

#include "modes/cutscene_world.hpp"

#include <string>
#include <IMeshSceneNode.h>
#include <ISceneManager.h>

#include "animations/animation_base.hpp"
#include "audio/music_manager.hpp"
#include "graphics/irr_driver.hpp"
#include "io/file_manager.hpp"
#include "karts/abstract_kart.hpp"
#include "karts/kart_model.hpp"
#include "karts/kart_properties.hpp"
#include "modes/overworld.hpp"
#include "physics/physics.hpp"
#include "states_screens/main_menu_screen.hpp"
#include "states_screens/race_gui_base.hpp"
#include "tracks/track.hpp"
#include "tracks/track_object.hpp"
#include "tracks/track_object_manager.hpp"
#include "utils/constants.hpp"
#include "utils/ptr_vector.hpp"

//-----------------------------------------------------------------------------
/** Constructor. Sets up the clock mode etc.
 */
CutsceneWorld::CutsceneWorld() : World()
{
    WorldStatus::setClockMode(CLOCK_NONE);
    m_use_highscores = false;
    m_play_racestart_sounds = false;
}   // CutsceneWorld

//-----------------------------------------------------------------------------
/** Initialises the three strikes battle. It sets up the data structure
 *  to keep track of points etc. for each kart.
 */
void CutsceneWorld::init()
{
    World::init();
    
    m_duration = -1.0f;
    
    //const btTransform &s = getTrack()->getStartTransform(0);
    //const Vec3 &v = s.getOrigin();
    m_camera = irr_driver->getSceneManager()
             ->addCameraSceneNode(NULL, core::vector3df(0.0f, 0.0f, 0.0f),
                                  core::vector3df(0.0f, 0.0f, 0.0f));
    m_camera->setFOV(0.61f);
    m_camera->bindTargetAndRotation(true); // no "look-at"
    
    // --- Build list of sounds to play at certain frames
    PtrVector<TrackObject>& objects = m_track->getTrackObjectManager()->getObjects();
    TrackObject* curr;
    for_in(curr, objects)
    {
        if (curr->getType() == "sfx-emitter" && !curr->getTriggerCondition().empty())
        {
            const std::string& condition = curr->getTriggerCondition();
            
            if (StringUtils::startsWith(condition, "frame "))
            {
                std::string frameStr = condition.substr(6); // remove 'frame ' prefix
                int frame;
                
                if (!StringUtils::fromString(frameStr, frame))
                {
                    fprintf(stderr, "[CutsceneWorld] Invalid condition '%s'\n", condition.c_str());
                    continue;
                }
                
                float FPS = 25.0f; // for now we assume the cutscene is saved at 25 FPS
                m_sounds_to_trigger[frame / FPS].push_back(curr);
            }
        }
        
        if (dynamic_cast<AnimationBase*>(curr) != NULL)
        {
            m_duration = std::max(m_duration, dynamic_cast<AnimationBase*>(curr)->getAnimationDuration());
        }
    }
    
    if (m_duration <= 0.0f)
    {
        fprintf(stderr, "[CutsceneWorld] WARNING: cutscene has no duration\n");
    }
    
}   // CutsceneWorld

//-----------------------------------------------------------------------------
/** Destructor. Clears all internal data structures, and removes the tire mesh
 *  from the mesh cache.
 */
CutsceneWorld::~CutsceneWorld()
{
}   // ~CutsceneWorld

//-----------------------------------------------------------------------------
/** Called when a kart is hit. 
 *  \param kart_id The world kart id of the kart that was hit.
 */
void CutsceneWorld::kartHit(const int kart_id)
{
}

//-----------------------------------------------------------------------------
/** Returns the internal identifier for this race.
 */
const std::string& CutsceneWorld::getIdent() const
{
    return IDENT_CUSTSCENE;
}   // getIdent

//-----------------------------------------------------------------------------
/** Update the world and the track.
 *  \param dt Time step size. 
 */
void CutsceneWorld::update(float dt)
{
    m_time += dt;
    
    /*
    if (m_time > m_duration)
    {
        printf("DONE!\n");
        
        race_manager->exitRace();
        StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());
        //OverWorld::enterOverWorld();
        return;
    }
    */
    
    World::update(dt);
    World::updateTrack(dt);
    
    PtrVector<TrackObject>& objects = m_track->getTrackObjectManager()->getObjects();
    TrackObject* curr;
    for_in(curr, objects)
    {
        if (curr->getType() == "cutscene_camera")
        {
            m_camera->setPosition(curr->getNode()->getPosition());
            
            core::vector3df rot = curr->getNode()->getRotation();
            Vec3 rot2(rot);
            rot2.setPitch(rot2.getPitch() + 90.0f);
            m_camera->setRotation(rot2.toIrrVector());
            //printf("Camera %f %f %f\n", curr->getNode()->getPosition().X, curr->getNode()->getPosition().Y, curr->getNode()->getPosition().Z);
        }
    }
    
    for (std::map<float, std::vector<TrackObject*> >::iterator it = m_sounds_to_trigger.begin();
         it != m_sounds_to_trigger.end(); )
    {
        if (m_time >= it->first)
        {
            std::vector<TrackObject*> objects = it->second;
            for (unsigned int i = 0; i < objects.size(); i++)
            {
                objects[i]->triggerSound();
            }
            m_sounds_to_trigger.erase(it++);
        }
        else
        {
            it++;
        }
     }
}   // update

//-----------------------------------------------------------------------------

void CutsceneWorld::enterRaceOverState()
{
    race_manager->exitRace();
    StateManager::get()->resetAndGoToScreen(MainMenuScreen::getInstance());
    OverWorld::enterOverWorld();
}

//-----------------------------------------------------------------------------
/** The battle is over if only one kart is left, or no player kart.
 */
bool CutsceneWorld::isRaceOver()
{
    return m_time > m_duration;
}   // isRaceOver

//-----------------------------------------------------------------------------
/** Called when the race finishes, i.e. after playing (if necessary) an
 *  end of race animation. It updates the time for all karts still racing,
 *  and then updates the ranks.
 */
void CutsceneWorld::terminateRace()
{
    World::terminateRace();
}   // terminateRace

//-----------------------------------------------------------------------------
/** Called then a battle is restarted.
 */
void CutsceneWorld::restartRace()
{
    World::restartRace();
}   // restartRace

//-----------------------------------------------------------------------------
/** Returns the data to display in the race gui.
 */
RaceGUIBase::KartIconDisplayInfo* CutsceneWorld::getKartsDisplayInfo()
{
    return NULL;
}   // getKartDisplayInfo

//-----------------------------------------------------------------------------
/** Moves a kart to its rescue position.
 *  \param kart The kart that was rescued.
 */
void CutsceneWorld::moveKartAfterRescue(AbstractKart* kart)
{
}   // moveKartAfterRescue