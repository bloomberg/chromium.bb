// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <Foundation/Foundation.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#include "ios/chrome/browser/chrome_url_constants.h"
#import "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_actions.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_error_util.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#include "ios/web/public/test/element_selector.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using chrome_test_util::GetCurrentWebState;

namespace {
// Directory containing the |kLogoPagePath| and |kLogoPageImageSourcePath|
// resources.
const char kServerFilesDir[] = "ios/testing/data/http_server_files/";

// Id of the "Start Logging" button.
NSString* const kStartLoggingButtonId = @"start-logging";
// Id of the "Stop Logging" button.
NSString* const kStopLoggingButtonId = @"stop-logging";

// Test page continaing buttons to test console logging.
const char kConsolePage[] = "/console_with_iframe.html";
// Id of the console page button to log a debug message.
NSString* const kDebugMessageButtonId = @"debug";
// Id of the console page button to log an error.
NSString* const kErrorMessageButtonId = @"error";
// Id of the console page button to log an info message.
NSString* const kInfoMessageButtonId = @"info";
// Id of the console page button to log a message.
NSString* const kLogMessageButtonId = @"log";
// Id of the console page button to log a warning.
NSString* const kWarningMessageButtonId = @"warn";

// Label for debug console messages.
const char kDebugMessageLabel[] = "DEBUG";
// Label for error console messages.
const char kErrorMessageLabel[] = "ERROR";
// Label for info console messages.
const char kInfoMessageLabel[] = "INFO";
// Label for log console messages.
const char kLogMessageLabel[] = "LOG";
// Label for warning console messages.
const char kWarningMessageLabel[] = "WARN";

// Text of the message emitted from the |kDebugMessageButtonId| button on
// |kConsolePage|.
const char kDebugMessageText[] = "This is a debug message.";
// Text of the message emitted from the |kErrorMessageButtonId| button on
// |kConsolePage|.
const char kErrorMessageText[] = "This is an error message.";
// Text of the message emitted from the |kInfoMessageButtonId| button on
// |kConsolePage|.
const char kInfoMessageText[] = "This is an informative message.";
// Text of the message emitted from the |kLogMessageButtonId| button on
// |kConsolePage|.
const char kLogMessageText[] = "This log is very round.";
// Text of the message emitted from the |kWarningMessageButtonId| button on
// |kConsolePage|.
const char kWarningMessageText[] = "This is a warning message.";

// Text of the message emitted from the |kDebugMessageButtonId| button within
// the iframe on |kConsolePage|.
const char kIFrameDebugMessageText[] = "This is an iframe debug message.";
// Text of the message emitted from the |kErrorMessageButtonId| button within
// the iframe on |kConsolePage|.
const char kIFrameErrorMessageText[] = "This is an iframe error message.";
// Text of the message emitted from the |kInfoMessageButtonId| button within the
// iframe on |kConsolePage|.
const char kIFrameInfoMessageText[] = "This is an iframe informative message.";
// Text of the message emitted from the |kLogMessageButtonId| button within the
// iframe on |kConsolePage|.
const char kIFrameLogMessageText[] = "This iframe log is very round.";
// Text of the message emitted from the |kWarningMessageButtonId| button within
// the iframe on |kConsolePage|.
const char kIFrameWarningMessageText[] = "This is an iframe warning message.";

ElementSelector* StartLoggingButton() {
  return [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kStartLoggingButtonId)];
}

}  // namespace

// Test case for chrome://inspect WebUI page.
@interface InspectUITestCase : ChromeTestCase
@end

@implementation InspectUITestCase

- (void)setUp {
  [super setUp];
  self.testServer->ServeFilesFromSourceDirectory(
      base::FilePath(kServerFilesDir));
  GREYAssertTrue(self.testServer->Start(), @"Server did not start.");
}

// Tests that chrome://inspect allows the user to enable and disable logging.
- (void)testStartStopLogging {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  ElementSelector* stopLoggingButton = [ElementSelector
      selectorWithElementID:base::SysNSStringToUTF8(kStopLoggingButtonId)];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:stopLoggingButton]);

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStopLoggingButtonId]);

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
}

