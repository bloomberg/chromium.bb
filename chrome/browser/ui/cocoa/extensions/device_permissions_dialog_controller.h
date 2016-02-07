// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLER_H_
#define CHROME_BROWSER_UI_COCOA_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLER_H_

#import <Cocoa/Cocoa.h>

#include "base/mac/scoped_nsobject.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "extensions/browser/api/device_permissions_prompt.h"

namespace content {
class WebContents;
}

@class DevicePermissionsViewController;

// Displays an device permissions selector prompt as a modal sheet constrained
// to the window/tab displaying the given web contents.
class DevicePermissionsDialogController
    : public extensions::DevicePermissionsPrompt::Prompt::Observer,
      public ConstrainedWindowMacDelegate {
 public:
  DevicePermissionsDialogController(
      content::WebContents* web_contents,
      scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt);
  ~DevicePermissionsDialogController() override;

  void Dismissed();

  // extensions::DevicePermissionsPrompt::Prompt::Observer implementation.
  void OnDevicesChanged() override;

  // ConstrainedWindowMacDelegate implementation.
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

 private:
  scoped_refptr<extensions::DevicePermissionsPrompt::Prompt> prompt_;
  base::scoped_nsobject<DevicePermissionsViewController> view_controller_;
  scoped_ptr<ConstrainedWindowMac> constrained_window_;
};

#endif  // CHROME_BROWSER_UI_COCOA_EXTENSIONS_DEVICE_PERMISSIONS_DIALOG_CONTROLER_H_
