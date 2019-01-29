// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/infobars/infobar_container_view_controller.h"

#include "base/ios/block_types.h"
#include "base/logging.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// TODO(crbug.com/1372916): PLACEHOLDER Work in Progress class for the new
// InfobarUI.
@interface InfobarContainerViewController ()

@property(nonatomic, strong) NSMutableArray<UIViewController*>* currentInfobars;

@end

@implementation InfobarContainerViewController

#pragma mark - UIViewController

- (void)viewDidLoad {
  self.view = [[UIView alloc] initWithFrame:CGRectZero];
}

#pragma mark - InfobarConsumer

- (void)addInfoBarWithDelegate:(id<InfobarUIDelegate>)infoBarDelegate
                      position:(NSInteger)position {
  UIViewController* infobarViewController =
      static_cast<UIViewController*>(infoBarDelegate);

  [self addChildViewController:infobarViewController];
  [self.view addSubview:infobarViewController.view];
  [infobarViewController didMoveToParentViewController:self];
  infobarViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  [NSLayoutConstraint activateConstraints:@[
    [infobarViewController.view.topAnchor
        constraintEqualToAnchor:self.view.topAnchor
                       constant:10],
    [infobarViewController.view.bottomAnchor
        constraintEqualToAnchor:self.view.bottomAnchor],
    [infobarViewController.view.leadingAnchor
        constraintEqualToAnchor:self.view.leadingAnchor],
    [infobarViewController.view.trailingAnchor
        constraintEqualToAnchor:self.view.trailingAnchor]
  ]];

  if (![self.currentInfobars count]) {
    [self.presenter presentInfobarContainer];
  }

  [self.currentInfobars addObject:infobarViewController];
}

- (void)setUserInteractionEnabled:(BOOL)enabled {
  [self.view setUserInteractionEnabled:enabled];
}

- (void)updateLayoutAnimated:(BOOL)animated {
  // NO-OP - This shouldn't be needed in the new UI since we use autolayout for
  // the contained Infobars.
}

#pragma mark - Private Methods

- (NSMutableArray<UIViewController*>*)currentInfobars {
  if (!_currentInfobars) {
    _currentInfobars = [[NSMutableArray alloc] init];
  }
  return _currentInfobars;
}

@end
