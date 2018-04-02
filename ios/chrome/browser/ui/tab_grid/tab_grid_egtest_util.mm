// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/ui/tab_grid/tab_grid_egtest_util.h"

#import <EarlGrey/EarlGrey.h>

#import "ios/chrome/browser/ui/tab_grid/tab_grid_constants.h"
#import "ios/chrome/browser/ui/tools_menu/public/tools_menu_constants.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace chrome_test_util {

id<GREYMatcher> TabGridOpenButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TAB_STRIP_ENTER_TAB_SWITCHER);
}

id<GREYMatcher> TabGridDoneButton() {
  return grey_allOf(grey_accessibilityID(kTabGridDoneButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridNewTabButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_TAB_GRID_CREATE_NEW_TAB),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridNewIncognitoTabButton() {
  return grey_allOf(
      ButtonWithAccessibilityLabelId(IDS_IOS_TAB_GRID_CREATE_NEW_INCOGNITO_TAB),
      grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> TabGridOpenTabsPanelButton() {
  return grey_accessibilityID(kTabGridRegularTabsPageButtonIdentifier);
}

id<GREYMatcher> TabGridIncognitoTabsPanelButton() {
  return grey_accessibilityID(kTabGridIncognitoTabsPageButtonIdentifier);
}

id<GREYMatcher> TabGridOtherDevicesPanelButton() {
  return grey_accessibilityID(kTabGridRemoteTabsPageButtonIdentifier);
}

}  // namespace chrome_test_util
