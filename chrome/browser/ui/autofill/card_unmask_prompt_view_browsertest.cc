// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view_tester.h"
#include "chrome/browser/ui/autofill/create_card_unmask_prompt_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "components/autofill/core/browser/ui/card_unmask_prompt_controller_impl.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace autofill {

namespace {

class TestCardUnmaskDelegate : public CardUnmaskDelegate {
 public:
  TestCardUnmaskDelegate() : weak_factory_(this) {}

  virtual ~TestCardUnmaskDelegate() {}

  // CardUnmaskDelegate implementation.
  void OnUnmaskResponse(const UnmaskResponse& response) override {
    response_ = response;
  }
  void OnUnmaskPromptClosed() override {}

  base::WeakPtr<TestCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  UnmaskResponse response() { return response_; }

 private:
  UnmaskResponse response_;

  base::WeakPtrFactory<TestCardUnmaskDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCardUnmaskDelegate);
};

class TestCardUnmaskPromptController : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptController(
      content::WebContents* contents,
      scoped_refptr<content::MessageLoopRunner> runner)
      : CardUnmaskPromptControllerImpl(
            user_prefs::UserPrefs::Get(contents->GetBrowserContext()),
            false),
        runner_(runner),
        weak_factory_(this) {}

  // CardUnmaskPromptControllerImpl implementation.
  base::TimeDelta GetSuccessMessageDuration() const override {
    return base::TimeDelta::FromMilliseconds(10);
  }

  base::WeakPtr<TestCardUnmaskPromptController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

  using CardUnmaskPromptControllerImpl::view;

 private:
  scoped_refptr<content::MessageLoopRunner> runner_;
  base::WeakPtrFactory<TestCardUnmaskPromptController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCardUnmaskPromptController);
};

class CardUnmaskPromptViewBrowserTest : public InProcessBrowserTest {
 public:
  CardUnmaskPromptViewBrowserTest() : InProcessBrowserTest() {}

  ~CardUnmaskPromptViewBrowserTest() override {}

  void SetUpOnMainThread() override {
    runner_ = new content::MessageLoopRunner;
    contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    controller_.reset(new TestCardUnmaskPromptController(contents_, runner_));
    delegate_.reset(new TestCardUnmaskDelegate());
  }

  void FreeDelegate() { delegate_.reset(); }

  content::WebContents* contents() { return contents_; }
  TestCardUnmaskPromptController* controller() { return controller_.get(); }
  TestCardUnmaskDelegate* delegate() { return delegate_.get(); }

 protected:
  // This member must outlive the controller.
  scoped_refptr<content::MessageLoopRunner> runner_;

 private:
  content::WebContents* contents_;
  std::unique_ptr<TestCardUnmaskPromptController> controller_;
  std::unique_ptr<TestCardUnmaskDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest, DisplayUI) {
  controller()->ShowPrompt(CreateCardUnmaskPromptView(controller(), contents()),
                           test::GetMaskedServerCard(),
                           AutofillClient::UNMASK_FOR_AUTOFILL,
                           delegate()->GetWeakPtr());
}

// TODO(bondd): bring up on Mac.
#if !defined(OS_MACOSX)
// Makes sure the user can close the dialog while the verification success
// message is showing.
IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest,
                       EarlyCloseAfterSuccess) {
  controller()->ShowPrompt(CreateCardUnmaskPromptView(controller(), contents()),
                           test::GetMaskedServerCard(),
                           AutofillClient::UNMASK_FOR_AUTOFILL,
                           delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("10"),
                                 base::ASCIIToUTF16("19"), false);
  EXPECT_EQ(base::ASCIIToUTF16("123"), delegate()->response().cvc);
  controller()->OnVerificationResult(AutofillClient::SUCCESS);

  // Simulate the user clicking [x] before the "Success!" message disappears.
  CardUnmaskPromptViewTester::For(controller()->view())->Close();
  // Wait a little while; there should be no crash.
  base::MessageLoop::current()->task_runner()->PostDelayedTask(
      FROM_HERE, base::Bind(&content::MessageLoopRunner::Quit,
                            base::Unretained(runner_.get())),
      2 * controller()->GetSuccessMessageDuration());
  runner_->Run();
}
#endif

// Makes sure the tab can be closed while the dialog is showing.
// https://crbug.com/484376
IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest,
                       CloseTabWhileDialogShowing) {
  controller()->ShowPrompt(CreateCardUnmaskPromptView(controller(), contents()),
                           test::GetMaskedServerCard(),
                           AutofillClient::UNMASK_FOR_AUTOFILL,
                           delegate()->GetWeakPtr());
  // Simulate AutofillManager (the delegate in production code) being destroyed
  // before CardUnmaskPromptViewBridge::OnConstrainedWindowClosed() is called.
  FreeDelegate();
  browser()->tab_strip_model()->GetActiveWebContents()->Close();

  content::RunAllPendingInMessageLoop();
}

}  // namespace

}  // namespace autofill
