/*
* dtEntity Game and Simulation Engine
*
* This library is free software; you can redistribute it and/or modify it under
* the terms of the GNU Lesser General Public License as published by the Free
* Software Foundation; either version 2.1 of the License, or (at your option)
* any later version.
*
* This library is distributed in the hope that it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
* FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
* details.
*
* You should have received a copy of the GNU Lesser General Public License
* along with this library; if not, write to the Free Software Foundation, Inc.,
* 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
*
* Martin Scheffler
*/

#include <dtEntity/layercomponent.h>

#include <dtEntity/layerattachpointcomponent.h>
#include <dtEntity/basemessages.h>
#include <dtEntity/entity.h>
#include <dtEntity/entitymanager.h>
#include <dtEntity/mapcomponent.h>
#include <dtEntity/nodemasks.h>
#include <osg/LineWidth>
#include <osg/PolygonMode>
#include <osg/ShapeDrawable>
#include <osg/PolygonOffset>
#include <osg/StateSet>
#include <osgDB/ReadFile>
#include <assert.h>
#include <sstream>

namespace dtEntity
{

   ////////////////////////////////////////////////////////////////////////////
   ////////////////////////////////////////////////////////////////////////////////

   const StringId LayerComponent::TYPE(SID("Layer"));
   const StringId LayerComponent::LayerId(SID("Layer"));
   const StringId LayerComponent::AttachedComponentId(SID("AttachedComponent"));
   const StringId LayerComponent::VisibleId(SID("Visible"));   
   
   ////////////////////////////////////////////////////////////////////////////
   LayerComponent::LayerComponent()
      : mLayerProperty(StringId())
      , mAttachedComponent(StringId())
      , mCurrentlyAttachedComponent(StringId())
      , mAttachPoint(StringId())
      , mCurrentlyVisible(true)
      , mAddedToScene(false)
      , mEntity(NULL)
   {
      Register(LayerId, &mLayerProperty);
      Register(AttachedComponentId, &mAttachedComponent);            
      Register(VisibleId, &mVisible);   
      mVisible.Set(true);
   }

   ////////////////////////////////////////////////////////////////////////////
   LayerComponent::~LayerComponent()
   {
   }

