// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/shell_matchers.h"

#import "ios/web/shell/view_controller.h"

@implementation GREYMatchers (WebShellAdditions)

+ (id<GREYMatcher>)matcherForWebShellBackButton {
  return grey_accessibilityLabel(kWebShellBackButtonAccessibilityLabel);
}

+ (id<GREYMatcher>)matcherForWebShellForwardButton {
  return grey_accessibilityLabel(kWebShellForwardButtonAccessibilityLabel);
}

+ (id<GREYMatcher>)matcherForWebShellAddressField {
  return grey_accessibilityLabel(kWebShellAddressFieldAccessibilityLabel);
}

@end

#if !(GREY_DISABLE_SHORTHAND)

id<GREYMatcher> shell_backButton() {
  return [GREYMatchers matcherForWebShellBackButton];
}

id<GREYMatcher> shell_forwardButton() {
  return [GREYMatchers matcherForWebShellForwardButton];
}

id<GREYMatcher> shell_addressField() {
  return [GREYMatchers matcherForWebShellAddressField];
}

#endif