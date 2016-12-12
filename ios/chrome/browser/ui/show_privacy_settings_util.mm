// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/show_privacy_settings_util.h"

#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

void ShowClearBrowsingData() {
  GenericChromeCommand* command = [[GenericChromeCommand alloc]
      initWithTag:IDC_SHOW_CLEAR_BROWSING_DATA_SETTINGS];
  UIWindow* main_window = [[UIApplication sharedApplication] keyWindow];
  [main_window chromeExecuteCommand:command];
}