// Tests that log messages from a page's main frame are displayed.
- (void)testMainFrameLogging {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  // Start logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  // Open console test page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);
  std::string debugButtonID = base::SysNSStringToUTF8(kDebugMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:debugButtonID]]);

  // Log messages.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kErrorMessageButtonId]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kInfoMessageButtonId]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kLogMessageButtonId]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kWarningMessageButtonId]);

  [ChromeEarlGrey selectTabAtIndex:0];
  // Validate messages and labels are displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kErrorMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kErrorMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kInfoMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kInfoMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLogMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLogMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kWarningMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kWarningMessageText]);
}

// Tests that log messages from an iframe are displayed.
- (void)testIframeLogging {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  // Start logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  // Open console test page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);

  std::string debugButtonID = base::SysNSStringToUTF8(kDebugMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:debugButtonID]]);

  // Log messages.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementInIFrameWithID:debugButtonID]);

  std::string errorButtonID = base::SysNSStringToUTF8(kErrorMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementInIFrameWithID:errorButtonID]);

  std::string infoButtonID = base::SysNSStringToUTF8(kInfoMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementInIFrameWithID:infoButtonID]);

  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementInIFrameWithID:logButtonID]);

  std::string warnButtonID = base::SysNSStringToUTF8(kWarningMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementInIFrameWithID:warnButtonID]);

  [ChromeEarlGrey selectTabAtIndex:0];
  // Validate messages and labels are displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kIFrameDebugMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kErrorMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kIFrameErrorMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kInfoMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kIFrameInfoMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLogMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kIFrameLogMessageText]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kWarningMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kIFrameWarningMessageText]);
}

// Tests that log messages are correctly displayed from multiple tabs.
- (void)testLoggingFromMultipleTabs {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  // Start logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  // Open console test page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);
  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]]);

  // Log a message and verify it is displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId]);
  [ChromeEarlGrey selectTabAtIndex:0];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText]);

  // Open console test page again.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]]);

  // Log another message and verify it is displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kLogMessageButtonId]);
  [ChromeEarlGrey selectTabAtIndex:0];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLogMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kLogMessageText]);

  // Ensure the log from the first tab still exists.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText]);
}

// Tests that messages are cleared after stopping logging.
- (void)testMessagesClearedOnStopLogging {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  // Start logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  // Open console test page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);
  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]]);

  // Log a message and verify it is displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId]);
  [ChromeEarlGrey selectTabAtIndex:0];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText]);

  // Stop logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStopLoggingButtonId]);
  // Ensure message was cleared.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageText]);
}

// Tests that messages are cleared after a page reload.
- (void)testMessagesClearedOnReload {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  // Start logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  // Open console test page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);
  std::string logButtonID = base::SysNSStringToUTF8(kLogMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:[ElementSelector
                                           selectorWithElementID:logButtonID]]);

  // Log a message and verify it is displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId]);
  [ChromeEarlGrey selectTabAtIndex:0];
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingText:kDebugMessageText]);

  // Reload page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey reload]);
  // Ensure message was cleared.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageText]);
}

// Tests that messages are cleared for a tab which is closed.
- (void)testMessagesClearedOnTabClosure {
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:GURL(kChromeUIInspectURL)]);

  // Start logging.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateContainingElement:StartLoggingButton()]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kStartLoggingButtonId]);

  // Open console test page.
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey openNewTab]);
  const GURL consoleTestsURL = self.testServer->GetURL(kConsolePage);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey loadURL:consoleTestsURL]);
  std::string debugButtonID = base::SysNSStringToUTF8(kDebugMessageButtonId);
  CHROME_EG_ASSERT_NO_ERROR([ChromeEarlGrey
      waitForWebStateContainingElement:
          [ElementSelector selectorWithElementID:debugButtonID]]);

  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey tapWebStateElementWithID:kDebugMessageButtonId]);
  [ChromeEarlGrey closeCurrentTab];

  // Validate message and label are not displayed.
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageLabel]);
  CHROME_EG_ASSERT_NO_ERROR(
      [ChromeEarlGrey waitForWebStateNotContainingText:kDebugMessageText]);
}

@end
