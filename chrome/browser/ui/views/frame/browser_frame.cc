// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "views/widget/native_widget.h"
#include "views/widget/widget.h"
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
  return browser_frame_view_->GetBoundsForTabStrip(tabstrip);
}

int BrowserFrame::GetHorizontalTabStripVerticalOffset(bool restored) const {
  return browser_frame_view_->GetHorizontalTabStripVerticalOffset(restored);
}

void BrowserFrame::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

ui::ThemeProvider* BrowserFrame::GetThemeProviderForFrame() const {
  return native_browser_frame_->GetThemeProviderForFrame();
}

bool BrowserFrame::AlwaysUseNativeFrame() const {
  return native_browser_frame_->AlwaysUseNativeFrame();
}

views::View* BrowserFrame::GetFrameView() const {
  return browser_frame_view_;
}

void BrowserFrame::TabStripDisplayModeChanged() {
  native_browser_frame_->TabStripDisplayModeChanged();
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, NativeBrowserFrameDelegate implementation:

views::RootView* BrowserFrame::DelegateCreateRootView() {
  root_view_ = new BrowserRootView(
      browser_view_,
      native_browser_frame_->AsNativeWindow()->AsNativeWidget()->GetWidget());
  return root_view_;
}

views::NonClientFrameView* BrowserFrame::DelegateCreateFrameViewForWindow() {
  browser_frame_view_ =
      native_browser_frame_->CreateBrowserNonClientFrameView();
  return browser_frame_view_;
}


////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, protected:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : native_browser_frame_(NULL),
      root_view_(NULL),
      browser_frame_view_(NULL),
      browser_view_(browser_view) {
}
