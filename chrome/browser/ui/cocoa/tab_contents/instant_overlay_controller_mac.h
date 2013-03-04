// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_overlay_controller.h"

class Browser;
@class BrowserWindowController;
@class OverlayableContentsController;

class InstantOverlayControllerMac : public InstantOverlayController {
 public:
  InstantOverlayControllerMac(Browser* browser,
                              BrowserWindowController* window,
                              OverlayableContentsController* overlay);
  virtual ~InstantOverlayControllerMac();

 private:
  // Overridden from InstantOverlayController:
  virtual void OverlayStateChanged(const InstantOverlayModel& model) OVERRIDE;

  BrowserWindowController* const window_;
  OverlayableContentsController* const overlay_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlayControllerMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_
