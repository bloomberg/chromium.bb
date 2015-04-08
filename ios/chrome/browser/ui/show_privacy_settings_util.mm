// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/ui/show_privacy_settings_util.h"

#include "base/mac/scoped_nsobject.h"
#include "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

void ShowClearBrowsingData() {
  base::scoped_nsobject<GenericChromeCommand> command(
      [[GenericChromeCommand alloc] init]);
  [command setTag:IDC_SHOW_PRIVACY_SETTINGS];
  UIWindow* main_window = [[UIApplication sharedApplication] keyWindow];
  [main_window chromeExecuteCommand:command];
}
