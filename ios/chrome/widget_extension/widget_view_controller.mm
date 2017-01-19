// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/widget_extension/widget_view_controller.h"

#import <NotificationCenter/NotificationCenter.h>

#import "ios/chrome/widget_extension/widget_view.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@interface WidgetViewController ()
@property(nonatomic, weak) WidgetView* widgetView;
@end

@implementation WidgetViewController

@synthesize widgetView = _widgetView;

#pragma mark - UIViewController

- (void)viewDidLoad {
  [super viewDidLoad];

  // A local variable is necessary here as the property is declared weak and the
  // object would be deallocated before being retained by the addSubview call.
  WidgetView* widgetView = [[WidgetView alloc] init];
  self.widgetView = widgetView;
  [self.view addSubview:self.widgetView];

  [self.widgetView setTranslatesAutoresizingMaskIntoConstraints:NO];
  [NSLayoutConstraint activateConstraints:@[
    [self.widgetView.leadingAnchor
        constraintEqualToAnchor:[self.view leadingAnchor]],
    [self.widgetView.trailingAnchor
        constraintEqualToAnchor:[self.view trailingAnchor]],
    [self.widgetView.heightAnchor
        constraintEqualToAnchor:[self.view heightAnchor]],
    [self.widgetView.widthAnchor
        constraintEqualToAnchor:[self.view widthAnchor]]
  ]];
}

@end
