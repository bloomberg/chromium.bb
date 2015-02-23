// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"

@implementation GenericChromeCommand {
  NSInteger _tag;
}

@synthesize tag = _tag;

- (instancetype)init {
  return [self initWithTag:0];
}

- (instancetype)initWithTag:(NSInteger)tag {
  self = [super init];
  if (self) {
    _tag = tag;
  }
  return self;
}

- (void)executeOnMainWindow {
  UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
  DCHECK(mainWindow);
  [mainWindow chromeExecuteCommand:self];
}

@end
