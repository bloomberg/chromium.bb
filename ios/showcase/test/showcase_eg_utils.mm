// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/showcase/test/showcase_eg_utils.h"

#import "base/mac/foundation_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Matcher for the back button on screens presented from the Showcase home
// screen.
id<GREYMatcher> BackButton() {
  return grey_kindOfClass(
      NSClassFromString(@"_UINavigationBarBackIndicatorView"));
}

// Returns the Showcase navigation controller.
UINavigationController* ShowcaseNavigationController() {
  UINavigationController* showcaseNavigationController =
      base::mac::ObjCCastStrict<UINavigationController>(
          [[[UIApplication sharedApplication] keyWindow] rootViewController]);
  return showcaseNavigationController;
}

}  // namespace

namespace showcase_utils {

void Open(NSString* name) {
  [[[EarlGrey selectElementWithMatcher:grey_accessibilityLabel(name)]
         usingSearchAction:grey_scrollInDirection(kGREYDirectionDown, 50)
      onElementWithMatcher:grey_accessibilityID(@"showcase_home_collection")]
      performAction:grey_tap()];
}

void Close() {
  // Some screens hides the navigation bar. Make sure it is showing.
  ShowcaseNavigationController().navigationBarHidden = NO;
  [[EarlGrey selectElementWithMatcher:BackButton()] performAction:grey_tap()];
}

}  // namespace showcase_utils
