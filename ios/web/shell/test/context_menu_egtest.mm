// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <WebKit/WebKit.h>
#import <XCTest/XCTest.h>

#include "base/ios/block_types.h"
#include "base/strings/sys_string_conversions.h"
#import "ios/testing/earl_grey/matchers.h"
#import "ios/web/public/test/http_server.h"
#include "ios/web/public/test/http_server_util.h"
#import "ios/web/public/test/web_view_interaction_test_util.h"
#import "ios/web/shell/test/app/web_shell_test_util.h"
#include "ios/web/shell/test/app/web_view_interaction_test_util.h"
#import "ios/web/shell/test/earl_grey/shell_base_test_case.h"
#import "ios/web/shell/test/earl_grey/shell_actions.h"
#import "ios/web/shell/test/earl_grey/shell_earl_grey.h"
#import "ios/web/shell/test/earl_grey/shell_matchers.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using testing::contextMenuItemWithText;
using testing::elementToDismissContextMenu;

// Context menu test cases for the web shell.
@interface ContextMenuTestCase : ShellBaseTestCase
@end

@implementation ContextMenuTestCase

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenu testContextMenu
#else
#define MAYBE_testContextMenu FLAKY_testContextMenu
#endif
// Tests context menu appears on a regular link.
- (void)MAYBE_testContextMenu {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  GURL initialURL = web::test::HttpServer::MakeUrl("http://contextMenuOpen");
  GURL destinationURL = web::test::HttpServer::MakeUrl("http://destination");
  // The initial page contains a link to the destination URL.
  std::string linkID = "link";
  responses[initialURL] =
      "<body>"
      "<a href='" +
      destinationURL.spec() + "' id='" + linkID +
      "'>link for context menu</a>"
      "</span></body>";

  web::test::SetUpSimpleHttpServer(responses);
  [ShellEarlGrey loadURL:initialURL];

  [[EarlGrey selectElementWithMatcher:web::webView()]
      performAction:web::longPressElementForContextMenu(
                        linkID, true /* menu should appear */)];

  id<GREYMatcher> copyItem = contextMenuItemWithText(@"Copy Link");

  // Context menu should have a "copy link" item.
  [[EarlGrey selectElementWithMatcher:copyItem]
      assertWithMatcher:grey_notNil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:elementToDismissContextMenu(@"Cancel")]
      performAction:grey_tap()];

  // Context menu should go away after the tap.
  [[EarlGrey selectElementWithMatcher:copyItem] assertWithMatcher:grey_nil()];
}

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuWebkitTouchCalloutNone \
  testContextMenuWebkitTouchCalloutNone
#else
#define MAYBE_testContextMenuWebkitTouchCalloutNone \
  FLAKY_testContextMenuWebkitTouchCalloutNone
#endif
// Tests context menu on element that has WebkitTouchCallout set to none.
- (void)MAYBE_testContextMenuWebkitTouchCalloutNone {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  GURL initialURL =
      web::test::HttpServer::MakeUrl("http://contextMenuDisabledByWebkit");
  GURL destinationURL = web::test::HttpServer::MakeUrl("http://destination");
  // The initial page contains a link to the destination URL that has an
  // ancestor that disables the context menu via -webkit-touch-callout.
  std::string linkID = "link";
  responses[initialURL] = "<body><a href='" + destinationURL.spec() +
                          "' style='-webkit-touch-callout: none' id='" +
                          linkID +
                          "'>no-callout link</a>"
                          "</body>";

  web::test::SetUpSimpleHttpServer(responses);
  [ShellEarlGrey loadURL:initialURL];

  [[EarlGrey selectElementWithMatcher:web::webView()]
      performAction:web::longPressElementForContextMenu(
                        linkID, false /* menu shouldn't appear */)];

  id<GREYMatcher> copyItem = contextMenuItemWithText(@"Copy Link");

  // Verify no context menu.
  [[EarlGrey selectElementWithMatcher:copyItem] assertWithMatcher:grey_nil()];
}

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuWebkitTouchCalloutNoneFromAncestor \
  testContextMenuWebkitTouchCalloutNoneFromAncestor
#else
#define MAYBE_testContextMenuWebkitTouchCalloutNoneFromAncestor \
  FLAKY_testContextMenuWebkitTouchCalloutNoneFromAncestor
#endif
// Tests context menu on element that has WebkitTouchCallout set to none from an
// ancestor.
- (void)MAYBE_testContextMenuWebkitTouchCalloutNoneFromAncestor {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  GURL initialURL =
      web::test::HttpServer::MakeUrl("http://contextMenuDisabledByWebkit");
  GURL destinationURL = web::test::HttpServer::MakeUrl("http://destination");
  // The initial page contains a link to the destination URL that has an
  // ancestor that disables the context menu via -webkit-touch-callout.
  std::string linkID = "link";
  responses[initialURL] =
      "<body style='-webkit-touch-callout: none'>"
      "<a href='" +
      destinationURL.spec() + "' id='" + linkID +
      "'>ancestor no-callout link</a>"
      "</body>";

  web::test::SetUpSimpleHttpServer(responses);
  [ShellEarlGrey loadURL:initialURL];

  [[EarlGrey selectElementWithMatcher:web::webView()]
      performAction:web::longPressElementForContextMenu(
                        linkID, false /* menu shouldn't appear */)];

  id<GREYMatcher> copyItem = contextMenuItemWithText(@"Copy Link");

  // Verify no context menu.
  [[EarlGrey selectElementWithMatcher:copyItem] assertWithMatcher:grey_nil()];
}

// TODO(crbug.com/675015): Re-enable this test on device.
#if TARGET_IPHONE_SIMULATOR
#define MAYBE_testContextMenuWebkitTouchCalloutOverride \
  testContextMenuWebkitTouchCalloutOverride
#else
#define MAYBE_testContextMenuWebkitTouchCalloutOverride \
  FLAKY_testContextMenuWebkitTouchCalloutOverride
#endif
// Tests context menu on element that has WebkitTouchCallout set to none from an
// ancestor and overridden.
- (void)MAYBE_testContextMenuWebkitTouchCalloutOverride {
  // Create map of canned responses and set up the test HTML server.
  std::map<GURL, std::string> responses;
  GURL initialURL =
      web::test::HttpServer::MakeUrl("http://contextMenuDisabledByWebkit");
  GURL destinationURL = web::test::HttpServer::MakeUrl("http://destination");
  // The initial page contains a link to the destination URL that has an
  // ancestor that disables the context menu via -webkit-touch-callout.
  std::string linkID = "link";
  responses[initialURL] =
      "<body style='-webkit-touch-callout: none'>"
      "<a href='" +
      destinationURL.spec() + "' style='-webkit-touch-callout: default' id='" +
      linkID +
      "'>override no-callout link</a>"
      "</body>";

  web::test::SetUpSimpleHttpServer(responses);
  [ShellEarlGrey loadURL:initialURL];

  [[EarlGrey selectElementWithMatcher:web::webView()]
      performAction:web::longPressElementForContextMenu(
                        linkID, true /* menu should appear */)];

  id<GREYMatcher> copyItem = contextMenuItemWithText(@"Copy Link");

  // Context menu should have a "copy link" item.
  [[EarlGrey selectElementWithMatcher:copyItem]
      assertWithMatcher:grey_notNil()];

  // Dismiss the context menu.
  [[EarlGrey selectElementWithMatcher:elementToDismissContextMenu(@"Cancel")]
      performAction:grey_tap()];

  // Context menu should go away after the tap.
  [[EarlGrey selectElementWithMatcher:copyItem] assertWithMatcher:grey_nil()];
}

@end
