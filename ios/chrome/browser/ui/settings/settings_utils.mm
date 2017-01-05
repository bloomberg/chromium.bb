// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/settings/settings_utils.h"

#import "base/ios/weak_nsobject.h"
#import "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"
#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"
#include "ios/chrome/browser/ui/commands/ios_command_ids.h"
#import "ios/chrome/browser/ui/commands/open_url_command.h"

namespace ios_internal_settings {

ProceduralBlockWithURL BlockToOpenURL(UIResponder* responder) {
  base::WeakNSObject<UIResponder> weakResponder(responder);
  ProceduralBlockWithURL blockToOpenURL = ^(const GURL& url) {
    base::scoped_nsobject<UIResponder> strongResponder([weakResponder retain]);
    if (!strongResponder)
      return;
    base::scoped_nsobject<OpenUrlCommand> command(
        [[OpenUrlCommand alloc] initWithURLFromChrome:url]);
    [command setTag:IDC_CLOSE_SETTINGS_AND_OPEN_URL];
    [strongResponder chromeExecuteCommand:command];
  };
  return [[blockToOpenURL copy] autorelease];
}

}  // namespace ios_internal_settings
