// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_provider.h"
#include "components/autofill/core/common/submission_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

namespace autofill {
namespace {

const base::FilePath::CharType kDocRoot[] =
    FILE_PATH_LITERAL("chrome/test/data");

class MockAutofillProvider : public TestAutofillProvider {
 public:
  MockAutofillProvider() {}
  ~MockAutofillProvider() override {}

  MOCK_METHOD5(OnFormSubmitted,
               bool(AutofillHandlerProxy* handler,
                    const FormData& form,
                    bool,
                    SubmissionSource,
                    base::TimeTicks));
};

}  // namespace

class AutofillProviderBrowserTest : public InProcessBrowserTest {
 public:
  AutofillProviderBrowserTest() {}
  ~AutofillProviderBrowserTest() override {}

  void SetUpOnMainThread() override {
    content::WebContents* web_contents = WebContents();
    ASSERT_TRUE(web_contents != NULL);

    web_contents->RemoveUserData(
        ContentAutofillDriverFactory::
            kContentAutofillDriverFactoryWebContentsUserDataKey);

    ContentAutofillDriverFactory::CreateForWebContentsAndDelegate(
        web_contents, &autofill_client_, "en-US",
        AutofillManager::DISABLE_AUTOFILL_DOWNLOAD_MANAGER,
        &autofill_provider_);

    embedded_test_server()->AddDefaultHandlers(base::FilePath(kDocRoot));
    // Serve both a.com and b.com (and any other domain).
    host_resolver()->AddRule("*", "127.0.0.1");
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void TearDownOnMainThread() override {
    testing::Mock::VerifyAndClearExpectations(&autofill_provider_);
  }

  content::RenderViewHost* RenderViewHost() {
    return WebContents()->GetRenderViewHost();
  }

  content::WebContents* WebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SimulateUserTypingInFocuedField() {
    content::WebContents* web_contents = WebContents();

    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('O'),
                              ui::DomCode::US_O, ui::VKEY_O, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('R'),
                              ui::DomCode::US_R, ui::VKEY_R, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('A'),
                              ui::DomCode::US_A, ui::VKEY_A, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('R'),
                              ui::DomCode::US_R, ui::VKEY_R, false, false,
                              false, false);
    content::SimulateKeyPress(web_contents, ui::DomKey::FromCharacter('Y'),
                              ui::DomCode::US_Y, ui::VKEY_Y, false, false,
                              false, false);
  }

 protected:
  MockAutofillProvider autofill_provider_;

 private:
  TestAutofillClient autofill_client_;
};

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTest,
                       FrameDetachedOnFormlessSubmission) {
  EXPECT_CALL(autofill_provider_,
              OnFormSubmitted(_, _, _, SubmissionSource::FRAME_DETACHED, _))
      .Times(1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/autofill/frame_detached_on_formless_submit.html"));

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue;

  std::string focus(
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('address_field').focus();");
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), focus));

  SimulateUserTypingInFocuedField();
  std::string fill_and_submit =
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('submit_button').click();";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"SUBMISSION_FINISHED\"")
      break;
  }
  EXPECT_EQ("\"SUBMISSION_FINISHED\"", message);
}

IN_PROC_BROWSER_TEST_F(AutofillProviderBrowserTest,
                       FrameDetachedOnFormSubmission) {
  EXPECT_CALL(autofill_provider_,
              OnFormSubmitted(_, _, _, SubmissionSource::FORM_SUBMISSION, _))
      .Times(1);
  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(
                     "/autofill/frame_detached_on_form_submit.html"));

  // Need to pay attention for a message that XHR has finished since there
  // is no navigation to wait for.
  content::DOMMessageQueue message_queue;

  std::string focus(
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('address_field').focus();");
  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), focus));

  SimulateUserTypingInFocuedField();
  std::string fill_and_submit =
      "var iframe = document.getElementById('address_iframe');"
      "var frame_doc = iframe.contentDocument;"
      "frame_doc.getElementById('submit_button').click();";

  ASSERT_TRUE(content::ExecuteScript(RenderViewHost(), fill_and_submit));
  std::string message;
  while (message_queue.WaitForMessage(&message)) {
    if (message == "\"SUBMISSION_FINISHED\"")
      break;
  }
  EXPECT_EQ("\"SUBMISSION_FINISHED\"", message);
}

}  // namespace autofill
