// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

void MockCallback(const FormStructure*, const std::string&) {}

class TestAutofillDialogController : public AutofillDialogControllerImpl {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      const AutofillMetrics& metric_logger,
      scoped_refptr<content::MessageLoopRunner> runner,
      const DialogType dialog_type)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     GURL(),
                                     dialog_type,
                                     base::Bind(MockCallback)),
        metric_logger_(metric_logger) ,
        runner_(runner) {
    DisableWallet();
  }

  virtual ~TestAutofillDialogController() {}

  virtual void ViewClosed() OVERRIDE {
    DCHECK(runner_);
    AutofillDialogControllerImpl::ViewClosed();
    runner_->Quit();
  }

  void RunMessageLoop() {
    DCHECK(runner_);
    runner_->Run();
  }

  AutofillDialogCocoa* GetView() {
    return static_cast<AutofillDialogCocoa*>(
        AutofillDialogControllerImpl::view());
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogCocoaBrowserTest : public InProcessBrowserTest {
 public:
  AutofillDialogCocoaBrowserTest() : InProcessBrowserTest() {}

  virtual ~AutofillDialogCocoaBrowserTest() {}

  virtual void SetUpOnMainThread() OVERRIDE {
    FormFieldData field;
    field.autocomplete_attribute = "cc-number";
    FormData form_data;
    form_data.fields.push_back(field);
    runner_ = new content::MessageLoopRunner;
    controller_ = new TestAutofillDialogController(
        browser()->tab_strip_model()->GetActiveWebContents(),
        form_data,
        metric_logger_,
        runner_,
        DIALOG_TYPE_REQUEST_AUTOCOMPLETE);
  }

  TestAutofillDialogController* controller() { return controller_; }

 private:
  // The controller owns itself.
  TestAutofillDialogController* controller_;

  // The following members must outlive the controller.
  AutofillMetrics metric_logger_;
  scoped_refptr<content::MessageLoopRunner> runner_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogCocoaBrowserTest);
};

// The following test fails under ASAN. Disabling until root cause is found.
// This can pop up a "browser_tests would like access to your Contacts" dialog.
// See also http://crbug.com/234008.
#if defined(ADDRESS_SANITIZER)
#define MAYBE_DisplayUI DISABLED_DisplayUI
#else
#define MAYBE_DisplayUI DisplayUI
#endif
IN_PROC_BROWSER_TEST_F(AutofillDialogCocoaBrowserTest, MAYBE_DisplayUI) {
  controller()->Show();
  controller()->GetView()->PerformClose();

  controller()->RunMessageLoop();
}

}  // namespace

}  // namespace autofill
