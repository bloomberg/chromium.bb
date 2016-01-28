// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager.h"

#include <stddef.h>

#include "base/logging.h"
#include "build/build_config.h"
#include "content/browser/accessibility/browser_accessibility.h"
#include "content/common/accessibility_messages.h"
#include "ui/accessibility/ax_tree_serializer.h"

namespace content {

namespace {

// Search the tree recursively from |node| and return any node that has
// a child tree ID of |ax_tree_id|.
BrowserAccessibility* FindNodeWithChildTreeId(BrowserAccessibility* node,
                                              int ax_tree_id) {
  if (!node)
    return nullptr;

  if (node->GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID) == ax_tree_id)
    return node;

  for (unsigned int i = 0; i < node->InternalChildCount(); ++i) {
    BrowserAccessibility* child = node->InternalGetChild(i);
    BrowserAccessibility* result = FindNodeWithChildTreeId(child, ax_tree_id);
    if (result)
      return result;
  }

  return nullptr;
}

}  // namespace

// Map from AXTreeID to BrowserAccessibilityManager
using AXTreeIDMap =
    base::hash_map<AXTreeIDRegistry::AXTreeID, BrowserAccessibilityManager*>;
base::LazyInstance<AXTreeIDMap> g_ax_tree_id_map = LAZY_INSTANCE_INITIALIZER;

ui::AXTreeUpdate MakeAXTreeUpdate(
    const ui::AXNodeData& node1,
    const ui::AXNodeData& node2 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node3 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node4 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node5 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node6 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node7 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node8 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node9 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node10 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node11 /* = ui::AXNodeData() */,
    const ui::AXNodeData& node12 /* = ui::AXNodeData() */) {
  CR_DEFINE_STATIC_LOCAL(ui::AXNodeData, empty_data, ());
  int32_t no_id = empty_data.id;

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
  if (node10.id != no_id)
    update.nodes.push_back(node10);
  if (node11.id != no_id)
    update.nodes.push_back(node11);
  if (node12.id != no_id)
    update.nodes.push_back(node12);
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

#if !defined(PLATFORM_HAS_NATIVE_ACCESSIBILITY_IMPL)
// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXTreeUpdate& initial_tree,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManager(initial_tree, delegate, factory);
}
#endif

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::FromID(
    AXTreeIDRegistry::AXTreeID ax_tree_id) {
  AXTreeIDMap* ax_tree_id_map = g_ax_tree_id_map.Pointer();
  auto iter = ax_tree_id_map->find(ax_tree_id);
  return iter == ax_tree_id_map->end() ? nullptr : iter->second;
}

