// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include "base/logging.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/accessibility_messages.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace content {

ui::AXTreeUpdate MakeAXTreeUpdate(
    const ui::AXNodeData& node1,
    const ui::AXNodeData& node2 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node3 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node4 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node5 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node6 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node7 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node8 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node9 /* = ui::AXNodeData() */) {
  CR_DEFINE_STATIC_LOCAL(ui::AXNodeData, empty_data, ());
  int32 no_id = empty_data.id;

  ui::AXTreeUpdate update;
  update.nodes.push_back(node1);
  if (node2.id != no_id)
    update.nodes.push_back(node2);
  if (node3.id != no_id)
    update.nodes.push_back(node3);
  if (node4.id != no_id)
    update.nodes.push_back(node4);
  if (node5.id != no_id)
    update.nodes.push_back(node5);
  if (node6.id != no_id)
    update.nodes.push_back(node6);
  if (node7.id != no_id)
    update.nodes.push_back(node7);
  if (node8.id != no_id)
    update.nodes.push_back(node8);
  if (node9.id != no_id)
    update.nodes.push_back(node9);
  return update;
}

BrowserAccessibility* BrowserAccessibilityFactory::Create() {
  return BrowserAccessibility::Create();
}

BrowserAccessibilityFindInPageInfo::BrowserAccessibilityFindInPageInfo()
    : request_id(-1),
      match_index(-1),
      start_id(-1),
      start_offset(0),
      end_id(-1),
      end_offset(-1),
      active_request_id(-1) {}

#if !defined(OS_MACOSX) && \
    !defined(OS_WIN) && \
    !defined(OS_ANDROID) \
// We have subclassess of BrowserAccessibilityManager on Mac, and Win. For any
// other platform, instantiate the base class.
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManager(initial_tree, delegate, factory);
}
#endif

BrowserAccessibilityManager::BrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : delegate_(delegate),
      factory_(factory),
      tree_(new ui::AXSerializableTree()),
      focus_(NULL),
      user_is_navigating_away_(false),
      osk_state_(OSK_ALLOWED) {
  tree_->SetDelegate(this);
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : delegate_(delegate),
      factory_(factory),
      tree_(new ui::AXSerializableTree()),
      focus_(NULL),
      user_is_navigating_away_(false),
      osk_state_(OSK_ALLOWED) {
  tree_->SetDelegate(this);
  Initialize(initial_tree);
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  tree_.reset(NULL);
}

void BrowserAccessibilityManager::Initialize(
    const ui::AXTreeUpdate& initial_tree) {
  if (!tree_->Unserialize(initial_tree)) {
    if (delegate_) {
      LOG(ERROR) << tree_->error();
      delegate_->AccessibilityFatalError();
    } else {
      LOG(FATAL) << tree_->error();
    }
  }

  if (!focus_)
    SetFocus(tree_->root(), false);
}

// static
ui::AXTreeUpdate BrowserAccessibilityManager::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() {
  return GetFromAXNode(tree_->root());
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromAXNode(
    ui::AXNode* node) {
  return GetFromID(node->id());
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromID(int32 id) {
  base::hash_map<int32, BrowserAccessibility*>::iterator iter =
      id_wrapper_map_.find(id);
  if (iter != id_wrapper_map_.end())
    return iter->second;
  return NULL;
}

void BrowserAccessibilityManager::OnWindowFocused() {
  if (focus_)
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetFromAXNode(focus_));
}

void BrowserAccessibilityManager::OnWindowBlurred() {
  if (focus_)
    NotifyAccessibilityEvent(ui::AX_EVENT_BLUR, GetFromAXNode(focus_));
}

void BrowserAccessibilityManager::UserIsNavigatingAway() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::UserIsReloading() {
  user_is_navigating_away_ = true;
}

void BrowserAccessibilityManager::NavigationSucceeded() {
  user_is_navigating_away_ = false;
}

void BrowserAccessibilityManager::NavigationFailed() {
  user_is_navigating_away_ = false;
}

void BrowserAccessibilityManager::GotMouseDown() {
  osk_state_ = OSK_ALLOWED_WITHIN_FOCUSED_OBJECT;
  NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetFromAXNode(focus_));
}

bool BrowserAccessibilityManager::UseRootScrollOffsetsWhenComputingBounds() {
  return true;
}

void BrowserAccessibilityManager::OnAccessibilityEvents(
    const std::vector<AccessibilityHostMsg_EventParams>& params) {
  bool should_send_initial_focus = false;

  // Process all changes to the accessibility tree first.
  for (uint32 index = 0; index < params.size(); index++) {
    const AccessibilityHostMsg_EventParams& param = params[index];
    if (!tree_->Unserialize(param.update)) {
      if (delegate_) {
        LOG(ERROR) << tree_->error();
        delegate_->AccessibilityFatalError();
      } else {
        CHECK(false) << tree_->error();
      }
      return;
    }

    // Set focus to the root if it's not anywhere else.
    if (!focus_) {
      SetFocus(tree_->root(), false);
      should_send_initial_focus = true;
    }
  }

  if (should_send_initial_focus &&
      (!delegate_ || delegate_->AccessibilityViewHasFocus())) {
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetFromAXNode(focus_));
  }

  // Now iterate over the events again and fire the events.
  for (uint32 index = 0; index < params.size(); index++) {
    const AccessibilityHostMsg_EventParams& param = params[index];

    // Find the node corresponding to the id that's the target of the
    // event (which may not be the root of the update tree).
    ui::AXNode* node = tree_->GetFromId(param.id);
    if (!node)
      continue;

    ui::AXEvent event_type = param.event_type;
    if (event_type == ui::AX_EVENT_FOCUS ||
        event_type == ui::AX_EVENT_BLUR) {
      SetFocus(node, false);

      if (osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_HIDDEN &&
          osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED)
        osk_state_ = OSK_ALLOWED;

      // Don't send a native focus event if the window itself doesn't
      // have focus.
      if (delegate_ && !delegate_->AccessibilityViewHasFocus())
        continue;
    }

    // Send the event event to the operating system.
    NotifyAccessibilityEvent(event_type, GetFromAXNode(node));
  }
}

