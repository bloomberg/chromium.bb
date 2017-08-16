// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/app/settings_test_util.h"
#import "ios/chrome/test/app/tab_test_util.h"
#include "ios/chrome/test/app/web_view_interaction_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/earl_grey/web_view_matchers.h"
#import "ios/web/public/test/http_server/http_server.h"
#import "ios/web/public/test/http_server/http_server_util.h"
#include "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::ButtonWithAccessibilityLabelId;
using chrome_test_util::OpenLinkInNewTabButton;

namespace {
const char kUrlChromiumLogoPage[] =
    "http://ios/testing/data/http_server_files/chromium_logo_page.html";
const char kUrlChromiumLogoImg[] =
    "http://ios/testing/data/http_server_files/chromium_logo.png";
const char kUrlInitialPage[] = "http://scenarioContextMenuOpenInNewTab";
const char kUrlDestinationPage[] = "http://destination";
const char kChromiumImageID[] = "chromium_image";
const char kDestinationLinkID[] = "link";

// HTML content of the destination page that sets the page title.
const char kDestinationHtml[] =
    "<script>document.title='new doc'</script>You made it!";

// Matcher for the open image button in the context menu.
id<GREYMatcher> OpenImageButton() {
  return ButtonWithAccessibilityLabelId(IDS_IOS_CONTENT_CONTEXT_OPENIMAGE);
}

// Matcher for the open image in new tab button in the context menu.
id<GREYMatcher> OpenImageInNewTabButton() {
  return ButtonWithAccessibilityLabelId(
      IDS_IOS_CONTENT_CONTEXT_OPENIMAGENEWTAB);
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

// Long press on |element_id| to trigger context menu and then tap on
// |contextMenuItemButton| item.
void LongPressElementAndTapOnButton(const char* element_id,
                                    id<GREYMatcher> context_menu_item_button) {
  id<GREYMatcher> web_view_matcher =
      web::WebViewInWebState(chrome_test_util::GetCurrentWebState());
  [[EarlGrey selectElementWithMatcher:web_view_matcher]
      performAction:chrome_test_util::LongPressElementForContextMenu(
                        element_id, true /* menu should appear */)];

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

// Tests that selecting "Open Image" from the context menu properly opens the
// image in the current tab.
- (void)testOpenImageInCurrentTabFromContextMenu {
  GURL pageURL = web::test::HttpServer::MakeUrl(kUrlChromiumLogoPage);
  GURL imageURL = web::test::HttpServer::MakeUrl(kUrlChromiumLogoImg);
  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElementAndTapOnButton(kChromiumImageID, OpenImageButton());

  // Verify url and tab count.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:1];
}

// Tests that selecting "Open Image in New Tab" from the context menu properly
// opens the image in a new background tab.
- (void)testOpenImageInNewTabFromContextMenu {
  GURL pageURL = web::test::HttpServer::MakeUrl(kUrlChromiumLogoPage);
  GURL imageURL = web::test::HttpServer::MakeUrl(kUrlChromiumLogoImg);
  web::test::SetUpFileBasedHttpServer();
  [ChromeEarlGrey loadURL:pageURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElementAndTapOnButton(kChromiumImageID, OpenImageInNewTabButton());

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          imageURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests "Open in New Tab" on context menu.
- (void)testContextMenuOpenInNewTab {
  // Set up test simple http server.
  std::map<GURL, std::string> responses;
  GURL initialURL = web::test::HttpServer::MakeUrl(kUrlInitialPage);
  GURL destinationURL = web::test::HttpServer::MakeUrl(kUrlDestinationPage);

  // The initial page contains a link to the destination page.
  responses[initialURL] = "<a style='margin-left:50px' href='" +
                          destinationURL.spec() + "' id='link'>link</a>";
  responses[destinationURL] = kDestinationHtml;

  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  LongPressElementAndTapOnButton(kDestinationLinkID, OpenLinkInNewTabButton());

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

// Tests "Open in New Tab" on context menu  on a link that requires scrolling
// on the page to verify that context menu can be properly triggered in the
// current screen view.
- (void)testContextMenuOpenInNewTabFromTallPage {
// TODO(crbug.com/755888): Reenable this test.
#if !TARGET_IPHONE_SIMULATOR
  if (!IsIPadIdiom()) {
    EARL_GREY_TEST_DISABLED(@"Failing constently on iPhone devices.");
  }
#endif

  // Set up test simple http server.
  std::map<GURL, std::string> responses;
  GURL initialURL =
      web::test::HttpServer::MakeUrl("http://scenarioContextMenuOpenInNewTab");
  GURL destinationURL = web::test::HttpServer::MakeUrl("http://destination");

  // The initial page contains a link to the destination page that is below a
  // really tall div so that scrolling is required.
  responses[initialURL] =
      "<div style='height:4000px'></div>"
      "<a style='margin-left:50px' href='" +
      destinationURL.spec() + "' id='link'>link</a>";
  responses[destinationURL] = kDestinationHtml;

  web::test::SetUpSimpleHttpServer(responses);
  [ChromeEarlGrey loadURL:initialURL];
  [ChromeEarlGrey waitForMainTabCount:1];

  // Scroll down on the web view to make the link visible.
  // grey_swipeFastInDirecton will quickly scroll towards the bottom, and then
  // grey_scrollToContentEdge guarantees the content edge is reached. Two
  // methods are used because the first one is much faster, but doesn't
  // guarantee the link becomes visible.
  // TODO(crbug.com/702272): Try to replace this with one EarlGrey method call.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionUp)];
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_scrollToContentEdge(kGREYContentEdgeBottom)];

  [ChromeEarlGrey waitForWebViewContainingText:kDestinationLinkID];

  LongPressElementAndTapOnButton(kDestinationLinkID, OpenLinkInNewTabButton());

  // Earl Grey cannot preperly synchronize some animations, so adding a
  // WaitUntilCondition to wait for the new tab opening animation to finish
  // and the scroll view to become interactable.
  ConditionBlock condition = ^{
    NSError* error = nil;
    [[EarlGrey
        selectElementWithMatcher:WebViewScrollView(
                                     chrome_test_util::GetCurrentWebState())]
        assertWithMatcher:grey_interactable()
                    error:&error];
    return !error;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForUIElementTimeout, condition),
             @"Web view did not become interactable");

  // Make the toolbar visible by scrolling up on the web view to select the
  // newly opened tab.
  [[EarlGrey
      selectElementWithMatcher:WebViewScrollView(
                                   chrome_test_util::GetCurrentWebState())]
      performAction:grey_swipeFastInDirection(kGREYDirectionDown)];
  [ChromeEarlGreyUI waitForToolbarVisible:YES];

  SelectTabAtIndexInCurrentMode(1U);

  // Verify url and tab count.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::OmniboxText(
                                          destinationURL.GetContent())]
      assertWithMatcher:grey_notNil()];
  [ChromeEarlGrey waitForMainTabCount:2];
}

@end
