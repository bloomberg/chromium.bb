// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/ui_button+cr_tab_grid.h"

#import "ios/clean/chrome/browser/ui/actions/settings_actions.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation UIButton (CRTabGrid)

+ (instancetype)cr_tabGridToolbarDoneButton {
  UIButton* button = [self buttonWithType:UIButtonTypeSystem];
  [button.titleLabel setFont:[MDCTypography buttonFont]];
  [button setTitle:@"Done" forState:UIControlStateNormal];
  [button setTintColor:[UIColor whiteColor]];
  return button;
}

+ (instancetype)cr_tabGridToolbarIncognitoButton {
  UIButton* button = [self buttonWithType:UIButtonTypeSystem];
  UIImage* image = [[UIImage imageNamed:@"tabswitcher_incognito"]
      imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
  [button setImage:image forState:UIControlStateNormal];
  [button setTintColor:[UIColor whiteColor]];
  return button;
}

+ (instancetype)cr_tabGridToolbarMenuButton {
  UIButton* button = [self buttonWithType:UIButtonTypeSystem];
  [button setImage:[UIImage imageNamed:@"tabswitcher_menu"]
          forState:UIControlStateNormal];
  [button setTintColor:[UIColor whiteColor]];
  [button addTarget:nil
                action:@selector(showSettings:)
      forControlEvents:UIControlEventTouchUpInside];
  return button;
}

@end
