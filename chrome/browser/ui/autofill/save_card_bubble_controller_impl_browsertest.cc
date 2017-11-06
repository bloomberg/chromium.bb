// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include <memory>

#include "base/json/json_reader.h"
#include "base/macros.h"
#include "base/values.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

class SaveCardBubbleControllerImplTest : public DialogBrowserTest {
 public:
  SaveCardBubbleControllerImplTest() {}

  std::unique_ptr<base::DictionaryValue> GetTestLegalMessage() {
    std::unique_ptr<base::Value> value(base::JSONReader::Read(
        "{"
        "  \"line\" : [ {"
        "     \"template\": \"This is the entire message.\""
        "  } ]"
        "}"));
    base::DictionaryValue* dictionary;
    value->GetAsDictionary(&dictionary);
    return dictionary->CreateDeepCopy();
  }

  // DialogBrowserTest:
  void ShowDialog(const std::string& name) override {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    // Do lazy initialization of SaveCardBubbleControllerImpl. Alternative:
    // invoke via ChromeAutofillClient.
    SaveCardBubbleControllerImpl::CreateForWebContents(web_contents);
    SaveCardBubbleControllerImpl* controller =
        SaveCardBubbleControllerImpl::FromWebContents(web_contents);
    DCHECK(controller);

    // Behavior depends on the test case name (not the most robust, but it will
    // do).
    if (name.find("Local") != std::string::npos) {
      controller->ShowBubbleForLocalSave(test::GetCreditCard(),
                                         base::Bind(&base::DoNothing));
    } else {
      CreditCard card = test::GetMaskedServerCard();
      bool should_cvc_be_requested = name.find("Cvc") != std::string::npos;
      controller->ShowBubbleForUpload(card, GetTestLegalMessage(),
                                      should_cvc_be_requested,
                                      base::Bind(&base::DoNothing));
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImplTest);
};

// Invokes a bubble asking the user if they want to save a credit card locally.
// See test_browser_dialog.h for instructions on how to run.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeDialog_Local) {
  RunDialog();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server. See test_browser_dialog.h for instructions on how to run.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest, InvokeDialog_Server) {
  RunDialog();
}

// Invokes a bubble asking the user if they want to save a credit card to the
// server, with an added CVC step. See test_browser_dialog.h for instructions on
// how to run.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeDialog_Server_WithCvcStep) {
  RunDialog();
}

}  // namespace autofill
