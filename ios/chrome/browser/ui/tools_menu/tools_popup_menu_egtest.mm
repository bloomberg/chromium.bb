// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <XCTest/XCTest.h>

#include "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/chrome_web_view_factory.h"
#import "ios/chrome/browser/ui/toolbar/toolbar_controller.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#import "ios/chrome/browser/ui/uikit_ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#import "ios/chrome/test/earl_grey/accessibility_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#include "ios/web/public/test/response_providers/data_response_provider.h"
#include "ui/base/l10n/l10n_util.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// A ResponseProvider that provides user agent for httpServer request.
class UserAgentResponseProvider : public web::DataResponseProvider {
 public:
  bool CanHandleRequest(const Request& request) override { return true; }

  void GetResponseHeadersAndBody(
      const Request& request,
      scoped_refptr<net::HttpResponseHeaders>* headers,
      std::string* response_body) override {
    // Do not return anything if static plist file has been requested,
    // as plain text is not a valid property list content.
    if ([[base::SysUTF8ToNSString(request.url.spec()) pathExtension]
            isEqualToString:@"plist"]) {
      *headers =
          web::ResponseProvider::GetResponseHeaders("", net::HTTP_NO_CONTENT);
      return;
    }

    *headers = web::ResponseProvider::GetDefaultResponseHeaders();
    std::string userAgent;
    const std::string kDesktopUserAgent =
        base::SysNSStringToUTF8(ChromeWebView::kDesktopUserAgent);
    if (request.headers.GetHeader("User-Agent", &userAgent) &&
        userAgent == kDesktopUserAgent) {
      response_body->assign("Desktop");
    } else {
      response_body->assign("Mobile");
    }
  }
};

// Matcher for the button to find in page.
id<GREYMatcher> FindInPageButton() {
  return chrome_test_util::ButtonWithAccessibilityLabel(
      l10n_util::GetNSStringWithFixup(IDS_IOS_TOOLS_MENU_FIND_IN_PAGE));
}

// Matcher for the button to request desktop version.
id<GREYMatcher> RequestDesktopButton() {
  return grey_accessibilityID(kToolsMenuRequestDesktopId);
}

const char kPDFURL[] = "http://ios/testing/data/http_server_files/testpage.pdf";

}  // namespace

// Tests for the tools popup menu.
@interface ToolsPopupMenuTestCase : ChromeTestCase
- (void)verifyMobileAndDesktopVersions:(const GURL&)url;
@end

@implementation ToolsPopupMenuTestCase

// Verify that requesting desktop and mobile versions works.
- (void)verifyMobileAndDesktopVersions:(const GURL&)url {
  NSString* const kMobileSiteLabel = @"Mobile";
  NSString* const kDesktopSiteLabel = @"Desktop";

  [ChromeEarlGrey loadURL:url];

  // Verify initial reception of the mobile site.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                   base::SysNSStringToUTF8(kMobileSiteLabel))]
      assertWithMatcher:grey_notNil()];

  // Request and verify reception of the desktop site.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:RequestDesktopButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                   base::SysNSStringToUTF8(kDesktopSiteLabel))]
      assertWithMatcher:grey_notNil()];

  // Verify that going back returns to the mobile site.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::BackButton()]
      performAction:grey_tap()];
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::WebViewContainingText(
                                   base::SysNSStringToUTF8(kMobileSiteLabel))]
      assertWithMatcher:grey_notNil()];
}

// Tests that the menu is closed when tapping the close button.
- (void)testOpenAndCloseToolsMenu {
  [ChromeEarlGreyUI openToolsMenu];

  if (!IsCompact()) {
    [[EarlGrey
        selectElementWithMatcher:grey_accessibilityLabel(l10n_util::GetNSString(
                                     IDS_IOS_TOOLBAR_CLOSE_MENU))]
        performAction:grey_tap()];
  } else {
    [[EarlGrey selectElementWithMatcher:chrome_test_util::ToolsMenuButton()]
        performAction:grey_tap()];
  }

  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuTableViewId)]
      assertWithMatcher:grey_notVisible()];
}

// Navigates to a pdf page and verifies that the "Find in Page..." tool
// is not enabled
- (void)testNoSearchForPDF {
  web::test::SetUpFileBasedHttpServer();
  const GURL URL = web::test::HttpServer::MakeUrl(kPDFURL);

  // Navigate to a mock pdf and verify that the find button is disabled.
  [ChromeEarlGrey loadURL:URL];
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey selectElementWithMatcher:FindInPageButton()]
      assertWithMatcher:grey_accessibilityTrait(
                            UIAccessibilityTraitNotEnabled)];
}

// Test requesting desktop version of page works and going back re-opens mobile
// version of page.
- (void)testToolsMenuRequestDesktopNetwork {
  std::unique_ptr<web::DataResponseProvider> provider(
      new UserAgentResponseProvider());
  web::test::SetUpHttpServer(std::move(provider));

  const GURL networkLayerTestURL =
      web::test::HttpServer::MakeUrl("http://network");
  [self verifyMobileAndDesktopVersions:networkLayerTestURL];
}

// Test requesting the desktop version of a page works correctly for
// script-based desktop/mobile differentation.
- (void)testToolsMenuRequestDesktopScript {
  web::test::SetUpFileBasedHttpServer();
  const GURL scriptLayerTestURL = web::test::HttpServer::MakeUrl(
      "http://ios/testing/data/http_server_files/"
      "request_desktop_test_page.html");
  [self verifyMobileAndDesktopVersions:scriptLayerTestURL];
}

// Open tools menu and verify elements are accessible.
- (void)testAccessibilityOnToolsMenu {
  [ChromeEarlGreyUI openToolsMenu];
  chrome_test_util::VerifyAccessibilityForCurrentScreen();
  // Close Tools menu.
  [ChromeTestCase removeAnyOpenMenusAndInfoBars];
}

@end
