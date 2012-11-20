// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_PREVIEW_CONTROLLER_MAC_H_
#define CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_PREVIEW_CONTROLLER_MAC_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/instant/instant_preview_controller.h"

class Browser;
@class BrowserWindowController;
@class PreviewableContentsController;

class InstantPreviewControllerMac : public InstantPreviewController {
 public:
  InstantPreviewControllerMac(Browser* browser,
                              BrowserWindowController* window,
                              PreviewableContentsController* preview);
  virtual ~InstantPreviewControllerMac();

 private:
  // Overridden from InstantPreviewController:
  virtual void PreviewStateChanged(const InstantModel& model) OVERRIDE;

  BrowserWindowController* const window_;
  PreviewableContentsController* const preview_;

  DISALLOW_COPY_AND_ASSIGN(InstantPreviewControllerMac);
};

#endif  // CHROME_BROWSER_UI_COCOA_TAB_CONTENTS_INSTANT_PREVIEW_CONTROLLER_MAC_H_
