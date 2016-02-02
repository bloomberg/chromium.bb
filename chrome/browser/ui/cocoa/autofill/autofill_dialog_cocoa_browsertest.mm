// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/form_data.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void MockCallback(AutofillClient::RequestAutocompleteResult result,
                  const base::string16&,
                  const FormStructure*) {
}

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      scoped_refptr<content::MessageLoopRunner> runner)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     GURL(),
                                     base::Bind(MockCallback)),
        runner_(runner) {}

  ~TestAutofillDialogController() override {}

  void ViewClosed() override {
    DCHECK(runner_.get());
    runner_->Quit();
    AutofillDialogControllerImpl::ViewClosed();
  }

  AutofillDialogCocoa* GetView() {
    return static_cast<AutofillDialogCocoa*>(
        AutofillDialogControllerImpl::view());
  }

 private:
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogCocoaBrowserTest : public InProcessBrowserTest {
 public:
  AutofillDialogCocoaBrowserTest() {}

  void SetUpOnMainThread() override {
    // Ensure Mac OS X does not pop up a modal dialog for the Address Book.
    autofill::test::DisableSystemServices(browser()->profile()->GetPrefs());

    // Stick to local autofill mode.
    browser()->profile()->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet, true);

    FormFieldData field;
    field.autocomplete_attribute = "cc-number";
    FormData form_data;
    form_data.fields.push_back(field);
    runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        browser()->tab_strip_model()->GetActiveWebContents(),
        form_data,
        runner_);
  }

  TestAutofillDialogController* controller() { return controller_; }

  void RunMessageLoop() {
    DCHECK(runner_.get());
    runner_->Run();
  }

 private:
  // The controller owns itself.
  TestAutofillDialogController* controller_;

  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogCocoaBrowserTest);
};

IN_PROC_BROWSER_TEST_F(AutofillDialogCocoaBrowserTest, DisplayUI) {
  controller()->Show();
  controller()->OnCancel();
  controller()->Hide();

  RunMessageLoop();
}

}  // namespace

}  // namespace autofill
