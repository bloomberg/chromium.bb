// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame_gtk.h"

#include "base/logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/browser_theme_provider.h"
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
    : WindowGtk(browser_view),
      browser_view_(browser_view),
      browser_frame_view_(NULL),
      root_view_(NULL),
      profile_(profile) {
  browser_view_->set_frame(this);
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

void BrowserFrameGtk::InitBrowserFrame() {
  if (browser_frame_view_ == NULL)
    browser_frame_view_ =
        browser::CreateBrowserNonClientFrameView(this, browser_view_);

  GetNonClientView()->SetFrameView(browser_frame_view_);
  WindowGtk::InitWindow(NULL, gfx::Rect());
  // Don't focus anything on creation, selecting a tab will set the focus.
}

views::Window* BrowserFrameGtk::GetWindow() {
  return this;
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

gfx::Rect BrowserFrameGtk::GetBoundsForTabStrip(views::View* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

int BrowserFrameGtk::GetHorizontalTabStripVerticalOffset(bool restored) const {
  return browser_frame_view_->GetHorizontalTabStripVerticalOffset(restored);
}

void BrowserFrameGtk::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

ThemeProvider* BrowserFrameGtk::GetThemeProviderForFrame() const {
  // This is implemented for a different interface than GetThemeProvider is,
  // but they mean the same things.
  return GetThemeProvider();
}

bool BrowserFrameGtk::AlwaysUseNativeFrame() const {
  return false;
}

views::View* BrowserFrameGtk::GetFrameView() const {
  return browser_frame_view_;
}

void BrowserFrameGtk::TabStripDisplayModeChanged() {
  if (GetRootView()->has_children()) {
    // Make sure the child of the root view gets Layout again.
    GetRootView()->GetChildViewAt(0)->InvalidateLayout();
  }
  GetRootView()->Layout();
}

ThemeProvider* BrowserFrameGtk::GetThemeProvider() const {
  return profile_->GetThemeProvider();
}

views::RootView* BrowserFrameGtk::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

void BrowserFrameGtk::IsActiveChanged() {
  GetRootView()->SchedulePaint();
  browser_view_->ActivationChanged(IsActive());
  views::WidgetGtk::IsActiveChanged();
}

void BrowserFrameGtk::SetInitialFocus() {
  browser_view_->RestoreFocus();
}

bool BrowserFrameGtk::GetAccelerator(int cmd_id,
                                     ui::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
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
