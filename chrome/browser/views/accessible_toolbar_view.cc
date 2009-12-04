// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/view_ids.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/accessible_toolbar_view.h"
#include "views/controls/button/menu_button.h"
#include "views/focus/view_storage.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"

AccessibleToolbarView::AccessibleToolbarView()
    : selected_focused_view_(NULL),
      last_focused_view_storage_id_(-1) {
}

AccessibleToolbarView::~AccessibleToolbarView() {
}

void AccessibleToolbarView::InitiateTraversal(int view_storage_id) {
  // We only traverse if accessibility is active.
  if (selected_focused_view_)
    return;

  // Save the storage id to the last focused view. This would be used to request
  // focus to the view when the traversal is ended.
  last_focused_view_storage_id_ = view_storage_id;

  // Request focus to the toolbar.
  RequestFocus();
}

views::View* AccessibleToolbarView::GetNextAccessibleView(int view_index,
                                                          bool forward) {
  int modifier = forward ? 1 : -1;
  int current_view_index = view_index + modifier;

  while ((current_view_index >= 0) &&
         (current_view_index < GetChildViewCount())) {
    // Try to find the next available view that can be interacted with. That
    // view must be enabled, visible, and traversable.
    views::View* current_view = GetChildViewAt(current_view_index);
    if (current_view->IsEnabled() && current_view->IsVisible() &&
        IsAccessibleViewTraversable(current_view)) {
      return current_view;
    }
    current_view_index += modifier;
  }

  // No button is available in the specified direction.
  return NULL;
}

bool AccessibleToolbarView::IsAccessibleViewTraversable(views::View* view) {
  return true;
}

void AccessibleToolbarView::DidGainFocus() {
  // Check to see if the accessible focus should be restored to previously
  // focused button. The must be enabled and visible in the toolbar.
  if (!selected_focused_view_ ||
      !selected_focused_view_->IsEnabled() ||
      !selected_focused_view_->IsVisible()) {
    // Find first accessible child (-1 to start search at parent).
    selected_focused_view_ = GetNextAccessibleView(-1, true);

    // No buttons enabled or visible.
    if (!selected_focused_view_)
      return;
  }

  // Set the focus to the current accessible view.
  SetFocusToAccessibleView();
}

void AccessibleToolbarView::WillLoseFocus() {
  // Any tooltips that are active should be hidden when toolbar loses focus.
  if (GetWidget() && GetWidget()->GetTooltipManager())
    GetWidget()->GetTooltipManager()->HideKeyboardTooltip();

  // Removes the child accessibility view's focus when toolbar loses focus. It
  // will not remove the view from the ViewStorage because it might be
  // traversing to another toolbar hence the last focused view should not be
  // removed.
  if (selected_focused_view_) {
    selected_focused_view_->SetHotTracked(false);
    selected_focused_view_ = NULL;
  }
}

void AccessibleToolbarView::ShowContextMenu(int x, int y,
                                            bool is_mouse_gesture) {
  if (selected_focused_view_)
    selected_focused_view_->ShowContextMenu(x, y, is_mouse_gesture);
}

void AccessibleToolbarView::RequestFocus() {
  // When the toolbar needs to request focus, the default implementation of
  // View::RequestFocus requires the View to be focusable. Since ToolbarView is
  // not technically focused, we need to temporarily set and remove focus so
  // that it can focus back to its focused state. |selected_focused_view_| is
  // not necessarily set since it can be null if this view has already lost
  // focus, such as traversing through the context menu.
  SetFocusable(true);
  View::RequestFocus();
  SetFocusable(false);
}

bool AccessibleToolbarView::OnKeyPressed(const views::KeyEvent& e) {
  if (!HasFocus())
    return View::OnKeyPressed(e);

  int focused_view = GetChildIndex(selected_focused_view_);
  views::View* next_view = NULL;

  switch (e.GetKeyCode()) {
    case base::VKEY_LEFT:
      next_view = GetNextAccessibleView(focused_view, false);
      break;
    case base::VKEY_RIGHT:
      next_view = GetNextAccessibleView(focused_view, true);
      break;
    case base::VKEY_DOWN:
    case base::VKEY_RETURN:
      if (selected_focused_view_ && selected_focused_view_->GetClassName() ==
          views::MenuButton::kViewClassName) {
        // If a menu button is activated and its menu is displayed, then the
        // active tooltip should be hidden.
        if (GetWidget()->GetTooltipManager())
          GetWidget()->GetTooltipManager()->HideKeyboardTooltip();

        // Safe to cast, given to above check.
        static_cast<views::MenuButton*>(selected_focused_view_)->Activate();

        // If activate did not trigger a focus change, the menu button should
        // remain hot tracked since the view is still focused.
        if (selected_focused_view_)
          selected_focused_view_->SetHotTracked(true);
        return true;
      }
    default:
      // If key is not handled explicitly, pass it on to view.
      if (selected_focused_view_)
        return selected_focused_view_->OnKeyPressed(e);
      else
        return View::OnKeyPressed(e);
  }

  // No buttons enabled, visible, or focus hasn't moved.
  if (!next_view || !selected_focused_view_)
    return false;

  // Remove hot-tracking from old focused button.
  selected_focused_view_->SetHotTracked(false);

  // All is well, update the focused child member variable.
  selected_focused_view_ = next_view;

  // Set the focus to the current accessible view.
  SetFocusToAccessibleView();
  return true;
}

