// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/accessible_pane_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "views/controls/button/menu_button.h"
#include "views/controls/native/native_view_host.h"
#include "views/focus/focus_search.h"
#include "views/focus/view_storage.h"
#include "views/widget/tooltip_manager.h"
#include "views/widget/widget.h"

AccessiblePaneView::AccessiblePaneView()
    : pane_has_focus_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(method_factory_(this)),
      focus_manager_(NULL),
      home_key_(ui::VKEY_HOME, false, false, false),
      end_key_(ui::VKEY_END, false, false, false),
      escape_key_(ui::VKEY_ESCAPE, false, false, false),
      left_key_(ui::VKEY_LEFT, false, false, false),
      right_key_(ui::VKEY_RIGHT, false, false, false),
      last_focused_view_storage_id_(-1) {
  focus_search_.reset(new views::FocusSearch(this, true, true));
}

AccessiblePaneView::~AccessiblePaneView() {
  if (pane_has_focus_) {
    focus_manager_->RemoveFocusChangeListener(this);
  }
}

bool AccessiblePaneView::SetPaneFocus(int view_storage_id,
                                      views::View* initial_focus) {
  if (!IsVisible())
    return false;

  // Save the storage id to the last focused view. This would be used to request
  // focus to the view when the traversal is ended.
  last_focused_view_storage_id_ = view_storage_id;

  if (!focus_manager_)
    focus_manager_ = GetFocusManager();

  // Use the provided initial focus if it's visible and enabled, otherwise
  // use the first focusable child.
  if (!initial_focus ||
      !Contains(initial_focus) ||
      !initial_focus->IsVisible() ||
      !initial_focus->IsEnabled()) {
    initial_focus = GetFirstFocusableChild();
  }

  // Return false if there are no focusable children.
  if (!initial_focus)
    return false;

  // Set focus to the initial view. If it's a location bar, use a special
  // method that tells it to select all, also.
  if (initial_focus->GetClassName() == LocationBarView::kViewClassName) {
    static_cast<LocationBarView*>(initial_focus)->FocusLocation(true);
  } else {
    focus_manager_->SetFocusedView(initial_focus);
  }

  // If we already have pane focus, we're done.
  if (pane_has_focus_)
    return true;

  // Otherwise, set accelerators and start listening for focus change events.
  pane_has_focus_ = true;
  focus_manager_->RegisterAccelerator(home_key_, this);
  focus_manager_->RegisterAccelerator(end_key_, this);
  focus_manager_->RegisterAccelerator(escape_key_, this);
  focus_manager_->RegisterAccelerator(left_key_, this);
  focus_manager_->RegisterAccelerator(right_key_, this);
  focus_manager_->AddFocusChangeListener(this);

  return true;
}

bool AccessiblePaneView::SetPaneFocusAndFocusDefault(
    int view_storage_id) {
  return SetPaneFocus(view_storage_id, GetDefaultFocusableChild());
}

void AccessiblePaneView::RemovePaneFocus() {
  focus_manager_->RemoveFocusChangeListener(this);
  pane_has_focus_ = false;

  focus_manager_->UnregisterAccelerator(home_key_, this);
  focus_manager_->UnregisterAccelerator(end_key_, this);
  focus_manager_->UnregisterAccelerator(escape_key_, this);
  focus_manager_->UnregisterAccelerator(left_key_, this);
  focus_manager_->UnregisterAccelerator(right_key_, this);
}

void AccessiblePaneView::LocationBarSelectAll() {
  views::View* focused_view = GetFocusManager()->GetFocusedView();
  if (focused_view &&
      focused_view->GetClassName() == LocationBarView::kViewClassName) {
    static_cast<LocationBarView*>(focused_view)->SelectAll();
  }
}

void AccessiblePaneView::RestoreLastFocusedView() {
  views::ViewStorage* view_storage = views::ViewStorage::GetInstance();
  views::View* last_focused_view =
      view_storage->RetrieveView(last_focused_view_storage_id_);
  if (last_focused_view) {
    focus_manager_->SetFocusedViewWithReason(
        last_focused_view, views::FocusManager::kReasonFocusRestore);
  } else {
    // Focus the location bar
    views::View* view = GetAncestorWithClassName(BrowserView::kViewClassName);
    if (view) {
      BrowserView* browser_view = static_cast<BrowserView*>(view);
      browser_view->SetFocusToLocationBar(false);
    }
  }
}

