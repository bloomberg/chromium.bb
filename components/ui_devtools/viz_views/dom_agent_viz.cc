// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/viz_views/dom_agent_viz.h"

#include "base/stl_util.h"
#include "components/ui_devtools/root_element.h"
#include "components/ui_devtools/ui_element.h"
#include "components/ui_devtools/viz_views/frame_sink_element.h"
#include "components/viz/service/frame_sinks/compositor_frame_sink_support.h"
#include "components/viz/service/frame_sinks/frame_sink_manager_impl.h"

namespace ui_devtools {

// Updating logic for FrameSinks:
//
// 1. Creating. We register a FrameSinkId, create a CompositorFrameSink and if a
// CompositorFrameSink is not a root we register hierarchy from the parent of
// this FrameSink to this CompositorFrameSink. When we register a FrameSinkId,
// we check if its corresponding element is already in the tree. If not, we
// attach it to the RootElement which serves as the root of the
// CompositorFrameSink tree. In this state the CompositorFrameSink is considered
// unembedded and it is a sibling of RootCompositorFrameSinks. If it is present
// in a tree we just change the properties (|has_created_frame_sink_|).
// These events don't know anything about the hierarchy
// so we don't change it. When we get OnRegisteredHierarchy from parent to child
// the corresponding elements must already be present in a tree. The usual state
// is: child is attached to RootElement and now we will detach it from the
// RootElement and attach to the real parent. During handling this event we
// actually delete the subtree of RootElement rooted from child and create a new
// subtree of parent. This potentially could be more efficient, but if we just
// switch necessary pointers we must send a notification to the frontend so it
// can update the UI. Unfortunately, this action involves deleting a node from
// the backend list of UI Elements (even when it is still alive) and trying to
// delete it once again (for instance, when we close a corresponding tab) causes
// crash.
//
// 2. Deleting. We unregister hierarchy, destroy a CompositorFrameSink and
// invalidate a FrameSinkId. When we invalidate an FrameSinkId or destroy a
// FrameSink we check if it's the last action that has to happen with the
// corresponding element. For example, if the element has
// |has_created_frame_sink_| = true and we get a |OnDestroyedFrameSink| event we
// just set |has_created_frame_sink_| = false, but don't remove it from a tree,
// because its FrameSinkId is still registered, so it's not completely dead. But
// when after that we get |OnInvalidatedFrameSinkId| we can remove the
// node from the tree. When we get OnUnregisteredHierarchy we assume the nodes
// are still present in a tree, so we do the same work as we did in registering
// case. Only here we move a subtree of parent rooted from child to the
// RootElement. Obviously, now the child will be in detached state.

using namespace ui_devtools::protocol;

DOMAgentViz::DOMAgentViz(viz::FrameSinkManagerImpl* frame_sink_manager)
    : frame_sink_manager_(frame_sink_manager) {}

DOMAgentViz::~DOMAgentViz() {
  Clear();
}

void DOMAgentViz::OnRegisteredFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  // If a FrameSink was just registered we don't know anything about
  // hierarchy. So we should attach it to the RootElement.
  element_root()->AddChild(
      CreateFrameSinkElement(frame_sink_id, element_root(), /*is_root=*/false,
                             /*has_created_frame_sink=*/false));
}

void DOMAgentViz::OnInvalidatedFrameSinkId(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // A FrameSinkElement with |frame_sink_id| can only be invalidated after
  // being destroyed.
  DCHECK(!it->second->has_created_frame_sink());
  DestroyElementAndRemoveSubtree(it->second.get());
}

void DOMAgentViz::OnCreatedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id,
    bool is_root) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());
  // The corresponding element is already present in a tree, so we
  // should update its |has_created_frame_sink_| and |is_root_| properties.
  it->second->SetHasCreatedFrameSink(true);
  it->second->SetRoot(is_root);
}

void DOMAgentViz::OnDestroyedCompositorFrameSink(
    const viz::FrameSinkId& frame_sink_id) {
  auto it = frame_sink_elements_.find(frame_sink_id);
  DCHECK(it != frame_sink_elements_.end());

  // Set FrameSinkElement to not connected to mark it as destroyed.
  it->second->SetHasCreatedFrameSink(false);
}

void DOMAgentViz::OnRegisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the new
  // parent.
  auto it_parent = frame_sink_elements_.find(parent_frame_sink_id);
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_parent != frame_sink_elements_.end());
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second.get();
  FrameSinkElement* new_parent = it_parent->second.get();

  // TODO: Add support for |child| to have multiple parents.
  Reparent(new_parent, child);
}

void DOMAgentViz::OnUnregisteredFrameSinkHierarchy(
    const viz::FrameSinkId& parent_frame_sink_id,
    const viz::FrameSinkId& child_frame_sink_id) {
  // At this point these elements must be present in a tree.
  // We should detach a child from its current parent and attach to the
  // RootElement since it wasn't destroyed yet.
  auto it_child = frame_sink_elements_.find(child_frame_sink_id);
  DCHECK(it_child != frame_sink_elements_.end());

  FrameSinkElement* child = it_child->second.get();

  // TODO: Add support for |child| to have multiple parents: only adds |child|
  // to RootElement if all parents of |child| are unregistered.
  Reparent(element_root(), child);
}

