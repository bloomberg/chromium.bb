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
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/font.h"
#include "ui/views/views_delegate.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

// static
const gfx::Font& BrowserFrame::GetTitleFont() {
  static gfx::Font *title_font = new gfx::Font();
  return *title_font;
}

BrowserFrameGtk::BrowserFrameGtk(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetGtk(browser_frame),
      browser_view_(browser_view) {
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameGtk, NativeBrowserFrame implementation:

views::NativeWidget* BrowserFrameGtk::AsNativeWidget() {
  return this;
}

const views::NativeWidget* BrowserFrameGtk::AsNativeWidget() const {
  return this;
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

void BrowserFrameGtk::TabStripDisplayModeChanged() {
  if (GetWidget()->GetRootView()->has_children()) {
    // Make sure the child of the root view gets Layout again.
    GetWidget()->GetRootView()->child_at(0)->InvalidateLayout();
  }
  GetWidget()->GetRootView()->Layout();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameGtk, NativeWidgetGtk overrides:

gboolean BrowserFrameGtk::OnWindowStateEvent(GtkWidget* widget,
                                             GdkEventWindowState* event) {
  bool was_full_screen = IsFullscreen();
  gboolean result = views::NativeWidgetGtk::OnWindowStateEvent(widget, event);
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
  return views::NativeWidgetGtk::OnConfigureEvent(widget, event);
}

////////////////////////////////////////////////////////////////////////////////
// NativeBrowserFrame, public:

// static
NativeBrowserFrame* NativeBrowserFrame::CreateNativeBrowserFrame(
    BrowserFrame* browser_frame,
    BrowserView* browser_view) {
  return new BrowserFrameGtk(browser_frame, browser_view);
}
