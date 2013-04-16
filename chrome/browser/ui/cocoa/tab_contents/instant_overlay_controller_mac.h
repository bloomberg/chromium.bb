// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/search/instant_overlay_controller.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Browser;
@class BrowserWindowController;
@class OverlayableContentsController;

class InstantOverlayControllerMac : public InstantOverlayController,
                                    public content::NotificationObserver {
 public:
  InstantOverlayControllerMac(Browser* browser,
                              BrowserWindowController* window,
                              OverlayableContentsController* overlay);
  virtual ~InstantOverlayControllerMac();

 private:
  // InstantOverlayController:
  virtual void OverlayStateChanged(const InstantOverlayModel& model) OVERRIDE;

  // content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  BrowserWindowController* const window_;
  OverlayableContentsController* const overlay_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(InstantOverlayControllerMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_OVERLAY_CONTROLLER_MAC_H_
