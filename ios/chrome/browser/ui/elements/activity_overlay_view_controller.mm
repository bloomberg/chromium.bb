// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/elements/activity_overlay_view_controller.h"

#import "ios/chrome/browser/ui/material_components/activity_indicator.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#import "ios/third_party/material_components_ios/src/components/ActivityIndicator/src/MaterialActivityIndicator.h"

namespace {
// Size of the activity indicator.
const CGFloat kActivityIndicatorSize = 48;
// Alpha of the presented view's background.
const CGFloat kBackgroundAlpha = 0.5;
}

@implementation ActivityOverlayViewController

- (void)viewDidLoad {
  [super viewDidLoad];
  self.view.backgroundColor = [UIColor colorWithWhite:0 alpha:kBackgroundAlpha];

  MDCActivityIndicator* activityIndicator = [[[MDCActivityIndicator alloc]
      initWithFrame:CGRectMake(0, 0, kActivityIndicatorSize,
                               kActivityIndicatorSize)] autorelease];
  activityIndicator.translatesAutoresizingMaskIntoConstraints = NO;
  [self.view addSubview:activityIndicator];
  AddSameCenterConstraints(self.view, activityIndicator);
  activityIndicator.cycleColors = ActivityIndicatorBrandedCycleColors();
  [activityIndicator startAnimating];
}

@end
