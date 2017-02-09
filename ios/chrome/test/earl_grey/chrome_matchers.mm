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

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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

  return result;
}

// TODO(crbug.com/684142): This matcher uses too many implementation details,
// it would be good to replace it.
id<GREYMatcher> WebViewWithNavDelegateOfClass(Class cls) {
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

  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

id<GREYMatcher> CollectionViewSwitchIsOn(BOOL isOn) {
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
  return [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                              descriptionBlock:describe];
}

}  // namespace

namespace chrome_test_util {

id<GREYMatcher> ButtonWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
}

id<GREYMatcher> ButtonWithAccessibilityLabelId(int message_id) {
  return ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> StaticTextWithAccessibilityLabel(NSString* label) {
  return grey_allOf(grey_accessibilityLabel(label),
                    grey_accessibilityTrait(UIAccessibilityTraitStaticText),
                    nil);
}

id<GREYMatcher> StaticTextWithAccessibilityLabelId(int message_id) {
  return StaticTextWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(message_id));
}

id<GREYMatcher> WebViewContainingText(std::string text) {
  return web::WebViewContainingText(std::move(text), GetCurrentWebState());
}

id<GREYMatcher> WebViewNotContainingText(std::string text) {
  return web::WebViewNotContainingText(std::move(text), GetCurrentWebState());
}

id<GREYMatcher> StaticHtmlViewContainingText(NSString* text) {
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
      WebViewWithNavDelegateOfClass([StaticHtmlViewController class]),
      [[GREYElementMatcherBlock alloc] initWithMatchesBlock:matches
                                           descriptionBlock:describe],
      nil);
}

id<GREYMatcher> WebViewContainingBlockedImage(std::string image_id) {
  return web::WebViewContainingBlockedImage(
      std::move(image_id), chrome_test_util::GetCurrentWebState());
}

id<GREYMatcher> WebViewContainingLoadedImage(std::string image_id) {
  return web::WebViewContainingLoadedImage(
      std::move(image_id), chrome_test_util::GetCurrentWebState());
}

id<GREYMatcher> CancelButton() {
  return ButtonWithAccessibilityLabelId(IDS_CANCEL);
}

id<GREYMatcher> ForwardButton() {
  return ButtonWithAccessibilityLabelId(IDS_ACCNAME_FORWARD);
}

id<GREYMatcher> BackButton() {
  return ButtonWithAccessibilityLabelId(IDS_ACCNAME_BACK);
}

id<GREYMatcher> ReloadButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_RELOAD);
}

id<GREYMatcher> StopButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_ACCNAME_STOP);
}

id<GREYMatcher> Omnibox() {
  return grey_kindOfClass([OmniboxTextFieldIOS class]);
}

id<GREYMatcher> PageSecurityInfoButton() {
  return grey_accessibilityLabel(@"Page Security Info");
}

id<GREYMatcher> OmniboxText(std::string text) {
  return grey_allOf(Omnibox(),
                    hasProperty(@"text", base::SysUTF8ToNSString(text)), nil);
}

id<GREYMatcher> ToolsMenuButton() {
  return grey_allOf(grey_accessibilityID(kToolbarToolsMenuButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> ShareButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_TOOLS_MENU_SHARE);
}

id<GREYMatcher> ShowTabsButton() {
  return grey_allOf(grey_accessibilityID(kToolbarStackButtonIdentifier),
                    grey_sufficientlyVisible(), nil);
}

id<GREYMatcher> CollectionViewSwitchCell(NSString* accessibilityIdentifier,
                                         BOOL isOn) {
  return grey_allOf(grey_accessibilityID(accessibilityIdentifier),
                    CollectionViewSwitchIsOn(isOn), grey_sufficientlyVisible(),
                    nil);
}

}  // namespace chrome_test_util
