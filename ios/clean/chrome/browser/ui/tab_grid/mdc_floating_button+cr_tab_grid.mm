// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/clean/chrome/browser/ui/tab_grid/mdc_floating_button+cr_tab_grid.h"

#import "ios/chrome/browser/ui/colors/MDCPalette+CrAdditions.h"
#include "ios/chrome/browser/ui/rtl_geometry.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/clean/chrome/browser/ui/actions/tab_grid_actions.h"
#import "ios/third_party/material_components_ios/src/components/Palettes/src/MaterialPalettes.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {
// Margin from trailing and bottom edges.
const CGFloat kNewTabButtonMarginFromEdges = 48;

// Diameter of circular button.
const CGFloat kNewTabButtonDiameter = 48;
}

@implementation MDCFloatingButton (CRTabGrid)

+ (instancetype)cr_tabGridNewTabButton {
  MDCFloatingButton* button = [[self alloc] init];
  if (button) {
    UIImage* openNewTabButtonImage =
        [[UIImage imageNamed:@"tabswitcher_new_tab_fab"]
            imageWithRenderingMode:UIImageRenderingModeAlwaysTemplate];
    [button setImage:openNewTabButtonImage forState:UIControlStateNormal];
    [[button imageView] setTintColor:[UIColor whiteColor]];
    [button setAutoresizingMask:UIViewAutoresizingFlexibleTopMargin |
                                UIViewAutoresizingFlexibleLeadingMargin()];

    MDCPalette* palette = [MDCPalette cr_bluePalette];
    [button setInkColor:[[palette tint300] colorWithAlphaComponent:0.5f]];
    [button setBackgroundColor:[palette tint500] forState:UIControlStateNormal];
    [button setBackgroundColor:[UIColor colorWithWhite:0.8f alpha:1.0f]
                      forState:UIControlStateDisabled];
    [button setAccessibilityLabel:l10n_util::GetNSString(
                                      IDS_IOS_TAB_SWITCHER_CREATE_NEW_TAB)];
    [button addTarget:nil
                  action:@selector(createNewTab:)
        forControlEvents:UIControlEventTouchUpInside];
  }
  return button;
}

+ (CGRect)cr_frameForTabGridNewTabButtonInRect:(CGRect)rect {
  CGFloat yOrigin = CGRectGetHeight(rect) - kNewTabButtonMarginFromEdges -
                    kNewTabButtonDiameter;
  CGFloat leading = CGRectGetWidth(rect) - kNewTabButtonMarginFromEdges -
                    kNewTabButtonDiameter;
  LayoutRect buttonLayout =
      LayoutRectMake(leading, CGRectGetWidth(rect), yOrigin,
                     kNewTabButtonDiameter, kNewTabButtonDiameter);
  return LayoutRectGetRect(buttonLayout);
}

@end
