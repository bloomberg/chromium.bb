// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/bookmarks/bookmark_navigation_controller.h"

#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/bookmarks/bookmark_utils_ios.h"

@implementation BookmarkNavigationController

- (id)initWithRootViewController:(UIViewController*)rootViewController {
  self = [super initWithRootViewController:rootViewController];
  if (self) {
    [self setNavigationBarHidden:YES];
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor whiteColor];
}

- (BOOL)disablesAutomaticKeyboardDismissal {
  // This allows us to hide the keyboard when controllers are being displayed in
  // a modal form sheet on the iPad.
  return NO;
}

#pragma mark - UIResponder

- (BOOL)canBecomeFirstResponder {
  return YES;
}

@end
