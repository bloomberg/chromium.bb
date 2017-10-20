// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/save_card_bubble_controller_impl.h"

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"

namespace autofill {

class SaveCardBubbleControllerImplTest : public DialogBrowserTest {
 public:
  SaveCardBubbleControllerImplTest() {}

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
    CreditCard card = test::GetCreditCard();
    controller->ShowBubbleForLocalSave(card, base::Bind(&base::DoNothing));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SaveCardBubbleControllerImplTest);
};

// Invokes a bubble asking the user if they want to save a credit card. See
// test_browser_dialog.h.
IN_PROC_BROWSER_TEST_F(SaveCardBubbleControllerImplTest,
                       InvokeDialog_LocalSave) {
  RunDialog();
}

}  // namespace autofill