std::unique_ptr<DOM::Node> DOMAgentViz::BuildTreeForFrameSink(
    UIElement* parent_element,
    const viz::FrameSinkId& parent_id) {
  std::unique_ptr<Array<DOM::Node>> children = Array<DOM::Node>::create();

  // Once the FrameSinkElement is created it calls this function to build its
  // subtree. So we iterate through |parent_element|'s children and
  // recursively build the subtree for them.
  for (auto& child_id : frame_sink_manager_->GetChildrenByParent(parent_id)) {
    bool has_created_frame_sink =
        !!frame_sink_manager_->GetFrameSinkForId(child_id);

    FrameSinkElement* child_element = CreateFrameSinkElement(
        child_id, parent_element, /*is_root=*/false, has_created_frame_sink);

    children->addItem(BuildTreeForFrameSink(child_element, child_id));
    parent_element->AddChild(child_element);
  }

  std::unique_ptr<DOM::Node> node =
      BuildNode("FrameSink", parent_element->GetAttributes(),
                std::move(children), parent_element->node_id());
  return node;
}

protocol::Response DOMAgentViz::enable() {
  frame_sink_manager_->AddObserver(this);
  return protocol::Response::OK();
}

protocol::Response DOMAgentViz::disable() {
  frame_sink_manager_->RemoveObserver(this);
  Clear();
  return DOMAgent::disable();
}

std::vector<UIElement*> DOMAgentViz::CreateChildrenForRoot() {
  std::vector<UIElement*> children;

  // All of the FrameSinkElements and SurfaceElements are owned here, so make
  // sure the root element doesn't delete our pointers.
  element_root()->set_owns_children(false);

  // Find all elements that are not part of any hierarchy. This will be
  // FrameSinks that are either root, or detached.
  std::vector<viz::FrameSinkId> registered_frame_sinks =
      frame_sink_manager_->GetRegisteredFrameSinkIds();
  base::flat_set<viz::FrameSinkId> detached_frame_sinks(registered_frame_sinks);

  for (auto& frame_sink_id : registered_frame_sinks) {
    for (auto& child_id :
         frame_sink_manager_->GetChildrenByParent(frame_sink_id)) {
      detached_frame_sinks.erase(child_id);
    }
  }

  // Add created RootFrameSinks and detached FrameSinks.
  for (auto& frame_sink_id : detached_frame_sinks) {
    const viz::CompositorFrameSinkSupport* support =
        frame_sink_manager_->GetFrameSinkForId(frame_sink_id);
    bool is_root = support && support->is_root();
    bool has_created_frame_sink = !!support;
    children.push_back(CreateFrameSinkElement(frame_sink_id, element_root(),
                                              is_root, has_created_frame_sink));
  }

  return children;
}

std::unique_ptr<DOM::Node> DOMAgentViz::BuildTreeForUIElement(
    UIElement* ui_element) {
  if (ui_element->type() == UIElementType::FRAMESINK) {
    return BuildTreeForFrameSink(ui_element,
                                 FrameSinkElement::From(ui_element));
  }
  return nullptr;
}

void DOMAgentViz::Clear() {
  frame_sink_elements_.clear();
}

void DOMAgentViz::DestroyElementAndRemoveSubtree(UIElement* element) {
  // We may come across the case where we've got the event to delete the
  // FrameSink, but we haven't got events to delete its children. We should
  // detach all its children and attach them to RootElement and then delete the
  // node we were asked for.
  for (auto* child : element->children())
    Reparent(element_root(), child);

  element->parent()->RemoveChild(element);
  DestroyElement(element);
}

void DOMAgentViz::Reparent(UIElement* new_parent, UIElement* child) {
  DestroySubtree(child);

  // This removes the child element from the Node map. It has to be added with
  // null parent to recreate the entry.
  child->parent()->RemoveChild(child);
  OnUIElementAdded(nullptr, child);
  new_parent->AddChild(child);
  child->set_parent(new_parent);
}

void DOMAgentViz::DestroyElement(UIElement* element) {
  if (element->type() == UIElementType::FRAMESINK) {
    frame_sink_elements_.erase(FrameSinkElement::From(element));
  } else {
    NOTREACHED();
  }
}

void DOMAgentViz::DestroySubtree(UIElement* element) {
  for (auto* child : element->children()) {
    DestroySubtree(child);
    DestroyElement(child);
  }
  element->ClearChildren();
}

FrameSinkElement* DOMAgentViz::CreateFrameSinkElement(
    const viz::FrameSinkId& frame_sink_id,
    UIElement* parent,
    bool is_root,
    bool is_client_connected) {
  DCHECK(!base::ContainsKey(frame_sink_elements_, frame_sink_id));
  frame_sink_elements_[frame_sink_id] = std::make_unique<FrameSinkElement>(
      frame_sink_id, frame_sink_manager_, this, parent, is_root,
      is_client_connected);
  return frame_sink_elements_[frame_sink_id].get();
}

}  // namespace ui_devtools
