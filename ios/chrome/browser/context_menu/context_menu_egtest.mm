// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/ios_util.h"
#include "base/test/scoped_feature_list.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/histogram_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#import "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/features.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OmniboxText;
using chrome_test_util::OpenLinkInNewTabButton;

namespace {
const char kServerFilesDir[] = "ios/testing/data/http_server_files/";
const char kLogoPagePath[] = "/chromium_logo_page.html";
const char kLogoPageImageSourcePath[] = "/chromium_logo.png";

const char kUrlInitialPage[] = "/scenarioContextMenuOpenInNewTab";
const char kUrlDestinationPage[] = "/destination";
const char kChromiumImageID[] = "chromium_image";
const char kDestinationLinkID[] = "link";

// HTML content of the destination page that sets the page title.
const char kDestinationHtml[] =
    "<html><body><script>document.title='new doc'</script>You made it!"
    "</body></html>";

// Matcher for the open image button in the context menu.
id<GREYMatcher> OpenImageButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
}

// Matcher for the open image in new tab button in the context menu.
id<GREYMatcher> OpenImageInNewTabButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
}

// Provides responses for initial page and destination URLs.
std::unique_ptr<net::test_server::HttpResponse> StandardResponse(
    const net::test_server::HttpRequest& request) {
  std::unique_ptr<net::test_server::BasicHttpResponse> http_response =
      std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);

  if (request.relative_url == kUrlInitialPage) {
    // The initial page contains a link to the destination page.
    http_response->set_content(
        "<html><body><a style='margin-left:50px' href='" +
        std::string(kUrlDestinationPage) +
        "' id='link'>link</a></body></html>");
  } else if (request.relative_url == kUrlDestinationPage) {
    http_response->set_content(kDestinationHtml);
  } else {
    return nullptr;
  }

  return std::move(http_response);
}

// Waits for the context menu item to disappear. TODO(crbug.com/682871): Remove
// this once EarlGrey is synchronized with context menu.
void WaitForContextMenuItemDisappeared(
    id<GREYMatcher> context_menu_item_button) {
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey selectElementWithMatcher:context_menu_item_button]
        assertWithMatcher:grey_nil()
                    error:&error];
    return error == nil;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout, condition),
             @"Waiting for matcher %@ failed.", context_menu_item_button);
}

// Long press on |element_id| to trigger context menu.
void LongPressElement(const char* element_id) {
  id<GREYMatcher> web_view_matcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:web_view_matcher]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        element_id, true /* menu should appear */)];
}

//  Tap on |context_menu_item_button| context menu item.
void TapOnContextMenuButton(id<GREYMatcher> context_menu_item_button) {
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      assertWithMatcher:grey_notNil()];
  [[EarlGrey selectElementWithMatcher:context_menu_item_button]
      performAction:grey_tap()];
  WaitForContextMenuItemDisappeared(context_menu_item_button);
}

// A simple wrapper that sleeps for 1s to wait for the animation, triggered from
// opening a new tab through context menu, to finish before selecting tab.
// TODO(crbug.com/643792): Remove this function when the bug is fixed.
void SelectTabAtIndexInCurrentMode(NSUInteger index) {
  // Delay for 1 second.
  GREYCondition* myCondition = [GREYCondition conditionWithName:@"delay"
                                                          block:^BOOL {
                                                            return NO;
                                                          }];
  [myCondition waitWithTimeout:1];

  chrome_test_util::SelectTabAtIndexInCurrentMode(index);
}

}  // namespace

// Context menu tests for Chrome.
@interface ContextMenuTestCase : ChromeTestCase
@end

@implementation ContextMenuTestCase

+ (void)setUp {
  [super setUp];
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_ALLOW);
}

+ (void)tearDown {
  chrome_test_util::SetContentSettingsBlockPopups(CONTENT_SETTING_DEFAULT);
  [super tearDown];
}

