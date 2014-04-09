// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/browser_accessibility_manager_win.h"

#include "base/command_line.h"
#include "base/win/scoped_comptr.h"
#include "base/win/windows_version.h"
#include "content/browser/accessibility/browser_accessibility_state_impl.h"
#include "content/browser/accessibility/browser_accessibility_win.h"
#include "content/browser/renderer_host/legacy_render_widget_host_win.h"
#include "content/common/accessibility_messages.h"

namespace content {

// static
BrowserAccessibilityManager* BrowserAccessibilityManager::Create(
    const ui::AXNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory) {
  return new BrowserAccessibilityManagerWin(
      content::LegacyRenderWidgetHostHWND::Create(GetDesktopWindow()).get(),
      NULL, src, delegate, factory);
}

BrowserAccessibilityManagerWin*
BrowserAccessibilityManager::ToBrowserAccessibilityManagerWin() {
  return static_cast<BrowserAccessibilityManagerWin*>(this);
}

BrowserAccessibilityManagerWin::BrowserAccessibilityManagerWin(
    LegacyRenderWidgetHostHWND* accessible_hwnd,
    IAccessible* parent_iaccessible,
    const ui::AXNodeData& src,
    BrowserAccessibilityDelegate* delegate,
    BrowserAccessibilityFactory* factory)
    : BrowserAccessibilityManager(src, delegate, factory),
      parent_hwnd_(accessible_hwnd->GetParent()),
      parent_iaccessible_(parent_iaccessible),
      tracked_scroll_object_(NULL),
      accessible_hwnd_(accessible_hwnd) {
  accessible_hwnd_->set_browser_accessibility_manager(this);
}

BrowserAccessibilityManagerWin::~BrowserAccessibilityManagerWin() {
  if (tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
  if (accessible_hwnd_)
    accessible_hwnd_->OnManagerDeleted();
}

// static
ui::AXNodeData BrowserAccessibilityManagerWin::GetEmptyDocument() {
  ui::AXNodeData empty_document;
  empty_document.id = 0;
  empty_document.role = ui::AX_ROLE_ROOT_WEB_AREA;
  empty_document.state =
      (1 << ui::AX_STATE_ENABLED) |
      (1 << ui::AX_STATE_READ_ONLY) |
      (1 << ui::AX_STATE_BUSY);
  return empty_document;
}

void BrowserAccessibilityManagerWin::MaybeCallNotifyWinEvent(DWORD event,
                                                             LONG child_id) {
  // Don't fire events if this view isn't hooked up to its parent.
  if (!parent_iaccessible())
    return;

  // If on Win 7 and complete accessibility is enabled, use the fake child HWND
  // to use as the root of the accessibility tree. See comments above
  // LegacyRenderWidgetHostHWND for details.
  if (BrowserAccessibilityStateImpl::GetInstance()->IsAccessibleBrowser()) {
    DCHECK(accessible_hwnd_);
    parent_hwnd_ = accessible_hwnd_->hwnd();
    parent_iaccessible_ = accessible_hwnd_->window_accessible();
  }
  ::NotifyWinEvent(event, parent_hwnd(), OBJID_CLIENT, child_id);
}

void BrowserAccessibilityManagerWin::AddNodeToMap(BrowserAccessibility* node) {
  BrowserAccessibilityManager::AddNodeToMap(node);
  LONG unique_id_win = node->ToBrowserAccessibilityWin()->unique_id_win();
  unique_id_to_renderer_id_map_[unique_id_win] = node->GetId();
}

void BrowserAccessibilityManagerWin::RemoveNode(BrowserAccessibility* node) {
  unique_id_to_renderer_id_map_.erase(
      node->ToBrowserAccessibilityWin()->unique_id_win());
  BrowserAccessibilityManager::RemoveNode(node);
  if (node == tracked_scroll_object_) {
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::OnWindowFocused() {
  // Fire a focus event on the root first and then the focused node.
  if (focus_ != root_)
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, root_);
  BrowserAccessibilityManager::OnWindowFocused();
}

void BrowserAccessibilityManagerWin::OnWindowBlurred() {
  // Fire a blur event on the focused node first and then the root.
  BrowserAccessibilityManager::OnWindowBlurred();
  if (focus_ != root_)
    NotifyAccessibilityEvent(ui::AX_EVENT_BLUR, root_);
}

void BrowserAccessibilityManagerWin::NotifyAccessibilityEvent(
    ui::AXEvent event_type,
    BrowserAccessibility* node) {
  if (node->GetRole() == ui::AX_ROLE_INLINE_TEXT_BOX)
    return;

  LONG event_id = EVENT_MIN;
  switch (event_type) {
    case ui::AX_EVENT_ACTIVEDESCENDANTCHANGED:
      event_id = IA2_EVENT_ACTIVE_DESCENDANT_CHANGED;
      break;
    case ui::AX_EVENT_ALERT:
      event_id = EVENT_SYSTEM_ALERT;
      break;
    case ui::AX_EVENT_ARIA_ATTRIBUTE_CHANGED:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_AUTOCORRECTION_OCCURED:
      event_id = IA2_EVENT_OBJECT_ATTRIBUTE_CHANGED;
      break;
    case ui::AX_EVENT_BLUR:
      // Equivalent to focus on the root.
      event_id = EVENT_OBJECT_FOCUS;
      node = GetRoot();
      break;
    case ui::AX_EVENT_CHECKED_STATE_CHANGED:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case ui::AX_EVENT_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ui::AX_EVENT_FOCUS:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_INVALID_STATUS_CHANGED:
      event_id = EVENT_OBJECT_STATECHANGE;
      break;
    case ui::AX_EVENT_LIVE_REGION_CHANGED:
      // TODO: try not firing a native notification at all, since
      // on Windows, each individual item in a live region that changes
      // already gets its own notification.
      event_id = EVENT_OBJECT_REORDER;
      break;
    case ui::AX_EVENT_LOAD_COMPLETE:
      event_id = IA2_EVENT_DOCUMENT_LOAD_COMPLETE;
      break;
    case ui::AX_EVENT_MENU_LIST_ITEM_SELECTED:
      event_id = EVENT_OBJECT_FOCUS;
      break;
    case ui::AX_EVENT_MENU_LIST_VALUE_CHANGED:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    case ui::AX_EVENT_HIDE:
      event_id = EVENT_OBJECT_HIDE;
      break;
    case ui::AX_EVENT_SHOW:
      event_id = EVENT_OBJECT_SHOW;
      break;
    case ui::AX_EVENT_SCROLL_POSITION_CHANGED:
      event_id = EVENT_SYSTEM_SCROLLINGEND;
      break;
    case ui::AX_EVENT_SCROLLED_TO_ANCHOR:
      event_id = EVENT_SYSTEM_SCROLLINGSTART;
      break;
    case ui::AX_EVENT_SELECTED_CHILDREN_CHANGED:
      event_id = EVENT_OBJECT_SELECTIONWITHIN;
      break;
    case ui::AX_EVENT_SELECTED_TEXT_CHANGED:
      event_id = IA2_EVENT_TEXT_CARET_MOVED;
      break;
    case ui::AX_EVENT_TEXT_CHANGED:
      event_id = EVENT_OBJECT_NAMECHANGE;
      break;
    case ui::AX_EVENT_TEXT_INSERTED:
      event_id = IA2_EVENT_TEXT_INSERTED;
      break;
    case ui::AX_EVENT_TEXT_REMOVED:
      event_id = IA2_EVENT_TEXT_REMOVED;
      break;
    case ui::AX_EVENT_VALUE_CHANGED:
      event_id = EVENT_OBJECT_VALUECHANGE;
      break;
    default:
      // Not all WebKit accessibility events result in a Windows
      // accessibility notification.
      break;
  }

  if (event_id != EVENT_MIN) {
    // Pass the node's unique id in the |child_id| argument to NotifyWinEvent;
    // the AT client will then call get_accChild on the HWND's accessibility
    // object and pass it that same id, which we can use to retrieve the
    // IAccessible for this node.
    LONG child_id = node->ToBrowserAccessibilityWin()->unique_id_win();
    MaybeCallNotifyWinEvent(event_id, child_id);
  }

  // If this is a layout complete notification (sent when a container scrolls)
  // and there is a descendant tracked object, send a notification on it.
  // TODO(dmazzoni): remove once http://crbug.com/113483 is fixed.
  if (event_type == ui::AX_EVENT_LAYOUT_COMPLETE &&
      tracked_scroll_object_ &&
      tracked_scroll_object_->IsDescendantOf(node)) {
    MaybeCallNotifyWinEvent(
        IA2_EVENT_VISIBLE_DATA_CHANGED,
        tracked_scroll_object_->ToBrowserAccessibilityWin()->unique_id_win());
    tracked_scroll_object_->Release();
    tracked_scroll_object_ = NULL;
  }
}

void BrowserAccessibilityManagerWin::OnRootChanged() {
  if (delegate_ && delegate_->HasFocus())
    NotifyAccessibilityEvent(ui::AX_EVENT_FOCUS, root_);
}

void BrowserAccessibilityManagerWin::TrackScrollingObject(
    BrowserAccessibilityWin* node) {
  if (tracked_scroll_object_)
    tracked_scroll_object_->Release();
  tracked_scroll_object_ = node;
  tracked_scroll_object_->AddRef();
}

BrowserAccessibilityWin* BrowserAccessibilityManagerWin::GetFromUniqueIdWin(
    LONG unique_id_win) {
  base::hash_map<LONG, int32>::iterator iter =
      unique_id_to_renderer_id_map_.find(unique_id_win);
  if (iter != unique_id_to_renderer_id_map_.end()) {
    BrowserAccessibility* result = GetFromRendererID(iter->second);
    if (result)
      return result->ToBrowserAccessibilityWin();
  }
  return NULL;
}

void BrowserAccessibilityManagerWin::OnAccessibleHwndDeleted() {
  // If the AccessibleHWND is deleted, |parent_hwnd_| and
  // |parent_iaccessible_| are no longer valid either, since they were
  // derived from AccessibleHWND. We don't have to restore them to
  // previous values, though, because this should only happen
  // during the destruct sequence for this window.
  accessible_hwnd_ = NULL;
  parent_hwnd_ = NULL;
  parent_iaccessible_ = NULL;
}

}  // namespace content
