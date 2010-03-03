// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_frame_chromeos.h"

#include "chrome/browser/chromeos/frame/normal_browser_frame_view.h"
#include "chrome/browser/views/frame/browser_view.h"

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  chromeos::BrowserFrameChromeos* frame =
      new chromeos::BrowserFrameChromeos(browser_view, profile);
  frame->Init();
  return frame;
}

namespace chromeos {

BrowserFrameChromeos::BrowserFrameChromeos(
    BrowserView* browser_view, Profile* profile)
    : BrowserFrameGtk(browser_view, profile) {
}

BrowserFrameChromeos::~BrowserFrameChromeos() {
}

void BrowserFrameChromeos::Init() {
  // Excludes a browser intance that requires icon/title. This is typically true
  // for dev tools and javascript console.
  // TODO(oshima): handle app panels. This currently uses the default
  // implementation, which opens Chrome's app panel instead of
  // ChromeOS's panel.
  if (!IsPanel() &&
      !browser_view()->ShouldShowWindowIcon() &&
      !browser_view()->ShouldShowWindowTitle()) {
    set_browser_frame_view(new NormalBrowserFrameView(this, browser_view()));
  }
  BrowserFrameGtk::Init();
}

bool BrowserFrameChromeos::IsMaximized() const {
  return !IsPanel() || WindowGtk::IsMaximized();
}

bool BrowserFrameChromeos::IsPanel() const {
  return browser_view()->IsBrowserTypePanel() ||
      browser_view()->IsBrowserTypePopup();
}

}  // namespace chromeos
