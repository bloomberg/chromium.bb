// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container_gtk.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "views/focus/focus_manager.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerGtk, public:

NativeTabContentsContainerGtk::NativeTabContentsContainerGtk(
    TabContentsContainer* container)
    : container_(container),
      focus_callback_id_(0) {
  SetID(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);
}

NativeTabContentsContainerGtk::~NativeTabContentsContainerGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerGtk, NativeTabContentsContainer overrides:

void NativeTabContentsContainerGtk::AttachContents(TabContents* contents) {
  Attach(contents->GetNativeView());
}

void NativeTabContentsContainerGtk::DetachContents(TabContents* contents) {
  gtk_widget_hide(contents->GetNativeView());

  // Now detach the TabContents.
  Detach();
}

void NativeTabContentsContainerGtk::SetFastResize(bool fast_resize) {
  set_fast_resize(fast_resize);
}

void NativeTabContentsContainerGtk::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  // If we are focused, we need to pass the focus to the new RenderViewHost.
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager->GetFocusedView() == this)
    OnFocus();
}

views::View* NativeTabContentsContainerGtk::GetView() {
  return this;
}

void NativeTabContentsContainerGtk::TabContentsFocused(
    TabContents* tab_contents) {
#if !defined(TOUCH_UI)
  // Called when the tab contents native view gets focused (typically through a
  // user click).  We make ourself the focused view, so the focus is restored
  // properly when the browser window is deactivated/reactivated.
  views::FocusManager* focus_manager = GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(this);
#else
  // no native views in TOUCH_UI, so don't steal the focus
#endif
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerGtk, views::View overrides:

bool NativeTabContentsContainerGtk::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  // Don't look-up accelerators or tab-traverse if we are showing a non-crashed
  // TabContents.
  // We'll first give the page a chance to process the key events.  If it does
  // not process them, they'll be returned to us and we'll treat them as
  // accelerators then.
  return container_->tab_contents() &&
         !container_->tab_contents()->is_crashed();
}

views::FocusTraversable* NativeTabContentsContainerGtk::GetFocusTraversable() {
  return NULL;
}

bool NativeTabContentsContainerGtk::IsFocusable() const {
  // We need to be focusable when our contents is not a view hierarchy, as
  // clicking on the contents needs to focus us.
  return container_->tab_contents() != NULL;
}

void NativeTabContentsContainerGtk::OnFocus() {
  if (container_->tab_contents())
    container_->tab_contents()->Focus();
}

void NativeTabContentsContainerGtk::RequestFocus() {
  // This is a hack to circumvent the fact that a view does not explicitly get
  // a call to set the focus if it already has the focus. This causes a problem
  // with tabs such as the TabContents that instruct the RenderView that it got
  // focus when they actually get the focus. When switching from one TabContents
  // tab that has focus to another TabContents tab that had focus, since the
  // TabContentsContainerView already has focus, OnFocus() would not be called
  // and the RenderView would not get notified it got focused.
  // By clearing the focused view before-hand, we ensure OnFocus() will be
  // called.
  views::FocusManager* focus_manager = GetFocusManager();
  if (focus_manager)
    focus_manager->SetFocusedView(NULL);
  View::RequestFocus();
}

void NativeTabContentsContainerGtk::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  if (!container_->tab_contents())
    return;
  // Give an opportunity to the tab to reset its focus.
  if (container_->tab_contents()->interstitial_page()) {
    container_->tab_contents()->interstitial_page()->FocusThroughTabTraversal(
        reverse);
    return;
  }
  container_->tab_contents()->FocusThroughTabTraversal(reverse);
}

AccessibilityTypes::Role NativeTabContentsContainerGtk::GetAccessibleRole() {
  return AccessibilityTypes::ROLE_GROUPING;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainer, public:

// static
NativeTabContentsContainer* NativeTabContentsContainer::CreateNativeContainer(
    TabContentsContainer* container) {
  return new NativeTabContentsContainerGtk(container);
}
