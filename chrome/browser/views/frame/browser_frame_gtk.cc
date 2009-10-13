// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/frame/browser_frame_gtk.h"

#include "base/logging.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/frame/browser_root_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/opaque_browser_frame_view.h"
#include "views/widget/root_view.h"

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  BrowserFrameGtk* frame = new BrowserFrameGtk(browser_view, profile);
  frame->Init();
  return frame;
}

BrowserFrameGtk::BrowserFrameGtk(BrowserView* browser_view, Profile* profile)
    : WindowGtk(browser_view),
      browser_view_(browser_view),
      browser_frame_view_(NULL),
      root_view_(NULL),
      profile_(profile) {
  browser_view_->set_frame(this);
  browser_frame_view_ = new OpaqueBrowserFrameView(this, browser_view_);
  GetNonClientView()->SetFrameView(browser_frame_view_);
  // Don't focus anything on creation, selecting a tab will set the focus.
}

BrowserFrameGtk::~BrowserFrameGtk() {
}

void BrowserFrameGtk::Init() {
  WindowGtk::Init(NULL, gfx::Rect());
}

views::Window* BrowserFrameGtk::GetWindow() {
  return this;
}

void BrowserFrameGtk::TabStripCreated(TabStripWrapper* tabstrip) {
}

int BrowserFrameGtk::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

gfx::Rect BrowserFrameGtk::GetBoundsForTabStrip(
    TabStripWrapper* tabstrip) const {
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

void BrowserFrameGtk::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

void BrowserFrameGtk::ContinueDraggingDetachedTab() {
  NOTIMPLEMENTED();
}

ThemeProvider* BrowserFrameGtk::GetThemeProviderForFrame() const {
  // This is implemented for a different interface than GetThemeProvider is,
  // but they mean the same things.
  return GetThemeProvider();
}

bool BrowserFrameGtk::AlwaysUseNativeFrame() const {
  return false;
}

ThemeProvider* BrowserFrameGtk::GetThemeProvider() const {
  return profile_->GetThemeProvider();
}

ThemeProvider* BrowserFrameGtk::GetDefaultThemeProvider() const {
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

bool BrowserFrameGtk::GetAccelerator(int cmd_id,
                                     views::Accelerator* accelerator) {
  return browser_view_->GetAccelerator(cmd_id, accelerator);
}