BrowserAccessibilityManager::BrowserAccessibilityManager(
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : delegate_(delegate),
      factory_(factory),
      tree_(new ui::AXSerializableTree()),
      focus_(NULL),
      user_is_navigating_away_(false),
      osk_state_(OSK_ALLOWED),
      ax_tree_id_(AXTreeIDRegistry::kNoAXTreeID),
      parent_node_id_from_parent_tree_(0) {
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
      osk_state_(OSK_ALLOWED),
      ax_tree_id_(AXTreeIDRegistry::kNoAXTreeID),
      parent_node_id_from_parent_tree_(0) {
  tree_->SetDelegate(this);
  Initialize(initial_tree);
}

BrowserAccessibilityManager::~BrowserAccessibilityManager() {
  tree_.reset(NULL);
  g_ax_tree_id_map.Get().erase(ax_tree_id_);
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
ui::AXTreeUpdate
BrowserAccessibilityManager::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  ui::AXTreeUpdate update;
  update.nodes.push_back(empty_document);
  return update;
}

BrowserAccessibility* BrowserAccessibilityManager::GetRoot() {
  // tree_ can be null during destruction.
  if (!tree_)
    return nullptr;

  // tree_->root() can be null during AXTreeDelegate callbacks.
  ui::AXNode* root = tree_->root();
  return root ? GetFromAXNode(root) : nullptr;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromAXNode(
    const ui::AXNode* node) const {
  if (!node)
    return nullptr;
  return GetFromID(node->id());
}

BrowserAccessibility* BrowserAccessibilityManager::GetFromID(int32_t id) const {
  const auto iter = id_wrapper_map_.find(id);
  if (iter != id_wrapper_map_.end())
    return iter->second;

  return nullptr;
}

BrowserAccessibility*
BrowserAccessibilityManager::GetParentNodeFromParentTree() {
  if (!GetRoot())
    return nullptr;

  int parent_tree_id = GetTreeData().parent_tree_id;
  BrowserAccessibilityManager* parent_manager =
      BrowserAccessibilityManager::FromID(parent_tree_id);
  if (!parent_manager)
    return nullptr;

  // Try to use the cached parent node from the most recent time this
  // was called.
  if (parent_node_id_from_parent_tree_) {
    BrowserAccessibility* parent_node = parent_manager->GetFromID(
        parent_node_id_from_parent_tree_);
    if (parent_node) {
      int parent_child_tree_id =
          parent_node->GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID);
      if (parent_child_tree_id == ax_tree_id_)
        return parent_node;
    }
  }

  // If that fails, search for it and cache it for next time.
  BrowserAccessibility* parent_node = FindNodeWithChildTreeId(
      parent_manager->GetRoot(), ax_tree_id_);
  if (parent_node) {
    parent_node_id_from_parent_tree_ = parent_node->GetId();
    return parent_node;
  }

  return nullptr;
}

const ui::AXTreeData& BrowserAccessibilityManager::GetTreeData() {
  return tree_->data();
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
  if (!focus_)
    return;

  osk_state_ = OSK_ALLOWED_WITHIN_FOCUSED_OBJECT;
  NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetFromAXNode(focus_));
}

bool BrowserAccessibilityManager::UseRootScrollOffsetsWhenComputingBounds() {
  return true;
}

void BrowserAccessibilityManager::OnAccessibilityEvents(
    const std::vector<AXEventNotificationDetails>& details) {
  bool should_send_initial_focus = false;

  // Process all changes to the accessibility tree first.
  for (uint32_t index = 0; index < details.size(); ++index) {
    const AXEventNotificationDetails& detail = details[index];
    if (!tree_->Unserialize(detail.update)) {
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

  if (should_send_initial_focus && NativeViewHasFocus())
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, GetFromAXNode(focus_));

  // Now iterate over the events again and fire the events.
  for (uint32_t index = 0; index < details.size(); index++) {
    const AXEventNotificationDetails& detail = details[index];

    // Find the node corresponding to the id that's the target of the
    // event (which may not be the root of the update tree).
    ui::AXNode* node = tree_->GetFromId(detail.id);
    if (!node)
      continue;

    ui::AXEvent event_type = detail.event_type;
    if (event_type == ui::AX_EVENT_FOCUS ||
        event_type == ui::AX_EVENT_BLUR) {
      SetFocus(node, false);

      if (osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_HIDDEN &&
          osk_state_ != OSK_DISALLOWED_BECAUSE_TAB_JUST_APPEARED)
        osk_state_ = OSK_ALLOWED;

      // Don't send a native focus event if the window itself doesn't
      // have focus.
      if (!NativeViewHasFocus())
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

bool BrowserAccessibilityManager::NativeViewHasFocus() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (delegate)
    return delegate->AccessibilityViewHasFocus();
  return false;
}

BrowserAccessibility* BrowserAccessibilityManager::GetFocus(
    BrowserAccessibility* root) {
  if (!focus_)
    return nullptr;

  if (root && !focus_->IsDescendantOf(root->node()))
    return nullptr;

  BrowserAccessibility* obj = GetFromAXNode(focus_);
  DCHECK(obj);
  if (obj->HasIntAttribute(ui::AX_ATTR_CHILD_TREE_ID)) {
    BrowserAccessibilityManager* child_manager =
        BrowserAccessibilityManager::FromID(
            obj->GetIntAttribute(ui::AX_ATTR_CHILD_TREE_ID));
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

void BrowserAccessibilityManager::SetScrollOffset(
    const BrowserAccessibility& node, gfx::Point offset) {
  if (delegate_) {
    delegate_->AccessibilitySetScrollOffset(node.GetId(), offset);
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
    delegate_->AccessibilitySetSelection(node.GetId(), start_offset,
                                         node.GetId(), end_offset);
  }
}

gfx::Rect BrowserAccessibilityManager::GetViewBounds() {
  BrowserAccessibilityDelegate* delegate = GetDelegateFromRootManager();
  if (delegate)
    return delegate->AccessibilityGetViewBounds();
  return gfx::Rect();
}

BrowserAccessibility* BrowserAccessibilityManager::NextInTreeOrder(
    BrowserAccessibility* node) const {
  if (!node)
    return nullptr;

  if (node->PlatformChildCount())
    return node->PlatformGetChild(0);

  while (node) {
    const auto sibling = node->GetNextSibling();
    if (sibling)
      return sibling;

    node = node->GetParent();
  }

  return nullptr;
}

BrowserAccessibility* BrowserAccessibilityManager::PreviousInTreeOrder(
    BrowserAccessibility* node) const {
  if (!node)
    return nullptr;

  const auto sibling = node->GetPreviousSibling();
  if (!sibling)
    return node->GetParent();

  if (sibling->PlatformChildCount())
    return sibling->PlatformDeepestLastChild();

  return sibling;
}

BrowserAccessibility* BrowserAccessibilityManager::PreviousTextOnlyObject(
    BrowserAccessibility* node) const {
      BrowserAccessibility* previous_node = PreviousInTreeOrder(node);
  while (previous_node && !previous_node->IsTextOnlyObject())
    previous_node = PreviousInTreeOrder(previous_node);

  return previous_node;
}

BrowserAccessibility* BrowserAccessibilityManager::NextTextOnlyObject(
    BrowserAccessibility* node) const {
  BrowserAccessibility* next_node = NextInTreeOrder(node);
  while (next_node && !next_node->IsTextOnlyObject())
    next_node = NextInTreeOrder(next_node);

  return next_node;
}

void BrowserAccessibilityManager::OnTreeDataChanged(ui::AXTree* tree) {
}

void BrowserAccessibilityManager::OnNodeWillBeDeleted(ui::AXTree* tree,
                                                      ui::AXNode* node) {
  DCHECK(node);
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

void BrowserAccessibilityManager::OnSubtreeWillBeDeleted(ui::AXTree* tree,
                                                         ui::AXNode* node) {
  DCHECK(node);
  BrowserAccessibility* obj = GetFromAXNode(node);
  if (obj)
    obj->OnSubtreeWillBeDeleted();
}

void BrowserAccessibilityManager::OnNodeCreated(ui::AXTree* tree,
                                                ui::AXNode* node) {
  BrowserAccessibility* wrapper = factory_->Create();
  wrapper->Init(this, node);
  id_wrapper_map_[node->id()] = wrapper;
  wrapper->OnDataChanged();
}

void BrowserAccessibilityManager::OnNodeChanged(ui::AXTree* tree,
                                                ui::AXNode* node) {
  DCHECK(node);
  GetFromAXNode(node)->OnDataChanged();
}

void BrowserAccessibilityManager::OnAtomicUpdateFinished(
    ui::AXTree* tree,
    bool root_changed,
    const std::vector<ui::AXTreeDelegate::Change>& changes) {
  bool ax_tree_id_changed = false;
  if (GetTreeData().tree_id != -1 && GetTreeData().tree_id != ax_tree_id_) {
    g_ax_tree_id_map.Get().erase(ax_tree_id_);
    ax_tree_id_ = GetTreeData().tree_id;
    g_ax_tree_id_map.Get().insert(std::make_pair(ax_tree_id_, this));
    ax_tree_id_changed = true;
  }

  if (ax_tree_id_changed || root_changed) {
    BrowserAccessibility* parent = GetParentNodeFromParentTree();
    if (parent) {
      parent->OnDataChanged();
      parent->manager()->NotifyAccessibilityEvent(
          ui::AX_EVENT_CHILDREN_CHANGED, parent);
    }
  }
}

BrowserAccessibilityManager* BrowserAccessibilityManager::GetRootManager() {
  if (!GetRoot())
    return nullptr;
  int parent_tree_id = GetTreeData().parent_tree_id;
  BrowserAccessibilityManager* parent_manager =
      BrowserAccessibilityManager::FromID(parent_tree_id);
  if (parent_manager)
    return parent_manager->GetRootManager();
  return this;
}

BrowserAccessibilityDelegate*
    BrowserAccessibilityManager::GetDelegateFromRootManager() {
  BrowserAccessibilityManager* root_manager = GetRootManager();
  if (root_manager)
    return root_manager->delegate();
  return nullptr;
}

ui::AXTreeUpdate
BrowserAccessibilityManager::SnapshotAXTreeForTesting() {
  scoped_ptr<ui::AXTreeSource<const ui::AXNode*,
                              ui::AXNodeData,
                              ui::AXTreeData> > tree_source(
      tree_->CreateTreeSource());
  ui::AXTreeSerializer<const ui::AXNode*,
                       ui::AXNodeData,
                       ui::AXTreeData> serializer(tree_source.get());
  ui::AXTreeUpdate update;
  serializer.SerializeChanges(tree_->root(), &update);
  return update;
}

}  // namespace content
