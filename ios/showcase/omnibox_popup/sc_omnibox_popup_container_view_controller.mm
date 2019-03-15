// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/omnibox_popup/sc_omnibox_popup_container_view_controller.h"

#import "ios/chrome/browser/ui/omnibox/popup/omnibox_popup_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/buttons/toolbar_configuration.h"
#import "ios/chrome/common/ui_util/constraints_ui_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation SCOmniboxPopupContainerViewController

- (instancetype)initWithPopupViewController:
    (OmniboxPopupViewController*)popupViewController {
  self = [super initWithNibName:nil bundle:nil];
  if (self) {
    _popupViewController = popupViewController;
  }
  return self;
}

- (void)viewDidLoad {
  [super viewDidLoad];

  // Popup uses same colors as the toolbar, so the ToolbarConfiguration is
  // used to get the style.
  ToolbarConfiguration* configuration =
      [[ToolbarConfiguration alloc] initWithStyle:NORMAL];

  UIBlurEffect* effect = [configuration blurEffect];
  UIView* containerView;
  if (effect) {
    UIVisualEffectView* effectView =
        [[UIVisualEffectView alloc] initWithEffect:effect];
    [effectView.contentView addSubview:self.popupViewController.view];
    containerView = effectView;
  } else {
    containerView = [[UIView alloc] init];
    [containerView addSubview:self.popupViewController.view];
  }
  containerView.backgroundColor = [configuration blurBackgroundColor];
  containerView.translatesAutoresizingMaskIntoConstraints = NO;
  self.popupViewController.view.translatesAutoresizingMaskIntoConstraints = NO;
  AddSameConstraints(self.popupViewController.view, containerView);

  self.view.backgroundColor = UIColor.whiteColor;

  [self addChildViewController:self.popupViewController];
  [self.view addSubview:containerView];
  [self.popupViewController didMoveToParentViewController:self];

  AddSameConstraints(self.view, containerView);
}

@end
