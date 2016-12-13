// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/commands/UIKit+ChromeExecuteCommand.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIResponder (ChromeExecuteCommand)

- (void)chromeExecuteCommand:(id)sender {
  [[self nextResponder] chromeExecuteCommand:sender];
}

@end

@implementation UIWindow (ChromeExecuteCommand)

- (void)chromeExecuteCommand:(id)sender {
  id delegate = [[UIApplication sharedApplication] delegate];

  if ([delegate respondsToSelector:@selector(chromeExecuteCommand:)])
    [delegate chromeExecuteCommand:sender];
}

@end