   ////////////////////////////////////////////////////////////////////////////
   StringId LayerComponent::GetLayer() const
   {
      return mLayerProperty.Get();
   }
    
   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::OnAddedToEntity(Entity &entity)
   {
      mEntity = &entity;
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::SetLayer(StringId layername)
   {
      if(mAttachPoint == layername)
      {
         return;
      }

      assert(mEntity != NULL);

      osg::Node* attachedNode = GetAttachedComponentNode();

      if(attachedNode != NULL && mAddedToScene)
      {
         LayerAttachPointSystem* layerattsystem;
         mEntity->GetEntityManager().GetEntitySystem(LayerAttachPointComponent::TYPE, layerattsystem);
               
         LayerAttachPointComponent* current;
         if(mAttachPoint != StringId() && layerattsystem->GetByName(mAttachPoint, current))
         {
            current->GetGroup()->removeChild(attachedNode);           
         }

         if(mVisible.Get())
         {
            LayerAttachPointComponent* next;
            if(layerattsystem->GetByName(layername, next))
            {
               next->GetAttachmentGroup()->addChild(attachedNode);
            }
         }
      }
      mAttachPoint = layername;
      mLayerProperty.Set(layername);
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::SetAttachedComponent(ComponentType handle)
   {
      mAttachedComponent.Set(handle);

      if(mAttachPoint == StringId()) 
      {
         mCurrentlyAttachedComponent = handle;
         return;
      }

      if(mCurrentlyAttachedComponent == handle)
      {
         return;
      }

      LayerAttachPointSystem* layerattsystem;
      mEntity->GetEntityManager().GetEntitySystem(LayerAttachPointComponent::TYPE, layerattsystem);
      
      LayerAttachPointComponent* current;
      if(!layerattsystem->GetByName(mAttachPoint, current))
      {
         std::ostringstream os;
         os << "Cannot detach node from scene graph. Attach point is ";
         os << GetStringFromSID(mAttachPoint);
         LOG_WARNING(os.str());
         return;
      }

      // remove old attachment
      if(mAddedToScene && mVisible.Get() && mAttachPoint != StringId())
      {
         // remove old attachment
         osg::Node* attachedNode = GetAttachedComponentNode();
         
         LayerAttachPointComponent* current;
         if(attachedNode != NULL && layerattsystem->GetByName(mAttachPoint, current))
         {
            current->GetGroup()->removeChild(attachedNode);           
         }
      }
      
      mCurrentlyAttachedComponent = handle;

      // add new attachment
      if(mVisible.Get() && mAddedToScene)
      {
         // remove old attachment
         osg::Node* attachedNode = GetAttachedComponentNode();
         if(attachedNode != NULL)
         {
            current->GetAttachmentGroup()->addChild(attachedNode);
         }
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   ComponentType LayerComponent::GetAttachedComponent() const
   {
      return mAttachedComponent.Get();
   }

   ////////////////////////////////////////////////////////////////////////////
   osg::Node* LayerComponent::GetAttachedComponentNode() const
   {
      assert(mEntity != NULL);
      if(mCurrentlyAttachedComponent == StringId()) return NULL;
      Component* ac;
      bool found = mEntity->GetComponent((ComponentType)mCurrentlyAttachedComponent, ac);
      if(!found)
      {
         LOG_DEBUG("LayerComponent: Attached component does not exist: " 
            + GetStringFromSID(mCurrentlyAttachedComponent));
         return NULL;
      }
      NodeComponent* nc = dynamic_cast<NodeComponent*>(ac);
      if(nc == NULL) return NULL;
      return nc->GetNode();
   }

   ////////////////////////////////////////////////////////////////////////////
   bool LayerComponent::IsVisible() const
   {
      return mVisible.Get();
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::SetVisible(bool visible)
   {
      if(mCurrentlyVisible == visible) return;

      if(mAttachPoint != StringId() && mCurrentlyAttachedComponent != StringId())
      {

         osg::Node* attachedNode = GetAttachedComponentNode();
         if(attachedNode != NULL)
         {
            LayerAttachPointSystem* layerattsystem;
            mEntity->GetEntityManager().GetEntitySystem(LayerAttachPointComponent::TYPE, layerattsystem);
            
            LayerAttachPointComponent* current;
            if(layerattsystem->GetByName(mAttachPoint, current))
            {
               if(visible)
               {
                  current->GetAttachmentGroup()->addChild(attachedNode);
               }
               else
               {
                  current->GetAttachmentGroup()->removeChild(attachedNode);
               }
            }
         }
      }
      mCurrentlyVisible = visible;
      VisibilityChangedMessage msg;
      msg.SetAboutEntityId(mEntity->GetId());
      msg.SetVisible(visible);
      mEntity->GetEntityManager().EmitMessage(msg);
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::OnPropertyChanged(StringId propname, Property& prop)
   {
      if(propname == VisibleId)
      {
         SetVisible(prop.BoolValue());
      }
      else if(propname == LayerId)
      {
         SetLayer(prop.StringIdValue());
      }
      else if(propname == AttachedComponentId)
      {
         SetAttachedComponent(prop.StringIdValue());
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::OnAddedToScene()
   {
      if(mAddedToScene) return;
      mAddedToScene = true;
      mAttachPoint = mLayerProperty.Get();
      LayerAttachPointComponent* current = NULL;
      if(mAttachPoint != StringId() && mEntity != NULL)
      {
         LayerAttachPointSystem* ls;
         mEntity->GetEntityManager().GetEntitySystem(LayerAttachPointComponent::TYPE, ls);
         ls->GetByName(mAttachPoint, current);
      }

      if(current != NULL && mAttachedComponent.Get() != StringId() && mVisible.Get())
      {  
         osg::Node* attachedNode = GetAttachedComponentNode();
#ifdef _DEBUG
         if(attachedNode == NULL)
         {
            std::ostringstream os;
            os << "LayerSystem: Cannot attach entity, node to attach is not selected!";
            os << " ComponentType: " << GetStringFromSID(mCurrentlyAttachedComponent);
            LOG_WARNING(os.str());
         }
         if(attachedNode != NULL && (attachedNode->getUserData() == NULL ||
             dynamic_cast<Entity*>(attachedNode->getUserData()) == NULL))
         {
            std::ostringstream os;
            os << "Attaching node with no user data to scene graph!";
            os << " ComponentType: " << GetStringFromSID(mCurrentlyAttachedComponent);
            LOG_ERROR(os.str());
         }
#endif

         if(attachedNode != NULL)
         {
            bool success = current->GetAttachmentGroup()->addChild(attachedNode);
            assert(success);
            Component* ac;
            if(mEntity->GetComponent((ComponentType)mAttachedComponent.Get(), ac))
            {
               static_cast<NodeComponent*>(ac)->SetParentComponent(LayerComponent::TYPE);
            }
         }
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerComponent::OnRemovedFromScene()
   {
      if(!mAddedToScene) return;

      if(mAttachPoint != StringId() && mCurrentlyAttachedComponent != StringId() && mVisible.Get())
      {
         osg::Node* attachedNode = GetAttachedComponentNode();
         LayerAttachPointSystem* ls;
         mEntity->GetEntityManager().GetEntitySystem(LayerAttachPointComponent::TYPE, ls);
         LayerAttachPointComponent* current = NULL;
         ls->GetByName(mAttachPoint, current);
         
         if(attachedNode != NULL && current != NULL)
         {
            bool success = current->GetGroup()->removeChild(attachedNode);
            if(!success)
            {
               LOG_WARNING("Could not remove attached node from scene graph!");
            }
         }
      }
      mAddedToScene = false;
   }


   ////////////////////////////////////////////////////////////////////////////
   ////////////////////////////////////////////////////////////////////////////

   // visitor for getting bounds of object relative to its own
   // coordinate system
   class LocalBoundingBoxVisitor : public osg::NodeVisitor
   {
   public:

      LocalBoundingBoxVisitor()
         : osg::NodeVisitor(osg::NodeVisitor::TRAVERSE_ALL_CHILDREN)
         , mVisited(false)
      {}

      /**
      * Visits the specified geode.
      *
      * @param node the geode to visit
      */
      virtual void apply(osg::Geode& node)
      {
         osg::NodePath p = getNodePath();
         osg::NodePath nodePath;
         osg::NodePath::iterator i = p.begin();
         ++i;
         for(;i != p.end(); ++i)
         {
            nodePath.push_back(*i);
         }

         osg::Matrix matrix = osg::computeLocalToWorld(nodePath);

         for (unsigned int i = 0; i < node.getNumDrawables(); ++i)
         {
            for (unsigned int j = 0; j < 8; ++j)
            {
               mBoundingBox.expandBy(node.getDrawable(i)->getBound().corner(j) * matrix);
               mVisited = true;
            }
         }
      }

      /**
      * The aggregate bounding box.
      */
      osg::BoundingBox mBoundingBox;
      bool mVisited;
   };

   ////////////////////////////////////////////////////////////////////////////
   ////////////////////////////////////////////////////////////////////////////

   LayerSystem::LayerSystem(EntityManager& em)
      : DefaultEntitySystem<LayerComponent>(em)
   {
      mEnterWorldFunctor = MessageFunctor(this, &LayerSystem::OnEnterWorld);
      em.RegisterForMessages(EntityAddedToSceneMessage::TYPE, mEnterWorldFunctor,"LayerSystem::OnEnterWorld");

      mLeaveWorldFunctor = MessageFunctor(this, &LayerSystem::OnLeaveWorld);
      em.RegisterForMessages(EntityRemovedFromSceneMessage::TYPE, mLeaveWorldFunctor, "LayerSystem::OnLeaveWorld");

      AddScriptedMethod("addVisibleBoundingBox", ScriptMethodFunctor(this, &LayerSystem::ScriptAddVisibleBoundingBox));
      AddScriptedMethod("removeVisibleBoundingBox", ScriptMethodFunctor(this, &LayerSystem::ScriptRemoveVisibleBoundingBox));
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerSystem::OnEnterWorld(const Message& msg)
   {
      EntityId id = (EntityId) msg.Get(EntityAddedToSceneMessage::AboutEntityId)->UIntValue();
      LayerComponent* comp;
      bool found = GetEntityManager().GetComponent(id, comp);
      if(found)
      {
         comp->OnAddedToScene();
      }
   }

   ////////////////////////////////////////////////////////////////////////////
   void LayerSystem::OnLeaveWorld(const Message& msg)
   {
      EntityId id = (EntityId) msg.Get(EntityAddedToSceneMessage::AboutEntityId)->UIntValue();
      LayerComponent* comp;
      bool found = GetEntityManager().GetComponent(id, comp);
      if(found)
      {
         comp->OnRemovedFromScene();
      }
   }

   ////////////////////////////////////////////////////////////////////////////////
   void LayerSystem::AddVisibleBoundingBox(dtEntity::EntityId id)
   {
      RemoveVisibleBoundingBox(id);

      if(mComponents.find(id) == mComponents.end())
      {
         return;
      }

      LayerComponent* lc = mComponents[id];

      dtEntity::TransformComponent* transform;
      if(!GetEntityManager().GetComponent(id, transform, true))
      {
         return;
      }

      osg::Node* node = lc->GetAttachedComponentNode();
      if(node == NULL)
      {
         return;
      }

      LocalBoundingBoxVisitor v;
      v.setTraversalMask(dtEntity::NodeMasks::PICKABLE);
      node->accept(v);
      if(!v.mVisited)
      {
         return;
      }
      osg::BoundingBox b = v.mBoundingBox;

      float radius = b.radius();
      if(radius == 0 )
      {
         return;
      }

      osg::Box* box = new osg::Box(b.center(), b.xMax() - b.xMin(), b.yMax() - b.yMin(), b.zMax() - b.zMin());
      osg::ShapeDrawable* drawable = new osg::ShapeDrawable(box);
      drawable->setColor(osg::Vec4(1,1,1,1));

      // linux crashes in generateDisplayList if this is turned on, no idea why
      drawable->setUseDisplayList(false);
      osg::Geode* geode = new osg::Geode();

      geode->addDrawable(drawable);
      geode->setName("SelectionBounds");
      geode->setNodeMask(dtEntity::NodeMasks::VISIBLE);

      // store pointer to entity to make box identifiable for removal
      dtEntity::Entity* entity;
      GetEntityManager().GetEntity(id, entity);

      geode->setUserData(entity);
      transform->GetGroup()->addChild(geode);

      // make box wireframe
      osg::StateSet* stateset = geode->getOrCreateStateSet();
      osg::PolygonOffset* polyoffset = new osg::PolygonOffset();
      polyoffset->setFactor(-1.0f);
      polyoffset->setUnits(-1.0f);
      osg::PolygonMode* polymode = new osg::PolygonMode();
      polymode->setMode(osg::PolygonMode::FRONT_AND_BACK,osg::PolygonMode::LINE);
      stateset->setAttributeAndModes(polyoffset, osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
      stateset->setAttributeAndModes(polymode, osg::StateAttribute::OVERRIDE|osg::StateAttribute::ON);
      osg::LineWidth* lw = new osg::LineWidth();
      lw->setWidth(1);
      stateset->setAttributeAndModes(lw, osg::StateAttribute::ON);
      stateset->setMode(GL_LIGHTING, osg::StateAttribute::OFF);
   }

   ////////////////////////////////////////////////////////////////////////////////
   void LayerSystem::RemoveVisibleBoundingBox(dtEntity::EntityId id)
   {
      dtEntity::TransformComponent* tcomp;
      if(!GetEntityManager().GetComponent(id, tcomp, true))
      {
         return;
      }
      osg::Group* grp = tcomp->GetNode()->asGroup();

      dtEntity::Entity* entity;
      if(!GetEntityManager().GetEntity(id, entity))
      {
         return;
      }

      for(unsigned int i = 0; i < grp->getNumChildren(); ++i)
      {
         osg::Node* child = grp->getChild(i);
         if(child->getName() == "SelectionBounds" && child->getUserData() == entity)
         {
            grp->removeChild(i, 1);
            return;
         }
      }


      dtEntity::LayerAttachPointSystem* ls;
      GetEntityManager().GetEntitySystem(dtEntity::LayerAttachPointComponent::TYPE, ls);
      grp = ls->GetDefaultLayer()->GetNode()->asGroup();

      for(unsigned int i = 0; i < grp->getNumChildren(); ++i)
      {
         osg::Node* child = grp->getChild(i);
         if(child->getName() == "SelectionBounds" && child->getUserData() == entity)
         {
            grp->removeChild(i, 1);
            return;
         }
      }
   }
}
