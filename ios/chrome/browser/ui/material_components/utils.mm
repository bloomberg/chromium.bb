// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/material_components/utils.h"

#import <UIKit/UIKit.h>

#import "ios/third_party/material_components_ios/src/components/AppBar/src/MaterialAppBar.h"
#import "ios/third_party/material_components_ios/src/components/FlexibleHeader/src/MaterialFlexibleHeader.h"
#import "ios/third_party/material_components_ios/src/components/NavigationBar/src/MaterialNavigationBar.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"

void ConfigureAppBarWithCardStyle(MDCAppBar* appBar) {
  appBar.headerViewController.headerView.canOverExtend = NO;
  appBar.headerViewController.headerView.shiftBehavior =
      MDCFlexibleHeaderShiftBehaviorDisabled;
  appBar.headerViewController.headerView.backgroundColor =
      [[MDCPalette greyPalette] tint200];
  appBar.navigationBar.tintColor = [[MDCPalette greyPalette] tint900];
  appBar.navigationBar.titleAlignment = MDCNavigationBarTitleAlignmentLeading;
}
