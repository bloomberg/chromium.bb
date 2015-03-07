// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"
#include "base/message_loop/message_loop.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_controller_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/card_unmask_delegate.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace autofill {

namespace {

class TestCardUnmaskDelegate : public CardUnmaskDelegate {
 public:
  TestCardUnmaskDelegate() : weak_factory_(this) {}

  virtual ~TestCardUnmaskDelegate() {}

  // CardUnmaskDelegate implementation.
  void OnUnmaskResponse(const UnmaskResponse& response) override {}
  void OnUnmaskPromptClosed() override {}

  base::WeakPtr<TestCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<TestCardUnmaskDelegate> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCardUnmaskDelegate);
};

class TestCardUnmaskPromptController : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptController(
      content::WebContents* contents,
      scoped_refptr<content::MessageLoopRunner> runner)
      : CardUnmaskPromptControllerImpl(contents),
        runner_(runner),
        weak_factory_(this) {}

  // CardUnmaskPromptControllerImpl implementation.
  void OnDidLoadRiskFingerprint(const std::string& risk_data) override {
    CardUnmaskPromptControllerImpl::OnDidLoadRiskFingerprint(risk_data);

    // Call Quit() from here rather than from OnUnmaskDialogClosed().
    // FingerprintDataLoader starts several async tasks that take a while to
    // complete. If Quit() is called before FingerprintDataLoader is all done
    // then LeakTracker will detect that some resources have not been freed
    // and cause the browser test to fail. This is not a real leak though -
    // normally the async tasks would complete and encounter weak callbacks.
    runner_->Quit();
  }

  void RunMessageLoop() { runner_->Run(); }

  base::WeakPtr<TestCardUnmaskPromptController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

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
    controller_.reset(new TestCardUnmaskPromptController(
        browser()->tab_strip_model()->GetActiveWebContents(), runner_));
    delegate_.reset(new TestCardUnmaskDelegate());
  }

  TestCardUnmaskPromptController* controller() { return controller_.get(); }
  TestCardUnmaskDelegate* delegate() { return delegate_.get(); }

 private:
  // This member must outlive the controller.
  scoped_refptr<content::MessageLoopRunner> runner_;

  scoped_ptr<TestCardUnmaskPromptController> controller_;
  scoped_ptr<TestCardUnmaskDelegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptViewBrowserTest);
};

IN_PROC_BROWSER_TEST_F(CardUnmaskPromptViewBrowserTest, DisplayUI) {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2012");
  credit_card.SetTypeForMaskedCard(kMasterCard);

  controller()->ShowPrompt(credit_card, delegate()->GetWeakPtr());
  controller()->RunMessageLoop();
}

}  // namespace

}  // namespace autofill
