// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/common/accessibility_messages.h"

namespace content {

BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  return BrowserAccessibility::Create();
}

#if !defined(OS_MACOSX) && \
    !defined(OS_WIN) && \
    !defined(TOOLKIT_GTK)
// We have subclassess of BrowserAccessibilityManager on Mac, Linux/GTK,
// and Win. For any other platform, instantiate the base class.
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManager(src, delegate, factory);
}
#endif

BrowserAccessibilityManager::BrowserAccessibilityManager(
    const AccessibilityNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : delegate_(delegate),
      factory_(factory),
      root_(NULL),
      focus_(NULL),
      osk_state_(OSK_ALLOWED) {
  std::vector<AccessibilityNodeData> nodes;
  nodes.push_back(src);
  if (!UpdateNodes(nodes))
    return;
  if (!focus_)
    SetFocus(root_, false);
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  if (root_)
    root_->Destroy();
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() {
  return root_;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromRendererID(
    int32 renderer_id) {
  base::hash_map<int32, BrowserAccessibility*>::iterator iter =
      renderer_id_map_.find(renderer_id);
  if (iter != renderer_id_map_.end())
    return iter->second;
  return NULL;
}

void BrowserAccessibilityManager::GotFocus(bool touch_event_context) {
  if (!touch_event_context)
    osk_state_ = OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED;

  if (!focus_)
    return;

  NotifyAccessibilityEvent(AccessibilityNotificationFocusChanged, focus_);
}

void BrowserAccessibilityManager::WasHidden() {
  osk_state_ = OSK_DISALLOWED_BECAUSE_TAB_HIDDEN;
}

void BrowserAccessibilityManager::GotMouseDown() {
  osk_state_ = OSK_ALLOWED_WITHIN_FOCUSED_OBJECT;
  NotifyAccessibilityEvent(AccessibilityNotificationFocusChanged, focus_);
}

bool BrowserAccessibilityManager::IsOSKAllowed(const gfx::Rect& bounds) {
  if (!delegate_ || !delegate_->HasFocus())
    return false;

  gfx::Point touch_point = delegate_->GetLastTouchEventLocation();
  return bounds.Contains(touch_point);
}

void BrowserAccessibilityManager::RemoveNode(BrowserAccessibility* node) {
  if (node == focus_)
    SetFocus(root_, false);
  int renderer_id = node->renderer_id();
  renderer_id_map_.erase(renderer_id);
}

void BrowserAccessibilityManager::OnAccessibilityNotifications(
    const std::vector<AccessibilityHostMsg_NotificationParams>& params) {
  for (uint32 index = 0; index < params.size(); index++) {
    const AccessibilityHostMsg_NotificationParams& param = params[index];

    // Update nodes that changed.
    if (!UpdateNodes(param.nodes))
      return;

    // Find the node corresponding to the id that's the target of the
    // notification (which may not be the root of the update tree).
    BrowserAccessibility* node = GetFromRendererID(param.id);
    if (!node)
      continue;

    int notification_type = param.notification_type;
    if (notification_type == AccessibilityNotificationFocusChanged ||
        notification_type == AccessibilityNotificationBlur) {
      SetFocus(node, false);

      if (osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_HIDDEN &&
          osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED)
        osk_state_ = OSK_ALLOWED;

      // Don't send a native focus event if the window itself doesn't
      // have focus.
      if (delegate_ && !delegate_->HasFocus())
        continue;
    }

    // Send the notification event to the operating system.
    NotifyAccessibilityEvent(notification_type, node);

    // Set initial focus when a page is loaded.
    if (notification_type == AccessibilityNotificationLoadComplete) {
      if (!focus_)
        SetFocus(root_, false);
      if (!delegate_ || delegate_->HasFocus())
        NotifyAccessibilityEvent(AccessibilityNotificationFocusChanged, focus_);
    }
  }
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus(
    BrowserAccessibility* root) {
  if (focus_ && (!root || focus_->IsDescendantOf(root)))
    return focus_;

  return NULL;
}

void BrowserAccessibilityManager::SetFocus(
    BrowserAccessibility* node, bool notify) {
  if (focus_ != node)
    focus_ = node;

  if (notify && node && delegate_)
    delegate_->SetAccessibilityFocus(node->renderer_id());
}

void BrowserAccessibilityManager::DoDefaultAction(
    const BrowserAccessibility& node) {
  if (delegate_)
    delegate_->AccessibilityDoDefaultAction(node.renderer_id());
}

void BrowserAccessibilityManager::ScrollToMakeVisible(
    const BrowserAccessibility& node, gfx::Rect subfocus) {
  if (delegate_) {
    delegate_->AccessibilityScrollToMakeVisible(node.renderer_id(), subfocus);
  }
}

void BrowserAccessibilityManager::ScrollToPoint(
    const BrowserAccessibility& node, gfx::Point point) {
  if (delegate_) {
    delegate_->AccessibilityScrollToPoint(node.renderer_id(), point);
  }
}

void BrowserAccessibilityManager::SetTextSelection(
    const BrowserAccessibility& node, int start_offset, int end_offset) {
  if (delegate_) {
    delegate_->AccessibilitySetTextSelection(
        node.renderer_id(), start_offset, end_offset);
  }
}

gfx::Rect BrowserAccessibilityManager::GetViewBounds() {
  if (delegate_)
    return delegate_->GetViewBounds();
  return gfx::Rect();
}

void BrowserAccessibilityManager::UpdateNodesForTesting(
    const AccessibilityNodeData& node1,
    const AccessibilityNodeData& node2 /* = AccessibilityNodeData() */,
    const AccessibilityNodeData& node3 /* = AccessibilityNodeData() */,
    const AccessibilityNodeData& node4 /* = AccessibilityNodeData() */,
    const AccessibilityNodeData& node5 /* = AccessibilityNodeData() */,
    const AccessibilityNodeData& node6 /* = AccessibilityNodeData() */,
    const AccessibilityNodeData& node7 /* = AccessibilityNodeData() */) {
  std::vector<AccessibilityNodeData> nodes;
  nodes.push_back(node1);
  if (node2.id != AccessibilityNodeData().id)
    nodes.push_back(node2);
  if (node3.id != AccessibilityNodeData().id)
    nodes.push_back(node3);
  if (node4.id != AccessibilityNodeData().id)
    nodes.push_back(node4);
  if (node5.id != AccessibilityNodeData().id)
    nodes.push_back(node5);
  if (node6.id != AccessibilityNodeData().id)
    nodes.push_back(node6);
  if (node7.id != AccessibilityNodeData().id)
    nodes.push_back(node7);
  UpdateNodes(nodes);
}

bool BrowserAccessibilityManager::UpdateNodes(
    const std::vector<AccessibilityNodeData>& nodes) {
  bool success = true;

  // First, update all of the nodes in the tree.
  for (size_t i = 0; i < nodes.size() && success; i++) {
    if (!UpdateNode(nodes[i]))
      success = false;
  }

  // In a second pass, call PostInitialize on each one - this must
  // be called after all of each node's children are initialized too.
  for (size_t i = 0; i < nodes.size() && success; i++) {
    // Note: it's not a bug for nodes[i].id to not be found in the tree.
    // Consider this example:
    // Before:
    // A
    //   B
    //     C
    //   D
    //     E
    //       F
    // After:
    // A
    //   B
    //     C
    //       F
    //   D
    // In this example, F is being reparented. The renderer scans the tree
    // in order. If can't update "C" to add "F" as a child, when "F" is still
    // a child of "E". So it first updates "E", to remove "F" as a child.
    // Later, it ends up deleting "E". So when we get here, "E" was updated as
    // part of this sequence but it no longer exists in the final tree, so
    // there's nothing to postinitialize.
    BrowserAccessibility* instance = GetFromRendererID(nodes[i].id);
    if (instance)
      instance->PostInitialize();
  }

  if (!success) {
    // A bad accessibility tree could lead to memory corruption.
    // Ask the delegate to crash the renderer, or if not available,
    // crash the browser.
    if (delegate_)
      delegate_->FatalAccessibilityTreeError();
    else
      CHECK(false);
  }

  return success;
}

BrowserAccessibility* BrowserAccessibilityManager::CreateNode(
    BrowserAccessibility* parent,
    int32 renderer_id,
    int32 index_in_parent) {
  BrowserAccessibility* node = factory_->Create();
  node->InitializeTreeStructure(
      this, parent, renderer_id, index_in_parent);
  AddNodeToMap(node);
  return node;
}

void BrowserAccessibilityManager::AddNodeToMap(BrowserAccessibility* node) {
  renderer_id_map_[node->renderer_id()] = node;
}

bool BrowserAccessibilityManager::UpdateNode(const AccessibilityNodeData& src) {
  // This method updates one node in the tree based on serialized data
  // received from the renderer.

  // Create a set of child ids in |src| for fast lookup. If a duplicate id is
  // found, exit now with a fatal error before changing anything else.
  std::set<int32> new_child_ids;
  for (size_t i = 0; i < src.child_ids.size(); ++i) {
    if (new_child_ids.find(src.child_ids[i]) != new_child_ids.end())
      return false;
    new_child_ids.insert(src.child_ids[i]);
  }

  // Look up the node by id. If it's not found, then either the root
  // of the tree is being swapped, or we're out of sync with the renderer
  // and this is a serious error.
  BrowserAccessibility* instance = GetFromRendererID(src.id);
  if (!instance) {
    if (src.role != AccessibilityNodeData::ROLE_ROOT_WEB_AREA)
      return false;
    instance = CreateNode(NULL, src.id, 0);
  }

  if (src.bool_attributes.find(
          AccessibilityNodeData::ATTR_UPDATE_LOCATION_ONLY) !=
              src.bool_attributes.end()) {
    instance->SetLocation(src.location);
    return true;
  }

  // Update all of the node-specific data, like its role, state, name, etc.
  instance->InitializeData(src);

  //
  // Update the children in three steps:
  //
  // 1. Iterate over the old children and delete nodes that are no longer
  //    in the tree.
  // 2. Build up a vector of new children, reusing children that haven't
  //    changed (but may have been reordered) and adding new empty
  //    objects for new children.
  // 3. Swap in the new children vector for the old one.

  // Delete any previous children of this instance that are no longer
  // children first. We make a deletion-only pass first to prevent a
  // node that's being reparented from being the child of both its old
  // parent and new parent, which could lead to a double-free.
  // If a node is reparented, the renderer will always send us a fresh
  // copy of the node.
  const std::vector<BrowserAccessibility*>& old_children = instance->children();
  for (size_t i = 0; i < old_children.size(); ++i) {
    int old_id = old_children[i]->renderer_id();
    if (new_child_ids.find(old_id) == new_child_ids.end())
      old_children[i]->Destroy();
  }

  // Now build a vector of new children, reusing objects that were already
  // children of this node before.
  std::vector<BrowserAccessibility*> new_children;
  bool success = true;
  for (size_t i = 0; i < src.child_ids.size(); i++) {
    int32 child_renderer_id = src.child_ids[i];
    int32 index_in_parent = static_cast<int32>(i);
    BrowserAccessibility* child = GetFromRendererID(child_renderer_id);
    if (child) {
      if (child->parent() != instance) {
        // This is a serious error - nodes should never be reparented.
        // If this case occurs, continue so this node isn't left in an
        // inconsistent state, but return failure at the end.
        success = false;
        continue;
      }
      child->UpdateParent(instance, index_in_parent);
    } else {
      child = CreateNode(instance, child_renderer_id, index_in_parent);
    }
    new_children.push_back(child);
  }

  // Finally, swap in the new children vector for the old.
  instance->SwapChildren(new_children);

  // Handle the case where this node is the new root of the tree.
  if (src.role == AccessibilityNodeData::ROLE_ROOT_WEB_AREA &&
      (!root_ || root_->renderer_id() != src.id)) {
    if (root_)
      root_->Destroy();
    if (focus_ == root_)
      focus_ = instance;
    root_ = instance;
  }

  // Keep track of what node is focused.
  if ((src.state >> AccessibilityNodeData::STATE_FOCUSED) & 1)
    SetFocus(instance, false);

  return success;
}

}  // namespace content
