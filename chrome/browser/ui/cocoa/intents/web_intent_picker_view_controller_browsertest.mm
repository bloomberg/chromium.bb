// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_sheet_controller.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_prompt_test_utils.h"
#import "chrome/browser/ui/cocoa/extensions/extension_install_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_choose_service_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_extension_prompt_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_inline_service_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_message_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_cocoa.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_picker_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_progress_view_controller.h"
#import "chrome/browser/ui/cocoa/intents/web_intent_service_row_view_controller.h"
#import "chrome/browser/ui/cocoa/key_equivalent_constants.h"
#import "chrome/browser/ui/cocoa/spinner_progress_indicator.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate_mock.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message.h"
#import "testing/gtest_mac.h"

namespace {

NSString* const kTitle = @"A quick brown fox jumps over the lazy dog.";

}  // namespace

class WebIntentPickerViewControllerTest : public InProcessBrowserTest {
 public:
  WebIntentPickerViewControllerTest() : picker_(NULL) {
  }

  virtual void SetUpOnMainThread() OVERRIDE {
    content::WebContents* tab =
        browser()->tab_strip_model()->GetWebContentsAt(0);
    picker_ = new WebIntentPickerCocoa(tab, &delegate_, &model_);
    controller_ = picker_->view_controller();
  }

  virtual void CleanUpOnMainThread() OVERRIDE {
    EXPECT_CALL(delegate_, OnClosing());
    picker_->Close();
  }

 protected:
  WebIntentPickerDelegateMock delegate_;
  WebIntentPickerModel model_;
  WebIntentPickerCocoa* picker_;  // weak
  WebIntentPickerViewController* controller_;  // weak
};

// Test that the view is created and embedded in the constrained window.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, View) {
  EXPECT_TRUE([controller_ view]);
  EXPECT_TRUE([[controller_ view] superview]);
}

// Test the close button.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, CloseButton) {
  EXPECT_NSEQ([controller_ view],
              [[controller_ closeButton] superview]);
  EXPECT_NSEQ(kKeyEquivalentEscape, [[controller_ closeButton] keyEquivalent]);
  EXPECT_CALL(delegate_, OnUserCancelledPickerDialog());
  [[controller_ closeButton] performClick:nil];
}

// Test the "waiting for chrome web store" state.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, Waiting) {
  model_.SetWaitingForSuggestions(true);
  EXPECT_EQ(PICKER_STATE_WAITING, [controller_ state]);
  WebIntentProgressViewController* progress_controller =
      [controller_ progressViewController];
  EXPECT_NSEQ([controller_ view], [[progress_controller view] superview]);
  EXPECT_TRUE([[progress_controller progressIndicator] isIndeterminate]);
}

// Test the "no matching services" state.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, NoServices) {
  model_.SetWaitingForSuggestions(false);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(PICKER_STATE_NO_SERVICE, [controller_ state]);
  WebIntentMessageViewController* message_controller =
      [controller_ messageViewController];
  EXPECT_NSEQ([controller_ view], [[message_controller view] superview]);
}

// Test the "choose a service" state.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, ChooseService) {
  testing::InSequence dummy;
  model_.SetWaitingForSuggestions(false);

  // Add a installed service.
  GURL url("http://example.com");
  webkit_glue::WebIntentServiceData::Disposition disposition =
      webkit_glue::WebIntentServiceData::DISPOSITION_INLINE;
  model_.AddInstalledService(ASCIIToUTF16("Title"), url, disposition);

  // Add a suggested service.
  std::vector<WebIntentPickerModel::SuggestedExtension> suggestions;
  WebIntentPickerModel::SuggestedExtension suggestion(
      ASCIIToUTF16("Title"), "1234", 2);
  suggestions.push_back(suggestion);
  model_.AddSuggestedExtensions(suggestions);

  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(PICKER_STATE_CHOOSE_SERVICE, [controller_ state]);
  WebIntentChooseServiceViewController* choose_controller =
      [controller_ chooseServiceViewController];
  EXPECT_NSEQ([controller_ view], [[choose_controller view] superview]);
  NSArray* rows = [choose_controller rows];
  EXPECT_EQ(2u, [rows count]);

  // Test clicking "show more services in web store" button.
  EXPECT_CALL(delegate_, OnSuggestionsLinkClicked(CURRENT_TAB));
  [[choose_controller showMoreServicesButton] performClick:nil];

  // Test click "select" in the installed service row.
  EXPECT_CALL(delegate_,
              OnServiceChosen(url, disposition,
                              WebIntentPickerDelegate::kEnableDefaults));
  [[[rows objectAtIndex:0] selectButton] performClick:nil];

  // Test click "select" in the suggested service row.
  EXPECT_CALL(delegate_, OnExtensionInstallRequested(suggestion.id));
  [[[rows objectAtIndex:1] selectButton] performClick:nil];

  // Test click suggested service web store link.
  EXPECT_CALL(delegate_, OnExtensionLinkClicked(suggestion.id, CURRENT_TAB));
  [[[rows objectAtIndex:1] titleLinkButton] performClick:nil];

  // Remove everything but suggested extensions.
  model_.Clear();
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(PICKER_STATE_CHOOSE_SERVICE, [controller_ state]);
  rows = [choose_controller rows];
  EXPECT_EQ(1u, [rows count]);
}

