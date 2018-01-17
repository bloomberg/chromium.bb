// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_tab_grid_button.h"

#import "ios/chrome/browser/ui/toolbar/clean/toolbar_constants.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_base_feature.h"
#import "ios/chrome/browser/ui/toolbar/public/toolbar_controller_constants.h"
#import "ios/third_party/material_components_ios/src/components/Typography/src/MaterialTypography.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation ToolbarTabGridButton

- (void)setTabCount:(int)tabCount {
  // Update the text shown in the title of this button. Note that
  // the button's title may be empty or contain an easter egg, but the
  // accessibility value will always be equal to |tabCount|.
  NSString* tabStripButtonValue = [NSString stringWithFormat:@"%d", tabCount];
  NSString* tabStripButtonTitle;
  id<MDCTypographyFontLoading> fontLoader = [MDCTypography fontLoader];
  if (tabCount <= 0) {
    tabStripButtonTitle = @"";
  } else if (tabCount > kShowTabStripButtonMaxTabCount) {
    // As an easter egg, show a smiley face instead of the count if the user has
    // more than 99 tabs open.
    tabStripButtonTitle = @":)";
    self.titleLabel.font =
        [fontLoader boldFontOfSize:kFontSizeFewerThanTenTabs];
  } else {
    tabStripButtonTitle = tabStripButtonValue;
    if (tabCount < 10) {
      self.titleLabel.font =
          [fontLoader boldFontOfSize:kFontSizeFewerThanTenTabs];
    } else {
      self.titleLabel.font = [fontLoader boldFontOfSize:kFontSizeTenTabsOrMore];
    }
  }

  // TODO(crbug.com/799601): Delete this once its not needed.
  if (base::FeatureList::IsEnabled(kMemexTabSwitcher)) {
    tabStripButtonTitle = @"M";
    self.titleLabel.font =
        [fontLoader boldFontOfSize:kFontSizeFewerThanTenTabs];
  }

  [self setTitle:tabStripButtonTitle forState:UIControlStateNormal];
  [self setAccessibilityValue:tabStripButtonValue];
}

@end