void BrowserAccessibilityManager::OnLocationChanges(
    const std::vector<AccessibilityHostMsg_LocationChangeParams>& params) {
  for (size_t i = 0; i < params.size(); ++i) {
    BrowserAccessibility* obj = GetFromID(params[i].id);
    if (!obj)
      continue;
    ui::AXNode* node = obj->node();
    node->SetLocation(params[i].new_location);
    obj->OnLocationChanged();
  }
}

void BrowserAccessibilityManager::OnFindInPageResult(
    int request_id, int match_index, int start_id, int start_offset,
    int end_id, int end_offset) {
  find_in_page_info_.request_id = request_id;
  find_in_page_info_.match_index = match_index;
  find_in_page_info_.start_id = start_id;
  find_in_page_info_.start_offset = start_offset;
  find_in_page_info_.end_id = end_id;
  find_in_page_info_.end_offset = end_offset;

  if (find_in_page_info_.active_request_id == request_id)
    ActivateFindInPageResult(request_id);
}

void BrowserAccessibilityManager::ActivateFindInPageResult(
    int request_id) {
  find_in_page_info_.active_request_id = request_id;
  if (find_in_page_info_.request_id != request_id)
    return;

  BrowserAccessibility* node = GetFromID(find_in_page_info_.start_id);
  if (!node)
    return;

  // If an ancestor of this node is a leaf node, fire the notification on that.
  BrowserAccessibility* ancestor = node->GetParent();
  while (ancestor && ancestor != GetRoot()) {
    if (ancestor->PlatformIsLeaf())
      node = ancestor;
    ancestor = ancestor->GetParent();
  }

  // The "scrolled to anchor" notification is a great way to get a
  // screen reader to jump directly to a specific location in a document.
  NotifyAccessibilityEvent(ui::AX_EVENT_SCROLLED_TO_ANCHOR, node);
}

