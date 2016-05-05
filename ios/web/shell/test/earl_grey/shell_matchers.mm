// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"
#import "ios/web/shell/view_controller.h"

namespace web {

// Shorthand for GREYMatchers::matcherForWebViewContainingText.
id<GREYMatcher> webViewContainingText(NSString* text) {
  return [GREYMatchers matcherForWebViewContainingText:text];
}

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

+ (id<GREYMatcher>)matcherForWebViewContainingText:(NSString*)text {
  web::WebState* webState = web::web_shell_test_util::GetCurrentWebState();
  return web::webViewContainingText(text, webState);
}

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