// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <EarlGrey/EarlGrey.h>
#import <UIKit/UIKit.h>
#import <XCTest/XCTest.h>

#import "base/strings/sys_string_conversions.h"
#include "components/strings/grit/components_strings.h"
#import "ios/chrome/browser/ui/dialogs/dialog_presenter.h"
#import "ios/chrome/browser/ui/tools_menu/tools_menu_view_controller.h"
#include "ios/chrome/browser/ui/ui_util.h"
#include "ios/chrome/grit/ios_strings.h"
#include "ios/chrome/test/app/chrome_test_util.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey.h"
#import "ios/chrome/test/earl_grey/chrome_earl_grey_ui.h"
#import "ios/chrome/test/earl_grey/chrome_matchers.h"
#import "ios/chrome/test/earl_grey/chrome_test_case.h"
#import "ios/testing/earl_grey/disabled_test_macros.h"
#import "ios/testing/wait_util.h"
#import "ios/web/public/test/http_server.h"
#import "ios/web/public/test/http_server_util.h"
#include "ios/web/public/web_state/web_state.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"
#import "url/gurl.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Enum specifying different types of JavaScript alerts:
//   - JavaScriptAlertType::ALERT - Dialog with only one OK button.
//   - JavaScriptAlertType::CONFIRMATION - Dialog with OK and Cancel button.
//   - JavaScriptAlertType::PROMPT - Dialog with OK button, cancel button, and
//     a text field.
enum class JavaScriptAlertType : NSUInteger { ALERT, CONFIRMATION, PROMPT };

// Script to inject that will show an alert.  The document's body will be reset
// to |kAlertResultBody| after the dialog is dismissed.
const char kAlertMessage[] = "This is a JavaScript alert.";
const char kAlertResultBody[] = "JAVASCRIPT ALERT WAS DISMISSED";
const char kJavaScriptAlertTestScriptFormat[] =
    "(function(){ "
    "  alert(\"%@\");"
    "  document.body.innerHTML = \"%@\";"
    "})();";
NSString* GetJavaScriptAlertTestScript() {
  return [NSString stringWithFormat:@(kJavaScriptAlertTestScriptFormat),
                                    @(kAlertMessage), @(kAlertResultBody)];
}

// Script to inject that will show a confirmation dialog.  The document's body
// will be reset to |kConfirmationResultBodyOK| or
// |kConfirmationResultBodyCancelled| depending on whether the OK or Cancel
// button was tapped.
const char kConfirmationMessage[] = "This is a JavaScript confirmation.";
const char kConfirmationResultBodyOK[] = "Okay";
const char kConfirmationResultBodyCancelled[] = "Cancelled";
const char kJavaScriptConfirmationScriptFormat[] =
    "(function(){ "
    "  if (confirm(\"%@\") == true) {"
    "    document.body.innerHTML = \"%@\";"
    "  } else {"
    "    document.body.innerHTML = \"%@\";"
    "  }"
    "})();";
NSString* GetJavaScriptConfirmationTestScript() {
  return [NSString stringWithFormat:@(kJavaScriptConfirmationScriptFormat),
                                    @(kConfirmationMessage),
                                    @(kConfirmationResultBodyOK),
                                    @(kConfirmationResultBodyCancelled)];
}

// Script to inject that will show a prompt dialog.  The document's body will be
// reset to |kPromptResultBodyCancelled| or |kPromptTestUserInput| depending on
// whether the OK or Cancel button was tapped.
const char kPromptMessage[] = "This is a JavaScript prompt.";
const char kPromptResultBodyCancelled[] = "Cancelled";
const char kPromptTestUserInput[] = "test";
const char kJavaScriptPromptTestScriptFormat[] =
    "(function(){ "
    "  var input = prompt(\"%@\");"
    "  if (input != null) {"
    "    document.body.innerHTML = input;"
    "  } else {"
    "    document.body.innerHTML = \"%@\";"
    "  }"
    "})();";
NSString* GetJavaScriptPromptTestScript() {
  return [NSString stringWithFormat:@(kJavaScriptPromptTestScriptFormat),
                                    @(kPromptMessage),
                                    @(kPromptResultBodyCancelled)];
}

// Script to inject that will show a JavaScript alert in a loop 20 times, then
// reset the document's HTML to |kAlertLoopFinishedText|.
const char kAlertLoopFinishedText[] = "Loop Finished";
const char kJavaScriptAlertLoopScriptFormat[] =
    "(function(){ "
    "  for (i = 0; i < 20; ++i) {"
    "    alert(\"ALERT TEXT\");"
    "  }"
    "  document.body.innerHTML = \"%@\";"
    "})();";
