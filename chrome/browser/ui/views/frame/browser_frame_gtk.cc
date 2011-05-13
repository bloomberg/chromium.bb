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

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font *title_font = new gfx::Font();
  return *title_font;
}

BrowserFrameGtk::BrowserFrameGtk(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::WindowGtk(browser_frame),
      browser_view_(browser_view) {
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameGtk, NativeBrowserFrame implementation:

views::NativeWindow* BrowserFrameGtk::AsNativeWindow() {
  return this;
}

const views::NativeWindow* BrowserFrameGtk::AsNativeWindow() const {
  return this;
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

void BrowserFrameGtk::TabStripDisplayModeChanged() {
  if (GetWidget()->GetRootView()->has_children()) {
    // Make sure the child of the root view gets Layout again.
    GetWidget()->GetRootView()->GetChildViewAt(0)->InvalidateLayout();
  }
  GetWidget()->GetRootView()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameGtk, WindowGtk overrides :

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


////////////////////////////////////////////////////////////////////////////////
// NativeBrowserFrame, public:

// static
NativeBrowserFrame* NativeBrowserFrame::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameGtk(browser_frame, browser_view);
}

