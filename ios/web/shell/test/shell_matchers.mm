// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/shell_matchers.h"

#import "ios/web/shell/view_controller.h"

namespace web {

id<GREYMatcher> backButton() {
  return [GREYMatchers matcherForWebShellBackButton];
}

id<GREYMatcher> forwardButton() {
  return [GREYMatchers matcherForWebShellForwardButton];
}

id<GREYMatcher> addressField() {
  return [GREYMatchers matcherForWebShellAddressField];
}

}  // namespace web

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