- (void)setUp {
  [super setUp];
  self.testServer->RegisterRequestHandler(
      base::BindRepeating(&StandardResponse));
  self.testServer->ServeFilesFromSourceDirectory(
      base::FilePath(kServerFilesDir));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab. (With the kContextMenuElementPostMessage feature
// disabled.)
- (void)testOpenImageInCurrentTabFromContextMenu {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndDisableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kChromiumImageID);
  TapOnContextMenuButton(OpenImageButton());

  // Verify url and tab count.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that selecting "Open Image in New Tab" from the context menu properly
// opens the image in a new background tab. (With the
// kContextMenuElementPostMessage feature disabled.)
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testOpenImageInNewTabFromContextMenu \
  testOpenImageInNewTabFromContextMenu
#else
#define MAYBE_testOpenImageInNewTabFromContextMenu \
  FLAKY_testOpenImageInNewTabFromContextMenu
#endif
- (void)MAYBE_testOpenImageInNewTabFromContextMenu {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndDisableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kChromiumImageID);
  TapOnContextMenuButton(OpenImageInNewTabButton());

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests "Open in New Tab" on context menu. (With the
// kContextMenuElementPostMessage feature disabled.)
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuOpenInNewTab testContextMenuOpenInNewTab
#else
#define MAYBE_testContextMenuOpenInNewTab FLAKY_testContextMenuOpenInNewTab
#endif
- (void)MAYBE_testContextMenuOpenInNewTab {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndDisableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL initialURL = self.testServer->GetURL(kUrlInitialPage);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kDestinationLinkID);
  TapOnContextMenuButton(OpenLinkInNewTabButton());

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  const GURL destinationURL = self.testServer->GetURL(kUrlDestinationPage);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that the context menu is displayed for an image url. (With the
// kContextMenuElementPostMessage feature disabled.)
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuDisplayedOnImage testContextMenuDisplayedOnImage
#else
#define MAYBE_testContextMenuDisplayedOnImage \
  FLAKY_testContextMenuDisplayedOnImage
#endif
- (void)MAYBE_testContextMenuDisplayedOnImage {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndDisableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [ChromeEarlGrey loadURL:imageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Calculate a point inside the displayed image. Javascript can not be used to
  // find the element because no DOM exists.
  CGPoint point = CGPointMake(
      CGRectGetMidX([chrome_test_util::GetActiveViewController() view].bounds),
      20.0);

  id<GREYMatcher> web_view_matcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:web_view_matcher]
      performAction:grey_longPressAtPointWithDuration(
                        point, kGREYLongPressDefaultDuration)];

  TapOnContextMenuButton(OpenImageInNewTabButton());
  [ChromeEarlGrey waitForMainTabCount:2];

  SelectTabAtIndexInCurrentMode(1U);
  // Verify url.
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that the element fetch duration is logged once with the
// kContextMenuElementPostMessage feature disabled.
- (void)testContextMenuElementFetchDurationMetric {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndDisableFeature(
      web::features::kContextMenuElementPostMessage);
  chrome_test_util::HistogramTester histogramTester;

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kChromiumImageID);
  TapOnContextMenuButton(OpenImageButton());

  histogramTester.ExpectTotalCount("ContextMenu.DOMElementFetchDuration", 1,
                                   ^(NSString* error) {
                                     GREYFail(error);
                                   });
}

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab. (With the kContextMenuElementPostMessage feature
// enabled.)
- (void)testOpenImageInCurrentTabFromContextMenuPostMessage {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kChromiumImageID);
  TapOnContextMenuButton(OpenImageButton());

  // Verify url and tab count.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that selecting "Open Image in New Tab" from the context menu properly
// opens the image in a new background tab. (With the
// kContextMenuElementPostMessage feature enabled.)
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testOpenImageInNewTabFromContextMenuPostMessage \
  testOpenImageInNewTabFromContextMenuPostMessage
#else
#define MAYBE_testOpenImageInNewTabFromContextMenuPostMessage \
  FLAKY_testOpenImageInNewTabFromContextMenuPostMessage
#endif
- (void)MAYBE_testOpenImageInNewTabFromContextMenuPostMessage {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kChromiumImageID);
  TapOnContextMenuButton(OpenImageInNewTabButton());

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests "Open in New Tab" on context menu. (With the
// kContextMenuElementPostMessage feature enabled.)
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuOpenInNewTabPostMessage \
  testContextMenuOpenInNewTabPostMessage
#else
#define MAYBE_testContextMenuOpenInNewTabPostMessage \
  FLAKY_testContextMenuOpenInNewTabPostMessage
#endif
- (void)MAYBE_testContextMenuOpenInNewTabPostMessage {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL initialURL = self.testServer->GetURL(kUrlInitialPage);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kDestinationLinkID);
  TapOnContextMenuButton(OpenLinkInNewTabButton());

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  const GURL destinationURL = self.testServer->GetURL(kUrlDestinationPage);
  [[EarlGrey selectElementWithMatcher:OmniboxText(destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests that the context menu is displayed for an image url. (With the
// kContextMenuElementPostMessage feature enabled.)
// TODO(crbug.com/817810): Enable this test.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuDisplayedOnImagePostMessage \
  testContextMenuDisplayedOnImagePostMessage
#else
#define MAYBE_testContextMenuDisplayedOnImagePostMessage \
  FLAKY_testContextMenuDisplayedOnImagePostMessage
#endif
- (void)MAYBE_testContextMenuDisplayedOnImagePostMessage {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      web::features::kContextMenuElementPostMessage);

  const GURL imageURL = self.testServer->GetURL(kLogoPageImageSourcePath);
  [ChromeEarlGrey loadURL:imageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Calculate a point inside the displayed image. Javascript can not be used to
  // find the element because no DOM exists.
  CGPoint point = CGPointMake(
      CGRectGetMidX([chrome_test_util::GetActiveViewController() view].bounds),
      20.0);

  id<GREYMatcher> web_view_matcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:web_view_matcher]
      performAction:grey_longPressAtPointWithDuration(
                        point, kGREYLongPressDefaultDuration)];

  TapOnContextMenuButton(OpenImageInNewTabButton());
  [ChromeEarlGrey waitForMainTabCount:2];

  SelectTabAtIndexInCurrentMode(1U);
  // Verify url.
  [[EarlGrey selectElementWithMatcher:OmniboxText(imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
}

// Tests that the element fetch duration is logged once with the
// kContextMenuElementPostMessage feature enabled.
- (void)testContextMenuElementFetchDurationMetricPostMessage {
  base::test::ScopedFeatureList scopedFeatureList;
  scopedFeatureList.InitAndEnableFeature(
      web::features::kContextMenuElementPostMessage);
  chrome_test_util::HistogramTester histogramTester;

  const GURL pageURL = self.testServer->GetURL(kLogoPagePath);
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElement(kChromiumImageID);
  TapOnContextMenuButton(OpenImageButton());

  histogramTester.ExpectTotalCount("ContextMenu.DOMElementFetchDuration", 1,
                                   ^(NSString* error) {
                                     GREYFail(error);
                                   });
}

@end
