// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/generic_chrome_command.h"

#import <UIKit/UIKit.h>

#include "base/logging.h"
#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation GenericChromeCommand

@synthesize tag = _tag;

+ (instancetype)commandWithTag:(NSInteger)tag {
  return [[self alloc] initWithTag:tag];
}

- (instancetype)initWithTag:(NSInteger)tag {
  self = [super init];
  if (self) {
    _tag = tag;
  }
  return self;
}

- (instancetype)init {
  NOTREACHED();
  return nil;
}

- (void)executeOnMainWindow {
  UIWindow* mainWindow = [[UIApplication sharedApplication] keyWindow];
  DCHECK(mainWindow);
  [mainWindow chromeExecuteCommand:self];
}

@end
