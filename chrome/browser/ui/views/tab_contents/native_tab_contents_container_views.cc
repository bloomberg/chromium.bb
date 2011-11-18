// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container_views.h"

#include "chrome/browser/renderer_host/render_widget_host_view_views.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/focus/widget_focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "views/widget/native_widget_views.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerViews, public:

NativeTabContentsContainerViews::NativeTabContentsContainerViews(
    TabContentsContainer* container)
    : container_(container) {
  set_id(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);
  SetLayoutManager(new views::FillLayout);
}

NativeTabContentsContainerViews::~NativeTabContentsContainerViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerViews, NativeTabContentsContainer overrides:

void NativeTabContentsContainerViews::AttachContents(TabContents* contents) {
  TabContentsViewViews* widget =
      static_cast<TabContentsViewViews*>(contents->view());
  views::NativeWidgetViews* nwv =
      static_cast<views::NativeWidgetViews*>(widget->native_widget());
  AddChildView(nwv->GetView());
  Layout();
}

void NativeTabContentsContainerViews::DetachContents(TabContents* contents) {
  TabContentsViewViews* widget =
      static_cast<TabContentsViewViews*>(contents->view());
  views::NativeWidgetViews* nwv =
      static_cast<views::NativeWidgetViews*>(widget->native_widget());
  RemoveChildView(nwv->GetView());
}

void NativeTabContentsContainerViews::SetFastResize(bool fast_resize) {
}

bool NativeTabContentsContainerViews::GetFastResize() const {
  return false;
}

bool NativeTabContentsContainerViews::FastResizeAtLastLayout() const {
  return false;
}

void NativeTabContentsContainerViews::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  // If we are focused, we need to pass the focus to the new RenderViewHost.
  if (GetFocusManager()->GetFocusedView() == this)
    OnFocus();
}

views::View* NativeTabContentsContainerViews::GetView() {
  return this;
}

void NativeTabContentsContainerViews::TabContentsFocused(
    TabContents* tab_contents) {
  // This is called from RWHVViews::OnFocus, which means
  // the focus manager already set focus to RWHVViews, so don't
  // Update focus manager.
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, views::View overrides:

bool NativeTabContentsContainerViews::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  // Don't look-up accelerators or tab-traversal if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return container_->tab_contents() &&
         !container_->tab_contents()->is_crashed();
}

bool NativeTabContentsContainerViews::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return container_->tab_contents() != NULL;
}

void NativeTabContentsContainerViews::OnFocus() {
  if (container_->tab_contents())
    container_->tab_contents()->Focus();
}

void NativeTabContentsContainerViews::RequestFocus() {
  // This is a hack to circumvent the fact that a the OnFocus() method is not
  // invoked when RequestFocus() is called on an already focused view.
  // The TabContentsContainer is the view focused when the TabContents has
  // focus.  When switching between from one tab that has focus to another tab
  // that should also have focus, RequestFocus() is invoked one the
  // TabContentsContainer.  In order to make sure OnFocus() is invoked we need
  // to clear the focus before hands.
  {
    // Disable notifications.  Clear focus will assign the focus to the main
    // browser window.  Because this change of focus was not user requested,
    // don't send it to listeners.
    views::AutoNativeNotificationDisabler local_notification_disabler;
    GetFocusManager()->ClearFocus();
  }
  View::RequestFocus();
}

void NativeTabContentsContainerViews::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  container_->tab_contents()->FocusThroughTabTraversal(reverse);
}

void NativeTabContentsContainerViews::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
}

gfx::NativeViewAccessible
    NativeTabContentsContainerViews::GetNativeViewAccessible() {
  return View::GetNativeViewAccessible();
}