bool AccessibleToolbarView::OnKeyReleased(const views::KeyEvent& e) {
  if (!selected_focused_view_)
    return false;

  // Have keys be handled by the views themselves.
  return selected_focused_view_->OnKeyReleased(e);
}

bool AccessibleToolbarView::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  // Accessibility focus must be present in order to handle ESC and TAB related
  // key events.
  if (!HasFocus())
    return false;

  // The ancestor *must* be a BrowserView.
  views::View* view = GetAncestorWithClassName(BrowserView::kViewClassName);
  if (!view)
    return false;

  // Given the check above, we can ensure its a BrowserView.
  BrowserView* browser_view = static_cast<BrowserView*>(view);

  // Handle ESC and TAB events.
  switch (e.GetKeyCode()) {
    case base::VKEY_ESCAPE: {
      SetFocusToLastFocusedView();
      return true;
    }
    case base::VKEY_TAB: {
      if (e.IsShiftDown()) {
        browser_view->TraverseNextAccessibleToolbar(false);
      } else {
        browser_view->TraverseNextAccessibleToolbar(true);
      }
      return true;
    }
    default: return false;
  }
}

bool AccessibleToolbarView::GetAccessibleName(std::wstring* name) {
  *name = accessible_name_;
  return !accessible_name_.empty();
}

bool AccessibleToolbarView::GetAccessibleRole(AccessibilityTypes::Role* role) {
  DCHECK(role);

  *role = AccessibilityTypes::ROLE_TOOLBAR;
  return true;
}

void AccessibleToolbarView::SetAccessibleName(const std::wstring& name) {
  accessible_name_ = name;
}

void AccessibleToolbarView::ViewHierarchyChanged(bool is_add, View* parent,
                                                 View* child) {
  // When the toolbar is removed, traverse to the next accessible toolbar.
  if (child == selected_focused_view_ && !is_add) {
    selected_focused_view_->SetHotTracked(false);
    selected_focused_view_ = NULL;
    SetFocusToLastFocusedView();
  }
}

void AccessibleToolbarView::SetFocusToAccessibleView() {
  // Hot-track new focused button.
  selected_focused_view_->SetHotTracked(true);

  // Show the tooltip for the view that got the focus.
  if (GetWidget()->GetTooltipManager()) {
    GetWidget()->GetTooltipManager()->ShowKeyboardTooltip(
        selected_focused_view_);
  }

#if defined(OS_WIN)
  // Retrieve information to generate an accessible focus event.
  gfx::NativeView wnd = GetWidget()->GetNativeView();
  int view_id = selected_focused_view_->GetID();
  // Notify Access Technology that there was a change in keyboard focus.
  ::NotifyWinEvent(EVENT_OBJECT_FOCUS, wnd, OBJID_CLIENT,
                   static_cast<LONG>(view_id));
#else
  NOTIMPLEMENTED();
#endif
}

void AccessibleToolbarView::SetFocusToLastFocusedView() {
    views::ViewStorage* view_storage = views::ViewStorage::GetSharedInstance();
    views::View* focused_view =
        view_storage->RetrieveView(last_focused_view_storage_id_);
    if (focused_view) {
      view_storage->RemoveView(last_focused_view_storage_id_);
      focused_view->RequestFocus();
    } else {
      // The ancestor *must* be a BrowserView. The check below can ensure that.
      views::View* view = GetAncestorWithClassName(BrowserView::kViewClassName);
      if (!view)
        return;
      BrowserView* browser_view = static_cast<BrowserView*>(view);

      // Force the focus to be set on the location bar.
      browser_view->SetFocusToLocationBar();
    }
}
