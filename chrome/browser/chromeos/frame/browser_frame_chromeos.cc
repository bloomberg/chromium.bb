// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
  BrowserFrameGtk::InitBrowserFrame();

  if (!browser_view()->IsBrowserTypePopup() &&
      !browser_view()->IsBrowserTypePanel()) {
    // On chromeos we want windows to always render as active.
    DisableInactiveRendering();
  }
}

bool BrowserFrameChromeos::IsMaximized() const {
  if (CommandLine::ForCurrentProcess()->HasSwitch(switches::kChromeosFrame))
    return NativeWindowGtk::IsMaximized();
  bool is_popup_or_panel = browser_view()->IsBrowserTypePopup() ||
                           browser_view()->IsBrowserTypePanel();
  return !IsFullscreen() &&
      (!is_popup_or_panel || NativeWindowGtk::IsMaximized());
}

}  // namespace chromeos