NSString* GetJavaScriptAlertLoopScript() {
  return [NSString stringWithFormat:@(kJavaScriptAlertLoopScriptFormat),
                                    @(kAlertLoopFinishedText)];
}

// Returns the message for a JavaScript alert with |type|.
NSString* GetMessageForAlertWithType(JavaScriptAlertType type) {
  switch (type) {
    case JavaScriptAlertType::ALERT:
      return @(kAlertMessage);
    case JavaScriptAlertType::CONFIRMATION:
      return @(kConfirmationMessage);
    case JavaScriptAlertType::PROMPT:
      return @(kPromptMessage);
  }
  GREYFail(@"JavascriptAlertType not recognized.");
  return nil;
}

// Returns the script to show a JavaScript alert with |type|.
NSString* GetScriptForAlertWithType(JavaScriptAlertType type) {
  switch (type) {
    case JavaScriptAlertType::ALERT:
      return GetJavaScriptAlertTestScript();
    case JavaScriptAlertType::CONFIRMATION:
      return GetJavaScriptConfirmationTestScript();
    case JavaScriptAlertType::PROMPT:
      return GetJavaScriptPromptTestScript();
  }
  GREYFail(@"JavascriptAlertType not recognized.");
  return nil;
}

// Const for an http server that returns a blank document.
const char* kJavaScriptTestURL = "http://jsalerts";
const char* kJavaScriptTestResponse =
    "<!DOCTYPE html><html><body></body></html>";

// Waits until |string| is displayed on the web view.
void WaitForWebDisplay(const std::string& string) {
  id<GREYMatcher> response1Matcher =
      chrome_test_util::WebViewContainingText(string);
  [[EarlGrey selectElementWithMatcher:response1Matcher]
      assertWithMatcher:grey_notNil()];
}

// Display the javascript alert.
void DisplayJavaScriptAlert(JavaScriptAlertType type) {
  // Get the WebController.
  web::WebState* webState = chrome_test_util::GetCurrentWebState();

  // Evaluate JavaScript.
  NSString* script = GetScriptForAlertWithType(type);
  webState->ExecuteJavaScript(base::SysNSStringToUTF16(script));
}

// Assert that the javascript alert has been presented.
void WaitForAlertToBeShown(NSString* alert_label) {
  // Wait for the alert to be shown by trying to get the alert title.
  ConditionBlock condition = ^{
    NSError* error = nil;
    id<GREYMatcher> titleLabel =
        chrome_test_util::StaticTextWithAccessibilityLabel(alert_label);
    [[EarlGrey selectElementWithMatcher:titleLabel]
        assertWithMatcher:grey_notNil()
                    error:&error];
    return !error;
  };
  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForJSCompletionTimeout, condition),
             @"Alert with title was not present: %@", alert_label);
}

void WaitForJavaScripDialogToBeShown() {
  NSString* alert_label = [DialogPresenter
      localizedTitleForJavaScriptAlertFromPage:web::test::HttpServer::MakeUrl(
                                                   kJavaScriptTestURL)];
  WaitForAlertToBeShown(alert_label);
}

// Injects JavaScript to show a dialog with |type|, verifying that it was
// properly displayed.
void ShowJavaScriptDialog(JavaScriptAlertType type) {
  DisplayJavaScriptAlert(type);

  WaitForJavaScripDialogToBeShown();

  // Check the message of the alert.
  id<GREYMatcher> messageLabel =
      chrome_test_util::StaticTextWithAccessibilityLabel(
          GetMessageForAlertWithType(type));
  [[EarlGrey selectElementWithMatcher:messageLabel]
      assertWithMatcher:grey_notNil()];
}

// Assert no javascript alert is visible.
void AssertJavaScriptAlertNotPresent() {
  ConditionBlock condition = ^{
    NSError* error = nil;
    NSString* alertLabel = [DialogPresenter
        localizedTitleForJavaScriptAlertFromPage:web::test::HttpServer::MakeUrl(
                                                     kJavaScriptTestURL)];
    id<GREYMatcher> titleLabel =
        chrome_test_util::StaticTextWithAccessibilityLabel(alertLabel);
    [[EarlGrey selectElementWithMatcher:titleLabel] assertWithMatcher:grey_nil()
                                                                error:&error];
    return !error;
  };

  GREYAssert(testing::WaitUntilConditionOrTimeout(
                 testing::kWaitForJSCompletionTimeout, condition),
             @"Javascript alert title was still present");
}

