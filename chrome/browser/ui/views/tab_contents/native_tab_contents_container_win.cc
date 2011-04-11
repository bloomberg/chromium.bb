// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container_win.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "views/focus/focus_manager.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, public:

NativeTabContentsContainerWin::NativeTabContentsContainerWin(
    TabContentsContainer* container)
    : container_(container) {
  SetID(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);
}

NativeTabContentsContainerWin::~NativeTabContentsContainerWin() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, NativeTabContentsContainer overrides:

void NativeTabContentsContainerWin::AttachContents(TabContents* contents) {
  // We need to register the tab contents window with the BrowserContainer so
  // that the BrowserContainer is the focused view when the focus is on the
  // TabContents window (for the TabContents case).
  set_focus_view(this);

  Attach(contents->GetNativeView());
}

void NativeTabContentsContainerWin::DetachContents(TabContents* contents) {
  // Detach the TabContents.  Do this before we unparent the
  // TabContentsViewViews so that the window hierarchy is intact for any
  // cleanup during Detach().
  Detach();

  // TODO(brettw) should this move to NativeViewHost::Detach?  It
  // needs cleanup regardless.
  HWND container_hwnd = contents->GetNativeView();
  if (container_hwnd) {
    // Hide the contents before adjusting its parent to avoid a full desktop
    // flicker.
    ShowWindow(container_hwnd, SW_HIDE);

    // Reset the parent to NULL to ensure hidden tabs don't receive messages.
    static_cast<TabContentsViewViews*>(contents->view())->Unparent();
  }
}

void NativeTabContentsContainerWin::SetFastResize(bool fast_resize) {
  set_fast_resize(fast_resize);
}

void NativeTabContentsContainerWin::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  // If we are focused, we need to pass the focus to the new RenderViewHost.
  if (GetFocusManager()->GetFocusedView() == this)
    OnFocus();
}

views::View* NativeTabContentsContainerWin::GetView() {
  return this;
}

void NativeTabContentsContainerWin::TabContentsFocused(
    TabContents* tab_contents) {
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerWin, views::View overrides:

bool NativeTabContentsContainerWin::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  // Don't look-up accelerators or tab-traversal if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return container_->tab_contents() &&
         !container_->tab_contents()->is_crashed();
}

bool NativeTabContentsContainerWin::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return container_->tab_contents() != NULL;
}

void NativeTabContentsContainerWin::OnFocus() {
  if (container_->tab_contents())
    container_->tab_contents()->Focus();
}

void NativeTabContentsContainerWin::RequestFocus() {
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

void NativeTabContentsContainerWin::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  container_->tab_contents()->FocusThroughTabTraversal(reverse);
}

void NativeTabContentsContainerWin::GetAccessibleState(
    ui::AccessibleViewState* state) {
  state->role = ui::AccessibilityTypes::ROLE_GROUPING;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainer, public:

// static
NativeTabContentsContainer* NativeTabContentsContainer::CreateNativeContainer(
    TabContentsContainer* container) {
  return new NativeTabContentsContainerWin(container);
}
