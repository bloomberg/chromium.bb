// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/stack_view/stack_view_toolbar_controller.h"

#include "base/mac/foundation_util.h"
#include "base/metrics/user_metrics.h"
#include "base/metrics/user_metrics_action.h"
#import "ios/chrome/browser/ui/image_util.h"
#import "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/browser/ui/toolbar/new_tab_button.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"

using base::UserMetricsAction;

namespace {
const CGFloat kNewTabLeadingOffset = 10.0;
const int kBackgroundViewColor = 0x282C32;
const CGFloat kBackgroundViewColorAlpha = 0.95;
}

@implementation StackViewToolbarController {
  base::scoped_nsobject<UIView> _stackViewToolbar;
  base::scoped_nsobject<NewTabButton> _openNewTabButton;
}

- (instancetype)initWithStackViewToolbar {
  self = [super initWithStyle:ToolbarControllerStyleDarkMode];
  if (self) {
    _stackViewToolbar.reset(
        [[UIView alloc] initWithFrame:[self specificControlsArea]]);
    [_stackViewToolbar setAutoresizingMask:UIViewAutoresizingFlexibleHeight |
                                           UIViewAutoresizingFlexibleWidth];

    _openNewTabButton.reset([[NewTabButton alloc] initWithFrame:CGRectZero]);

    [_openNewTabButton
        setAutoresizingMask:UIViewAutoresizingFlexibleTopMargin |
                            UIViewAutoresizingFlexibleTrailingMargin()];
    CGFloat buttonSize = [_stackViewToolbar bounds].size.height;
    // Set the button's position.
    LayoutRect newTabButtonLayout = LayoutRectMake(
        kNewTabLeadingOffset, [_stackViewToolbar bounds].size.width, 0,
        buttonSize, buttonSize);
    [_openNewTabButton setFrame:LayoutRectGetRect(newTabButtonLayout)];
    // Set additional button action.
    [_openNewTabButton addTarget:self
                          action:@selector(recordUserMetrics:)
                forControlEvents:UIControlEventTouchUpInside];

    self.shadowView.alpha = 0.0;
    self.backgroundView.image = nil;
    self.backgroundView.backgroundColor =
        UIColorFromRGB(kBackgroundViewColor, kBackgroundViewColorAlpha);

    [_stackViewToolbar addSubview:_openNewTabButton];
    [self.view addSubview:_stackViewToolbar];
  }
  return self;
}

- (NewTabButton*)openNewTabButton {
  return _openNewTabButton.get();
}

- (IBAction)recordUserMetrics:(id)sender {
  if (sender == _openNewTabButton.get())
    base::RecordAction(UserMetricsAction("MobileToolbarStackViewNewTab"));
  else
    [super recordUserMetrics:sender];
}

@end
