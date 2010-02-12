// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/browser_frame_chromeos.h"

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
  // TODO(oshima)::Create chromeos specific frame view.
  BrowserFrameGtk::Init();
}

}  // namespace chromeos