// Types |input| in the prompt.
void TypeInPrompt(NSString* input) {
  [[[EarlGrey selectElementWithMatcher:
                  grey_accessibilityID(
                      kJavaScriptDialogTextFieldAccessibiltyIdentifier)]
      assertWithMatcher:grey_sufficientlyVisible()] performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:
                 grey_accessibilityID(
                     kJavaScriptDialogTextFieldAccessibiltyIdentifier)]
      performAction:grey_typeText(input)];
}

void TapOK() {
  id<GREYMatcher> ok_button =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_OK);
  [[EarlGrey selectElementWithMatcher:ok_button] performAction:grey_tap()];
}

void TapCancel() {
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()];
}

void TapSuppressDialogsButton() {
  id<GREYMatcher> suppress_dialogs_button =
      chrome_test_util::ButtonWithAccessibilityLabelId(
          IDS_IOS_JAVA_SCRIPT_DIALOG_BLOCKING_BUTTON_TEXT);
  [[EarlGrey selectElementWithMatcher:suppress_dialogs_button]
      performAction:grey_tap()];
}

}  // namespace

@interface JavaScriptDialogTestCase : ChromeTestCase {
  GURL _URL;
}

@end

@implementation JavaScriptDialogTestCase

- (void)setUp {
  [super setUp];
  _URL = web::test::HttpServer::MakeUrl(kJavaScriptTestURL);
  std::map<GURL, std::string> responses;
  responses[web::test::HttpServer::MakeUrl(kJavaScriptTestURL)] =
      kJavaScriptTestResponse;
  web::test::SetUpSimpleHttpServer(responses);

  [ChromeEarlGrey loadURL:_URL];
  id<GREYMatcher> response2Matcher =
      chrome_test_util::WebViewContainingText(std::string());
  [[EarlGrey selectElementWithMatcher:response2Matcher]
      assertWithMatcher:grey_notNil()];
}

- (void)tearDown {
  NSError* errorOK = nil;
  NSError* errorCancel = nil;

  // Dismiss JavaScript alert by tapping Cancel.
  [[EarlGrey selectElementWithMatcher:chrome_test_util::CancelButton()]
      performAction:grey_tap()
              error:&errorCancel];
  // Dismiss JavaScript alert by tapping OK.
  id<GREYMatcher> OKButton =
      chrome_test_util::ButtonWithAccessibilityLabelId(IDS_OK);
  [[EarlGrey selectElementWithMatcher:OKButton] performAction:grey_tap()
                                                        error:&errorOK];

  if (!errorOK || !errorCancel) {
    GREYFail(@"There are still alerts");
  }
  [super tearDown];
}

#pragma mark - Tests

// Tests that an alert is shown, and that the completion block is called.
- (void)testShowJavaScriptAlert {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  // Show an alert.
  ShowJavaScriptDialog(JavaScriptAlertType::ALERT);

  // Tap the OK button.
  TapOK();

  // Wait for the html body to be reset to the correct value.
  WaitForWebDisplay(kAlertResultBody);
}

// Tests that a confirmation dialog is shown, and that the completion block is
// called with the correct value when the OK buton is tapped.
- (void)testShowJavaScriptConfirmationOK {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  // Show a confirmation dialog.
  ShowJavaScriptDialog(JavaScriptAlertType::CONFIRMATION);

  // Tap the OK button.
  TapOK();

  // Wait for the html body to be reset to the correct value.
  WaitForWebDisplay(kConfirmationResultBodyOK);
}

// Tests that a confirmation dialog is shown, and that the completion block is
// called with the correct value when the Cancel buton is tapped.
- (void)testShowJavaScriptConfirmationCancelled {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  // Show a confirmation dialog.
  ShowJavaScriptDialog(JavaScriptAlertType::CONFIRMATION);

  // Tap the Cancel button.
  TapCancel();

  // Wait for the html body to be reset to the correct value.
  WaitForWebDisplay(kConfirmationResultBodyCancelled);
}

