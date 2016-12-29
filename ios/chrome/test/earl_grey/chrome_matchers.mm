// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/test/earl_grey/chrome_matchers.h"

#import <OCHamcrest/OCHamcrest.h>

#import <WebKit/WebKit.h>

#include "base/mac/foundation_util.h"
#include "base/strings/sys_string_conversions.h"
#import "base/test/ios/wait_util.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/collection_view/cells/collection_view_switch_item.h"
#import "ios/chrome/browser/ui/omnibox/omnibox_text_field_ios.h"
#import "ios/chrome/browser/ui/static_content/static_html_view_controller.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/block_types.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Script that returns document.body as a string.
NSString* const kGetDocumentBodyJavaScript =
    @"document.body ? document.body.textContent : null";

// Synchronously returns the result of executed JavaScript.
id ExecuteScriptInStaticController(
    StaticHtmlViewController* html_view_controller,
    NSString* script) {
  __block id result = nil;
  __block bool did_finish = false;
  web::JavaScriptResultBlock completion_handler =
      ^(id script_result, NSError* error) {
        result = [script_result copy];
        did_finish = true;
      };
  [html_view_controller executeJavaScript:script
                        completionHandler:completion_handler];

  GREYAssert(
      testing::WaitUntilConditionOrTimeout(testing::kWaitForJSCompletionTimeout,
                                           ^{
                                             return did_finish;
                                           }),
      @"JavaScript did not complete");

  return [result autorelease];
}

id<GREYMatcher> webViewWithNavDelegateOfClass(Class cls) {
  MatchesBlock matches = ^BOOL(UIView* view) {
    return [view isKindOfClass:[WKWebView class]] &&
           [base::mac::ObjCCast<WKWebView>(view).navigationDelegate
               isKindOfClass:cls];
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"web view with "];
    [description appendText:NSStringFromClass(cls)];
    [description appendText:@"navigation delegate"];
  };

  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

id<GREYMatcher> collectionViewSwitchIsOn(BOOL isOn) {
  MatchesBlock matches = ^BOOL(id element) {
    CollectionViewSwitchCell* switchCell =
        base::mac::ObjCCastStrict<CollectionViewSwitchCell>(element);
    UISwitch* switchView = switchCell.switchView;
    return (switchView.on && isOn) || (!switchView.on && !isOn);
  };
  DescribeToBlock describe = ^void(id<GREYDescription> description) {
    NSString* name =
        [NSString stringWithFormat:@"collectionViewSwitchInState(%@)",
                                   isOn ? @"ON" : @"OFF"];
    [description appendText:name];
  };
  return [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                               descriptionBlock:describe]
      autorelease];
}

}  // namespace

namespace chrome_test_util {

id<GREYMatcher> buttonWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> buttonWithAccessibilityLabelId(int message_id) {
  return buttonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> staticTextWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

id<GREYMatcher> staticTextWithAccessibilityLabelId(int message_id) {
  return staticTextWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> webViewBelongingToWebController() {
  return webViewWithNavDelegateOfClass(NSClassFromString(@"CRWWebController"));
}

id<GREYMatcher> webViewContainingText(std::string text) {
  return web::webViewContainingText(std::move(text), GetCurrentWebState());
}

id<GREYMatcher> webViewNotContainingText(std::string text) {
  return web::webViewNotContainingText(std::move(text), GetCurrentWebState());
}

id<GREYMatcher> staticHtmlViewContainingText(NSString* text) {
  // The WKWebView in a static HTML view isn't part of a webState, but it
  // does have the StaticHtmlViewController as its navigation delegate.
  MatchesBlock matches = ^BOOL(WKWebView* webView) {
    StaticHtmlViewController* html_view_controller =
        base::mac::ObjCCast<StaticHtmlViewController>(
            webView.navigationDelegate);

    __block BOOL did_succeed = NO;
    NSDate* deadline =
        [NSDate dateWithTimeIntervalSinceNow:testing::kWaitForUIElementTimeout];
    while (([[NSDate date] compare:deadline] != NSOrderedDescending) &&
           !did_succeed) {
      id result = ExecuteScriptInStaticController(html_view_controller,
                                                  kGetDocumentBodyJavaScript);
      if ([result isKindOfClass:[NSString class]]) {
        NSString* body = base::mac::ObjCCast<NSString>(result);
        did_succeed = [body containsString:text];
      }
      base::test::ios::SpinRunLoopWithMaxDelay(
          base::TimeDelta::FromSecondsD(testing::kSpinDelaySeconds));
    }
    return did_succeed;
  };

  DescribeToBlock describe = ^(id<GREYDescription> description) {
    [description appendText:@"static HTML web view containing "];
    [description appendText:text];
  };

  return grey_allOf(
      webViewWithNavDelegateOfClass([StaticHtmlViewController class]),
      [[[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                            descriptionBlock:describe]
          autorelease],
      nil);
}

id<GREYMatcher> forwardButton() {
  return buttonWithAccessibilityLabelId(IDS_ACCNAME_FORWARD);
}

id<GREYMatcher> backButton() {
  return buttonWithAccessibilityLabelId(IDS_ACCNAME_BACK);
}

id<GREYMatcher> reloadButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_ACCNAME_RELOAD);
}

id<GREYMatcher> stopButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_ACCNAME_STOP);
}

id<GREYMatcher> omnibox() {
  return grey_kindOfClass([OmniboxTextFieldIOS class]);
}

id<GREYMatcher> pageSecurityInfoButton() {
  return grey_accessibilityLabel(@"Page Security Info");
}

id<GREYMatcher> omniboxText(std::string text) {
  return grey_allOf(omnibox(),
                    hasProperty(@"text", base::SysUTF8ToNSString(text)), nil);
}

id<GREYMatcher> toolsMenuButton() {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> shareButton() {
  return buttonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_SHARE);
}

id<GREYMatcher> showTabsButton() {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> collectionViewSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL isOn) {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    collectionViewSwitchIsOn(isOn), grey_sufficientlyVisible(),
                    nil);
}

}  // namespace chrome_test_util