views::View* AccessiblePaneView::GetFirstFocusableChild() {
  FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      NULL, false, views::FocusSearch::DOWN, false,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

views::View* AccessiblePaneView::GetLastFocusableChild() {
  FocusTraversable* dummy_focus_traversable;
  views::View* dummy_focus_traversable_view;
  return focus_search_->FindNextFocusableView(
      this, true, views::FocusSearch::DOWN, false,
      &dummy_focus_traversable, &dummy_focus_traversable_view);
}

////////////////////////////////////////////////////////////////////////////////
// View overrides:

views::FocusTraversable* AccessiblePaneView::GetPaneFocusTraversable() {
  if (pane_has_focus_)
    return this;
  else
    return NULL;
}

bool AccessiblePaneView::AcceleratorPressed(
    const views::Accelerator& accelerator) {
  // Special case: don't handle any accelerators for the location bar,
  // so that it behaves exactly the same whether you focus it with Ctrl+L
  // or F6 or Alt+D or Alt+Shift+T.
  views::View* focused_view = focus_manager_->GetFocusedView();
  if ((focused_view->GetClassName() == LocationBarView::kViewClassName ||
       focused_view->GetClassName() == views::NativeViewHost::kViewClassName)) {
    return false;
  }

  switch (accelerator.GetKeyCode()) {
    case ui::VKEY_ESCAPE:
      RemovePaneFocus();
      RestoreLastFocusedView();
      return true;
    case ui::VKEY_LEFT:
      focus_manager_->AdvanceFocus(true);
      return true;
    case ui::VKEY_RIGHT:
      focus_manager_->AdvanceFocus(false);
      return true;
    case ui::VKEY_HOME:
      focus_manager_->SetFocusedViewWithReason(
          GetFirstFocusableChild(), views::FocusManager::kReasonFocusTraversal);
      return true;
    case ui::VKEY_END:
      focus_manager_->SetFocusedViewWithReason(
          GetLastFocusableChild(), views::FocusManager::kReasonFocusTraversal);
      return true;
    default:
      return false;
  }
}

void AccessiblePaneView::SetVisible(bool flag) {
  if (IsVisible() && !flag && pane_has_focus_) {
    RemovePaneFocus();
    RestoreLastFocusedView();
  }
  View::SetVisible(flag);
}

void AccessiblePaneView::GetAccessibleState(ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_PANE;
}

////////////////////////////////////////////////////////////////////////////////
// FocusChangeListener overrides:

void AccessiblePaneView::FocusWillChange(views::View* focused_before,
                                         views::View* focused_now) {
  if (!focused_now)
    return;

  views::FocusManager::FocusChangeReason reason =
      focus_manager_->focus_change_reason();

  if (focused_now->GetClassName() == LocationBarView::kViewClassName &&
      reason == views::FocusManager::kReasonFocusTraversal) {
    // Tabbing to the location bar should select all. Defer so that it happens
    // after the focus.
    MessageLoop::current()->PostTask(
        FROM_HERE, method_factory_.NewRunnableMethod(
            &AccessiblePaneView::LocationBarSelectAll));
  }

  if (!Contains(focused_now) ||
      reason == views::FocusManager::kReasonDirectFocusChange) {
    // We should remove pane focus (i.e. make most of the controls
    // not focusable again) either because the focus is leaving the pane,
    // or because the focus changed within the pane due to the user
    // directly focusing to a specific view (e.g., clicking on it).
    //
    // Defer rather than calling RemovePaneFocus right away, because we can't
    // remove |this| as a focus change listener while FocusManager is in the
    // middle of iterating over the list of listeners.
    MessageLoop::current()->PostTask(
        FROM_HERE, method_factory_.NewRunnableMethod(
            &AccessiblePaneView::RemovePaneFocus));
  }
}

////////////////////////////////////////////////////////////////////////////////
// FocusTraversable overrides:

views::FocusSearch* AccessiblePaneView::GetFocusSearch() {
  DCHECK(pane_has_focus_);
  return focus_search_.get();
}

views::FocusTraversable* AccessiblePaneView::GetFocusTraversableParent() {
  DCHECK(pane_has_focus_);
  return NULL;
}

views::View* AccessiblePaneView::GetFocusTraversableParentView() {
  DCHECK(pane_has_focus_);
  return NULL;
}
