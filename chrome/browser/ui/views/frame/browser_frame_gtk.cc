// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_gtk.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/status_bubble.h"
#include "chrome/browser/ui/views/frame/app_panel_browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/gfx/font.h"
#include "views/widget/root_view.h"
#include "views/window/hit_test.h"

#if !defined(OS_CHROMEOS)
// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  BrowserFrameGtk* frame = new BrowserFrameGtk(browser_view, profile);
  frame->InitBrowserFrame();
  return frame;
}
#endif

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font *title_font = new gfx::Font();
  return *title_font;
}

BrowserFrameGtk::BrowserFrameGtk(BrowserView* browser_view, Profile* profile)
    : BrowserFrame(browser_view),
      WindowGtk(browser_view),
      ALLOW_THIS_IN_INITIALIZER_LIST(delegate_(this)),
      browser_view_(browser_view) {
  set_native_browser_frame(this);
  browser_view_->set_frame(this);
  non_client_view()->SetFrameView(CreateFrameViewForWindow());
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

void BrowserFrameGtk::InitBrowserFrame() {
  WindowGtk::InitWindow(NULL, gfx::Rect());
  // Don't focus anything on creation, selecting a tab will set the focus.
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameGtk, NativeBrowserFrame implementation:

views::NativeWindow* BrowserFrameGtk::AsNativeWindow() {
  return this;
}

const views::NativeWindow* BrowserFrameGtk::AsNativeWindow() const {
  return this;
}

BrowserNonClientFrameView* BrowserFrameGtk::CreateBrowserNonClientFrameView() {
  return browser::CreateBrowserNonClientFrameView(this, browser_view_);
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

ThemeProvider* BrowserFrameGtk::GetThemeProviderForFrame() const {
  // This is implemented for a different interface than GetThemeProvider is,
  // but they mean the same things.
  return GetThemeProvider();
}

bool BrowserFrameGtk::AlwaysUseNativeFrame() const {
  return false;
}

void BrowserFrameGtk::TabStripDisplayModeChanged() {
  if (GetRootView()->has_children()) {
    // Make sure the child of the root view gets Layout again.
    GetRootView()->GetChildViewAt(0)->InvalidateLayout();
  }
  GetRootView()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameGtk, WindowGtk overrides :

ThemeProvider* BrowserFrameGtk::GetThemeProvider() const {
  return ThemeServiceFactory::GetForProfile(
      browser_view_->browser()->profile());
}

void BrowserFrameGtk::SetInitialFocus() {
  browser_view_->RestoreFocus();
}

views::RootView* BrowserFrameGtk::CreateRootView() {
  return delegate_->DelegateCreateRootView();
}

bool BrowserFrameGtk::GetAccelerator(int cmd_id,
                                     ui::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}

views::NonClientFrameView* BrowserFrameGtk::CreateFrameViewForWindow() {
  return delegate_->DelegateCreateFrameViewForWindow();
}

gboolean BrowserFrameGtk::OnWindowStateEvent(GtkWidget* widget,
                                             GdkEventWindowState* event) {
  bool was_full_screen = IsFullscreen();
  gboolean result = views::WindowGtk::OnWindowStateEvent(widget, event);
  if ((!IsVisible() || IsMinimized()) && browser_view_->GetStatusBubble()) {
    // The window is effectively hidden. We have to hide the status bubble as
    // unlike windows gtk has no notion of child windows that are hidden along
    // with the parent.
    browser_view_->GetStatusBubble()->Hide();
  }
  if (was_full_screen != IsFullscreen())
    browser_view_->FullScreenStateChanged();
  return result;
}

gboolean BrowserFrameGtk::OnConfigureEvent(GtkWidget* widget,
                                           GdkEventConfigure* event) {
  browser_view_->WindowMoved();
  return views::WindowGtk::OnConfigureEvent(widget, event);
}
