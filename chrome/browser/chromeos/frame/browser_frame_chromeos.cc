// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_frame_chromeos.h"

#include "base/command_line.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/common/chrome_switches.h"

// static (Factory method.)
BrowserFrame* BrowserFrame::Create(BrowserView* browser_view,
                                   Profile* profile) {
  chromeos::BrowserFrameChromeos* frame =
      new chromeos::BrowserFrameChromeos(browser_view, profile);
  frame->InitBrowserFrame();
  return frame;
}

namespace chromeos {

BrowserFrameChromeos::BrowserFrameChromeos(
    BrowserView* browser_view, Profile* profile)
    : BrowserFrameGtk(browser_view, profile) {
}

BrowserFrameChromeos::~BrowserFrameChromeos() {
}

void BrowserFrameChromeos::InitBrowserFrame() {
  // NOTE: This logic supersedes the logic in BrowserFrameGtk::Init()
  // by always setting browser_frame_view_.
  set_browser_frame_view(
      browser::CreateBrowserNonClientFrameView(this, browser_view()));

  BrowserFrameGtk::InitBrowserFrame();

  if (!browser_view()->IsBrowserTypePopup()) {
    // On chromeos we want windows to always render as active.
    DisableInactiveRendering();
  }
}

bool BrowserFrameChromeos::IsMaximized() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeosFrame))
    return WindowGtk::IsMaximized();
  bool is_popup = browser_view()->IsBrowserTypePopup();
  return !IsFullscreen() && (!is_popup || WindowGtk::IsMaximized());
}

}  // namespace chromeos
