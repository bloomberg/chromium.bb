// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/adaptive/secondary_toolbar_view_controller.h"

#import "ios/chrome/browser/ui/toolbar/adaptive/secondary_toolbar_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface SecondaryToolbarViewController ()
@property(nonatomic, strong) SecondaryToolbarView* view;
@end

@implementation SecondaryToolbarViewController

@dynamic view;

#pragma mark - UIViewController

- (void)loadView {
  self.view = [[SecondaryToolbarView alloc] init];
  self.view.buttonFactory = self.buttonFactory;
}

@end
