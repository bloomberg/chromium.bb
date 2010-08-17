// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_frame_chromeos.h"

#include "chrome/browser/chromeos/frame/normal_browser_frame_view.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/browser/views/frame/opaque_browser_frame_view.h"
#include "chrome/browser/views/frame/popup_non_client_frame_view.h"

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
  // NOTE: This logic supersedes the logic in BrowserFrameGtk::Init()
  // by always setting browser_frame_view_.
  if (IsPanel()) {
    // ChromeOS Panels should always use PopupNonClientFrameView.
    set_browser_frame_view(new PopupNonClientFrameView());
  } else if (!browser_view()->ShouldShowWindowIcon() &&
             !browser_view()->ShouldShowWindowTitle()) {
    // Excludes a browser intance that requires icon/title.
    // This is typically true for dev tools and javascript console.
    set_browser_frame_view(new NormalBrowserFrameView(this, browser_view()));
  } else {
    // Default FrameView.
    set_browser_frame_view(new OpaqueBrowserFrameView(this, browser_view()));
  }

  BrowserFrameGtk::Init();

  if (!IsPanel()) {
    // On chromeos we want windows to always render as active.
    GetNonClientView()->DisableInactiveRendering(true);
  }
}

bool BrowserFrameChromeos::IsMaximized() const {
  return !IsPanel() || WindowGtk::IsMaximized();
}

bool BrowserFrameChromeos::IsPanel() const {
  return browser_view()->IsBrowserTypePanel() ||
      browser_view()->IsBrowserTypePopup();
}

}  // namespace chromeos
