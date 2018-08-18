// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UI_DEVTOOLS_VIZ_VIEWS_DOM_AGENT_VIZ_H_
#define COMPONENTS_UI_DEVTOOLS_VIZ_VIEWS_DOM_AGENT_VIZ_H_

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "components/ui_devtools/DOM.h"
#include "components/ui_devtools/dom_agent.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "components/viz/service/frame_sinks/frame_sink_observer.h"

namespace viz {
class FrameSinkManagerImpl;
}

namespace ui_devtools {
class FrameSinkElement;

class DOMAgentViz : public viz::FrameSinkObserver, public DOMAgent {
 public:
  explicit DOMAgentViz(viz::FrameSinkManagerImpl* frame_sink_manager);
  ~DOMAgentViz() override;

  // viz::FrameSinkObserver:
  void OnRegisteredFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void OnInvalidatedFrameSinkId(const viz::FrameSinkId& frame_sink_id) override;
  void OnCreatedCompositorFrameSink(const viz::FrameSinkId& frame_sink_id,
                                    bool is_root) override;
  void OnDestroyedCompositorFrameSink(
      const viz::FrameSinkId& frame_sink_id) override;
  void OnRegisteredFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;
  void OnUnregisteredFrameSinkHierarchy(
      const viz::FrameSinkId& parent_frame_sink_id,
      const viz::FrameSinkId& child_frame_sink_id) override;

 private:
  std::unique_ptr<protocol::DOM::Node> BuildTreeForFrameSink(
      FrameSinkElement* frame_sink_element,
      const viz::FrameSinkId& frame_sink_id);

  // DOM::Backend:
  protocol::Response enable() override;
  protocol::Response disable() override;

  // DOMAgent:
  std::vector<UIElement*> CreateChildrenForRoot() override;
  std::unique_ptr<protocol::DOM::Node> BuildTreeForUIElement(
      UIElement* ui_element) override;

  // Removes the |child| subtree from the |parent| subtree and destroys every
  // element in the |child| subtree.
  void DestroyChildSubtree(UIElement* parent, UIElement* child);

  // Every time the frontend disconnects we don't destroy DOMAgent so once we
  // establish the connection again we need to clear the FrameSinkId sets
  // because they may carry obsolete data. Then we initialize these with alive
  // FrameSinkIds. Clears the sets of FrameSinkIds that correspond to created
  // FrameSinks, registered FrameSinkIds and those that have corresponding
  // FrameSinkElements created.
  void Clear();
  // Initializes the sets of FrameSinkIds that correspond to registered
  // FrameSinkIds and created FrameSinks.
  void InitFrameSinkSets();

  // Mark a FrameSink that has |frame_sink_id| and all its subtree as attached.
  void SetAttachedFrameSink(const viz::FrameSinkId& frame_sink_id);

  // We delete the FrameSinkElements in the subtree rooted at |root| from
  // |frame_sink_elements_| and attach all its children to the root_element().
  void RemoveFrameSinkSubtree(UIElement* root);

  // Remove FrameSinkElements for subtree rooted at |element| from the tree
  // |frame_sink_elements_|.
  void RemoveFrameSinkElement(UIElement* element);

  // These sets are used to create and update the DOM tree. We add/remove
  // registered FrameSinks to |registered_frame_sink_ids_to_is_connected_| when
  // FrameSink is registered/invalidated and initialize the bool to false to
  // indicate that the FrameSink is not created yet. Then when a |frame_sink| is
  // created/destroyed, we update |registered_frame_sink_ids_to_is_connected_|
  // of |frame_sink| true or false accordingly. When we get events
  // registered/unregistered hierarchy we don't change these sets because we
  // detach a subtree from one node and attach it to another node and the list
  // of registered/created FrameSinkIds doesn't change.
  // TODO(yiyix): Removes this map because it saves duplicated information as
  // |frame_sink_elements_|.
  base::flat_map<viz::FrameSinkId, bool>
      registered_frame_sink_ids_to_is_connected_;

  // This is used to track created FrameSinkElements in a FrameSink tree. Every
  // time we register/invalidate a FrameSinkId, create/destroy a FrameSink,
  // register/unregister hierarchy we change this set, because these actions
  // involve deleting and adding elements.
  base::flat_map<viz::FrameSinkId, FrameSinkElement*> frame_sink_elements_;

  // This is used to denote attached FrameSinks.
  base::flat_set<viz::FrameSinkId> attached_frame_sinks_;

  viz::FrameSinkManagerImpl* frame_sink_manager_;

  DISALLOW_COPY_AND_ASSIGN(DOMAgentViz);
};

}  // namespace ui_devtools

#endif  // COMPONENTS_UI_DEVTOOLS_VIZ_VIEWS_DOM_AGENT_VIZ_H_
