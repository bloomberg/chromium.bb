// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container_aura.h"

#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_container_views.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_container.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#include "content/browser/renderer_host/render_widget_host_view_win.h"
#include "content/browser/tab_contents/interstitial_page.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "views/views_delegate.h"
#include "views/focus/focus_manager.h"

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerAura, public:

NativeTabContentsContainerAura::NativeTabContentsContainerAura(
    TabContentsContainer* container)
    : container_(container) {
  set_id(VIEW_ID_TAB_CONTAINER_FOCUS_VIEW);
}

NativeTabContentsContainerAura::~NativeTabContentsContainerAura() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerAura, NativeTabContentsContainer overrides:

void NativeTabContentsContainerAura::AttachContents(TabContents* contents) {
}

void NativeTabContentsContainerAura::DetachContents(TabContents* contents) {
  // TODO(beng):
  NOTIMPLEMENTED();
}

void NativeTabContentsContainerAura::SetFastResize(bool fast_resize) {
  NOTIMPLEMENTED();
}

void NativeTabContentsContainerAura::RenderViewHostChanged(
    RenderViewHost* old_host,
    RenderViewHost* new_host) {
  NOTIMPLEMENTED();
}

views::View* NativeTabContentsContainerAura::GetView() {
  NOTIMPLEMENTED();
  return this;
}

void NativeTabContentsContainerAura::TabContentsFocused(
    TabContents* tab_contents) {
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainerAura, views::View overrides:

bool NativeTabContentsContainerAura::SkipDefaultKeyEventProcessing(
    const views::KeyEvent& e) {
  NOTIMPLEMENTED();
  return false;
}

bool NativeTabContentsContainerAura::IsFocusable() const {
  NOTIMPLEMENTED();
  return false;
}

void NativeTabContentsContainerAura::OnFocus() {
  NOTIMPLEMENTED();
}

void NativeTabContentsContainerAura::RequestFocus() {
  NOTIMPLEMENTED();
}

void NativeTabContentsContainerAura::AboutToRequestFocusFromTabTraversal(
    bool reverse) {
  NOTIMPLEMENTED();
}

void NativeTabContentsContainerAura::GetAccessibleState(
    ui::AccessibleViewState* state) {
  NOTIMPLEMENTED();
}

gfx::NativeViewAccessible
    NativeTabContentsContainerAura::GetNativeViewAccessible() {
  // TODO(beng):
  NOTIMPLEMENTED();
  return View::GetNativeViewAccessible();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTabContentsContainer, public:

// static
NativeTabContentsContainer* NativeTabContentsContainer::CreateNativeContainer(
    TabContentsContainer* container) {
  return new NativeTabContentsContainerViews(container);
  // TODO(beng): return new NativeTabContentsContainerAura(container);
}
