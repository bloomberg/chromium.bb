// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#import "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/ios/wait_util.h"
#include "ios/testing/earl_grey/wait_util.h"
#import "ios/web/public/web_state/web_state.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"
#import "ios/web/shell/view_controller.h"

namespace web {

id<GREYMatcher> webViewContainingText(const std::string& text) {
  return [GREYMatchers matcherForWebViewContainingText:text];
}

id<GREYMatcher> addressFieldText(const std::string& text) {
  return [GREYMatchers matcherForAddressFieldEqualToText:text];
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

+ (id<GREYMatcher>)matcherForWebViewContainingText:(const std::string&)text {
  web::WebState* webState = web::shell_test_util::GetCurrentWebState();
  return web::webViewContainingText(text, webState);
}

+ (id<GREYMatcher>)matcherForAddressFieldEqualToText:(const std::string&)text {
  MatchesBlock matches = ^BOOL(UIView* view) {
    if (![view isKindOfClass:[UITextField class]]) {
      return NO;
    }
    if (![[view accessibilityLabel]
            isEqualToString:kWebShellAddressFieldAccessibilityLabel]) {
      return NO;
    }
    UITextField* textField = base::mac::ObjCCastStrict<UITextField>(view);
    testing::WaitUntilCondition(testing::kWaitForUIElementTimeout, ^bool() {
      return [textField.text isEqualToString:base::SysUTF8ToNSString(text)];
    });
    return YES;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"address field containing "];
    [description appendText:base::SysUTF8ToNSString(text)];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
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