BrowserAccessibility* BrowserAccessibilityManager::GetActiveDescendantFocus(
    BrowserAccessibility* root) {
  BrowserAccessibility* node = BrowserAccessibilityManager::GetFocus(root);
  if (!node)
    return NULL;

  int active_descendant_id;
  if (node->GetIntAttribute(ui::AX_ATTR_ACTIVEDESCENDANT_ID,
                            &active_descendant_id)) {
    BrowserAccessibility* active_descendant =
        node->manager()->GetFromID(active_descendant_id);
    if (active_descendant)
      return active_descendant;
  }
  return node;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus(
    BrowserAccessibility* root) {
  if (!focus_)
    return NULL;

  if (root && !focus_->IsDescendantOf(root->node()))
    return NULL;

  if (!delegate())
    return NULL;

  BrowserAccessibility* obj = GetFromAXNode(focus_);
  if (obj->HasBoolAttribute(ui::AX_ATTR_IS_AX_TREE_HOST)) {
    BrowserAccessibilityManager* child_manager =
        delegate()->AccessibilityGetChildFrame(obj->GetId());
    if (child_manager)
      return child_manager->GetFocus(child_manager->GetRoot());
  }

  return obj;
}

void BrowserAccessibilityManager::SetFocus(ui::AXNode* node, bool notify) {
  if (focus_ != node)
    focus_ = node;

  if (notify && node && delegate_)
    delegate_->AccessibilitySetFocus(node->id());
}

void BrowserAccessibilityManager::SetFocus(
    BrowserAccessibility* obj, bool notify) {
  if (obj->node())
    SetFocus(obj->node(), notify);
}

void BrowserAccessibilityManager::DoDefaultAction(
    const BrowserAccessibility& node) {
  if (delegate_)
    delegate_->AccessibilityDoDefaultAction(node.GetId());
}

void BrowserAccessibilityManager::ScrollToMakeVisible(
    const BrowserAccessibility& node, gfx::Rect subfocus) {
  if (delegate_) {
    delegate_->AccessibilityScrollToMakeVisible(node.GetId(), subfocus);
  }
}

void BrowserAccessibilityManager::ScrollToPoint(
    const BrowserAccessibility& node, gfx::Point point) {
  if (delegate_) {
    delegate_->AccessibilityScrollToPoint(node.GetId(), point);
  }
}

void BrowserAccessibilityManager::SetValue(
    const BrowserAccessibility& node,
    const base::string16& value) {
  if (delegate_)
    delegate_->AccessibilitySetValue(node.GetId(), value);
}

void BrowserAccessibilityManager::SetTextSelection(
    const BrowserAccessibility& node,
    int start_offset,
    int end_offset) {
  if (delegate_) {
    delegate_->AccessibilitySetTextSelection(
        node.GetId(), start_offset, end_offset);
  }
}

gfx::Rect BrowserAccessibilityManager::GetViewBounds() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (delegate)
    return delegate->AccessibilityGetViewBounds();
  return gfx::Rect();
}

BrowserAccessibility* BrowserAccessibilityManager::NextInTreeOrder(
    BrowserAccessibility* node) {
  if (!node)
    return NULL;

  if (node->PlatformChildCount() > 0)
    return node->PlatformGetChild(0);
  while (node) {
    if (node->GetParent() &&
        node->GetIndexInParent() <
            static_cast<int>(node->GetParent()->PlatformChildCount()) - 1) {
      return node->GetParent()->PlatformGetChild(node->GetIndexInParent() + 1);
    }
    node = node->GetParent();
  }

  return NULL;
}

BrowserAccessibility* BrowserAccessibilityManager::PreviousInTreeOrder(
    BrowserAccessibility* node) {
  if (!node)
    return NULL;

  if (node->GetParent() && node->GetIndexInParent() > 0) {
    node = node->GetParent()->PlatformGetChild(node->GetIndexInParent() - 1);
    while (node->PlatformChildCount() > 0)
      node = node->PlatformGetChild(node->PlatformChildCount() - 1);
    return node;
  }

  return node->GetParent();
}

void BrowserAccessibilityManager::OnNodeWillBeDeleted(ui::AXNode* node) {
  if (node == focus_ && tree_) {
    if (node != tree_->root())
      SetFocus(tree_->root(), false);
    else
      focus_ = NULL;
  }
  if (id_wrapper_map_.find(node->id()) == id_wrapper_map_.end())
    return;
  GetFromAXNode(node)->Destroy();
  id_wrapper_map_.erase(node->id());
}

void BrowserAccessibilityManager::OnSubtreeWillBeDeleted(ui::AXNode* node) {
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (obj)
    obj->OnSubtreeWillBeDeleted();
}

void BrowserAccessibilityManager::OnNodeCreated(ui::AXNode* node) {
  BrowserAccessibility* wrapper = factory_->Create();
  wrapper->Init(this, node);
  id_wrapper_map_[node->id()] = wrapper;
  wrapper->OnDataChanged();
}

void BrowserAccessibilityManager::OnNodeChanged(ui::AXNode* node) {
  GetFromAXNode(node)->OnDataChanged();
}

void BrowserAccessibilityManager::OnAtomicUpdateFinished(
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
}

BrowserAccessibilityDelegate*
    BrowserAccessibilityManager::GetDelegateFromRootManager() {
  BrowserAccessibilityManager* manager = this;
  while (manager->delegate()) {
    BrowserAccessibility* host_node_in_parent_frame =
        manager->delegate()->AccessibilityGetParentFrame();
    if (!host_node_in_parent_frame)
      break;
    manager = host_node_in_parent_frame->manager();
  }
  return manager->delegate();
}

ui::AXTreeUpdate BrowserAccessibilityManager::SnapshotAXTreeForTesting() {
  scoped_ptr<ui::AXTreeSource<const ui::AXNode*> > tree_source(
      tree_->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*> serializer(tree_source.get());
  ui::AXTreeUpdate update;
  serializer.SerializeChanges(tree_->root(), &update);
  return update;
}

}  // namespace content