// Test showing an inline service.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, InlineService) {
  testing::InSequence dummy;

  // Create a web view
  GURL url("about:blank");
  content::WebContents::CreateParams create_params(
      browser()->profile(),
      tab_util::GetSiteInstanceForNewTab(browser()->profile(), url));
  content::WebContents* web_contents = content::WebContents::Create(
      create_params);
  EXPECT_CALL(delegate_,
              CreateWebContentsForInlineDisposition(testing::_, testing::_))
      .WillOnce(testing::Return(web_contents));

  model_.SetWaitingForSuggestions(false);

  webkit_glue::WebIntentServiceData::Disposition disposition =
      webkit_glue::WebIntentServiceData::DISPOSITION_INLINE;
  model_.AddInstalledService(ASCIIToUTF16("Title"), url, disposition);
  model_.SetInlineDisposition(url);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(PICKER_STATE_INLINE_SERVICE, [controller_ state]);

  WebIntentInlineServiceViewController * inline_controller =
      [controller_ inlineServiceViewController];
  EXPECT_NSEQ([controller_ view], [[inline_controller view] superview]);

  // Test clicking "choose another service".
  EXPECT_CALL(delegate_, OnChooseAnotherService());
  [[inline_controller chooseServiceButton] performClick:nil];

  // Test hiding "choose another service".
  EXPECT_FALSE([[inline_controller chooseServiceButton] isHidden]);
  model_.set_show_use_another_service(false);
  [controller_ update];
  EXPECT_TRUE([[inline_controller chooseServiceButton] isHidden]);
}

// Test the "installing a service" state.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, Installing) {
  // Add a suggested service.
  std::vector<WebIntentPickerModel::SuggestedExtension> suggestions;
  WebIntentPickerModel::SuggestedExtension suggestion(
      ASCIIToUTF16("Title"), "1234", 2);
  suggestions.push_back(suggestion);
  model_.AddSuggestedExtensions(suggestions);

  // Set a pending extension download.
  model_.SetWaitingForSuggestions(false);
  model_.SetPendingExtensionInstallId(suggestion.id);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(PICKER_STATE_INSTALLING_EXTENSION, [controller_ state]);

  WebIntentProgressViewController* progress_controller =
      [controller_ progressViewController];
  EXPECT_NSEQ([controller_ view], [[progress_controller view] superview]);
  SpinnerProgressIndicator* progress_indicator =
      [progress_controller progressIndicator];
  EXPECT_FALSE([progress_indicator isIndeterminate]);

  int percent_done = 50;
  model_.SetPendingExtensionInstallDownloadProgress(percent_done);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(percent_done, [progress_indicator percentDone]);
}

// Test show the extension install prompt.
IN_PROC_BROWSER_TEST_F(WebIntentPickerViewControllerTest, ExtensionPrompt) {
  scoped_refptr<extensions::Extension> extension =
      chrome::LoadInstallPromptExtension();
  chrome::MockExtensionInstallPromptDelegate delegate;
  ExtensionInstallPrompt::Prompt prompt =
      chrome::BuildExtensionInstallPrompt(extension.get());

  // Set a pending install prompt.
  model_.SetPendingExtensionInstallDelegate(&delegate);
  model_.SetPendingExtensionInstallPrompt(prompt);
  content::RunAllPendingInMessageLoop();
  EXPECT_EQ(PICKER_STATE_EXTENSION_PROMPT, [controller_ state]);

  // Verify that the view controll is embedded.
  WebIntentExtensionPromptViewController* extension_prompt_controller =
      [controller_ extensionPromptViewController];
  EXPECT_NSEQ([controller_ view],
              [[extension_prompt_controller view] superview]);

  // Press cancel.
  [[[extension_prompt_controller viewController] cancelButton]
      performClick:nil];
  EXPECT_EQ(1, delegate.abort_count());
}
