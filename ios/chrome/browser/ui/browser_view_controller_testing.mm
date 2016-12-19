// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/browser_view_controller_testing.h"

#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller.h"
#import "ios/chrome/browser/ui/toolbar/web_toolbar_controller_private.h"

@interface BrowserViewController (Private)

- (WebToolbarController*)toolbarController;

@end

@implementation BrowserViewController (TestingAdditions)

- (BOOL)testing_isLoading {
  return [[self toolbarController] isLoading];
}

- (void)testing_focusOmnibox {
  [[self toolbarController] focusOmnibox];
}

- (CGRect)testing_shareButtonAnchorRect {
  return [[self toolbarController] shareButtonAnchorRect];
}

@end
