// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/window/native_window.h"
#include "views/window/window.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::~BrowserFrame() {
}

views::Window* BrowserFrame::GetWindow() {
  return native_browser_frame_->AsNativeWindow()->GetWindow();
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  return native_browser_frame_->GetMinimizeButtonOffset();
}

gfx::Rect BrowserFrame::GetBoundsForTabStrip(views::View* tabstrip) const {
  return native_browser_frame_->GetBoundsForTabStrip(tabstrip);
}

int BrowserFrame::GetHorizontalTabStripVerticalOffset(bool restored) const {
  return native_browser_frame_->GetHorizontalTabStripVerticalOffset(restored);
}

void BrowserFrame::UpdateThrobber(bool running) {
  native_browser_frame_->UpdateThrobber(running);
}

ui::ThemeProvider* BrowserFrame::GetThemeProviderForFrame() const {
  return native_browser_frame_->GetThemeProviderForFrame();
}

bool BrowserFrame::AlwaysUseNativeFrame() const {
  return native_browser_frame_->AlwaysUseNativeFrame();
}

views::View* BrowserFrame::GetFrameView() const {
  return native_browser_frame_->GetFrameView();
}

void BrowserFrame::TabStripDisplayModeChanged() {
  native_browser_frame_->TabStripDisplayModeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, protected:

BrowserFrame::BrowserFrame() : native_browser_frame_(NULL) {
}
