// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"

namespace content {
class WebContents;
}
@class OneClickSigninViewController;

// After the user signs into chrome this class is used to display a tab modal
// signin confirmation dialog.
class OneClickSigninDialogController : public ConstrainedWindowMacDelegate {
 public:
  OneClickSigninDialogController(
      content::WebContents* web_contents,
      const BrowserWindow::StartSyncCallback& sync_callback);
  virtual ~OneClickSigninDialogController();

  // ConstrainedWindowMacDelegate implementation.
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  ConstrainedWindowMac* constrained_window() const {
    return constrained_window_.get();
  }

  OneClickSigninViewController* view_controller() { return view_controller_; }

 private:
  void PerformClose();

  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  scoped_nsobject<OneClickSigninViewController> view_controller_;
};

#endif  // CHROME_BROWSER_UI_COCOA_ONE_CLICK_SIGNIN_DIALOG_CONTROLLER_H_
