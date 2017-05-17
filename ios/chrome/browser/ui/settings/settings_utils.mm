// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_utils.h"

#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace ios_internal_settings {

ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder) {
  __weak UIResponder* weakResponder = responder;
  ProceduralBlockWithURL blockToOpenURL = ^(const GURL& url) {
    UIResponder* strongResponder = weakResponder;
    if (!strongResponder)
      return;
    OpenUrlCommand* command =
        [[OpenUrlCommand alloc] initWithURLFromChrome:url];
    [command setTag:IDC_CLOSE_SETTINGS_AND_OPEN_URL];
    [strongResponder chromeExecuteCommand:command];
  };
  return [blockToOpenURL copy];
}

}  // namespace ios_internal_settings