// Tests that a prompt dialog is shown, and that the completion block is called
// with the correct value when the OK buton is tapped.
- (void)testShowJavaScriptPromptOK {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  // Show a prompt dialog.
  ShowJavaScriptDialog(JavaScriptAlertType::PROMPT);

  // Enter text into text field.
  TypeInPrompt(@(kPromptTestUserInput));

  // Tap the OK button.
  TapOK();

  // Wait for the html body to be reset to the input text.
  WaitForWebDisplay(kPromptTestUserInput);
}

// Tests that a prompt dialog is shown, and that the completion block is called
// with the correct value when the Cancel buton is tapped.
- (void)testShowJavaScriptPromptCancelled {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  // Show a prompt dialog.
  ShowJavaScriptDialog(JavaScriptAlertType::PROMPT);

  // Enter text into text field.
  TypeInPrompt(@(kPromptTestUserInput));

  // Tap the Cancel button.
  TapCancel();

  // Wait for the html body to be reset to the cancel text.
  WaitForWebDisplay(kPromptResultBodyCancelled);
}

// Tests that JavaScript alerts that are shown in a loop can be suppressed.
- (void)testShowJavaScriptAlertLoop {
  // Evaluate JavaScript to show alerts in an endless loop.
  web::WebState* webState = chrome_test_util::GetCurrentWebState();
  NSString* script = GetJavaScriptAlertLoopScript();
  webState->ExecuteJavaScript(base::SysNSStringToUTF16(script));
  WaitForJavaScripDialogToBeShown();

  // Tap the OK button and wait for another dialog to be shown.
  TapOK();
  WaitForJavaScripDialogToBeShown();

  // Tap the suppress dialogs button.
  TapSuppressDialogsButton();

  // Wait for confirmation action sheet to be shown.
  NSString* alertLabel =
      l10n_util::GetNSString(IDS_JAVASCRIPT_MESSAGEBOX_SUPPRESS_OPTION);
  WaitForAlertToBeShown(alertLabel);

  // Tap the suppress dialogs confirmation button.
  TapSuppressDialogsButton();

  // Wait for the html body to be reset to the loop finished text.
  WaitForWebDisplay(kAlertLoopFinishedText);
}

// Tests to ensure crbug.com/658260 does not regress.
// Tests that if an alert should be called when settings are displays, the alert
// waits for the dismiss of the settings.
- (void)testShowJavaScriptBehindSettings {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  // Show settings.
  [ChromeEarlGreyUI openToolsMenu];
  [[EarlGrey
      selectElementWithMatcher:grey_accessibilityID(kToolsMenuSettingsId)]
      performAction:grey_tap()];
  [[EarlGrey selectElementWithMatcher:chrome_test_util::
                                          StaticTextWithAccessibilityLabelId(
                                              IDS_IOS_SETTINGS_TITLE)]
      assertWithMatcher:grey_sufficientlyVisible()];

  // Show an alert.
  DisplayJavaScriptAlert(JavaScriptAlertType::ALERT);

  // Make sure the alert is not present.
  AssertJavaScriptAlertNotPresent();

  // Close the settings.
  [[EarlGrey
      selectElementWithMatcher:chrome_test_util::ButtonWithAccessibilityLabelId(
                                   IDS_IOS_NAVIGATION_BAR_DONE_BUTTON)]
      performAction:grey_tap()];

  // Make sure the alert is present.
  WaitForJavaScripDialogToBeShown();

  // Tap the OK button.
  TapOK();

  // Wait for the html body to be reset to the correct value.
  WaitForWebDisplay(kAlertResultBody);
}

// Tests that an alert is presented after displaying the share menu.
- (void)testShowJavaScriptAfterShareMenu {
// TODO(crbug.com/663026): Reenable the test for devices.
#if !TARGET_IPHONE_SIMULATOR
  EARL_GREY_TEST_DISABLED(@"Disabled for devices because existing system "
                          @"alerts would prevent app alerts to present "
                          @"correctly.");
#endif

  [ChromeEarlGreyUI openShareMenu];

  // Copy URL, dismissing the share menu.
  id<GREYMatcher> printButton =
      grey_allOf(grey_accessibilityLabel(@"Copy"),
                 grey_accessibilityTrait(UIAccessibilityTraitButton), nil);
  [[EarlGrey selectElementWithMatcher:printButton] performAction:grey_tap()];

  // Show an alert and assert it is present.
  ShowJavaScriptDialog(JavaScriptAlertType::ALERT);

  // Tap the OK button.
  TapOK();

  // Wait for the html body to be reset to the correct value.
  WaitForWebDisplay(kAlertResultBody);
}

@end
