// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/card_unmask_prompt_controller_impl.h"

#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_utils.h"

namespace autofill {

CreditCard CreateCard() {
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Bonnie Parker",
                          "2109" /* Mastercard */, "12", "2012");
  credit_card.SetTypeForMaskedCard(kMasterCard);
  return credit_card;
}

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

class TestCardUnmaskPromptView : public CardUnmaskPromptView {
 public:
  void ControllerGone() override {}
  void DisableAndWaitForVerification() override {}
  void GotVerificationResult(const base::string16& error_message,
                             bool allow_retry) override {}
};

class TestCardUnmaskPromptController : public CardUnmaskPromptControllerImpl {
 public:
  TestCardUnmaskPromptController(
      content::WebContents* contents,
      TestCardUnmaskPromptView* test_unmask_prompt_view,
      scoped_refptr<content::MessageLoopRunner> runner)
      : CardUnmaskPromptControllerImpl(contents),
        test_unmask_prompt_view_(test_unmask_prompt_view),
        runner_(runner),
        weak_factory_(this) {}

  CardUnmaskPromptView* CreateAndShowView() override {
    return test_unmask_prompt_view_;
  }
  void LoadRiskFingerprint() override {}

  base::WeakPtr<TestCardUnmaskPromptController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  TestCardUnmaskPromptView* test_unmask_prompt_view_;
  scoped_refptr<content::MessageLoopRunner> runner_;
  base::WeakPtrFactory<TestCardUnmaskPromptController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestCardUnmaskPromptController);
};

class CardUnmaskPromptControllerImplTest
    : public ChromeRenderViewHostTestHarness {
 public:
  CardUnmaskPromptControllerImplTest() {}
  ~CardUnmaskPromptControllerImplTest() override {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    test_unmask_prompt_view_.reset(new TestCardUnmaskPromptView());
    controller_.reset(new TestCardUnmaskPromptController(
        web_contents(), test_unmask_prompt_view_.get(), runner_));
    delegate_.reset(new TestCardUnmaskDelegate());
    SetImportCheckboxState(false);
  }

  void TearDown() override {
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestCardUnmaskPromptController* controller() { return controller_.get(); }
  TestCardUnmaskDelegate* delegate() { return delegate_.get(); }

 protected:
  void SetImportCheckboxState(bool value) {
    user_prefs::UserPrefs::Get(web_contents()->GetBrowserContext())
        ->SetBoolean(prefs::kAutofillWalletImportStorageCheckboxState, value);
  }

 private:
  // This member must outlive the controller.
  scoped_refptr<content::MessageLoopRunner> runner_;

  scoped_ptr<TestCardUnmaskPromptView> test_unmask_prompt_view_;
  scoped_ptr<TestCardUnmaskPromptController> controller_;
  scoped_ptr<TestCardUnmaskDelegate> delegate_;
};

TEST_F(CardUnmaskPromptControllerImplTest, LogShown) {
  base::HistogramTester histogram_tester;

  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());

  histogram_tester.ExpectUniqueSample(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_SHOWN, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogClosedNoAttempts) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogClosedFailedToUnmaskRetriable) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  controller()->OnVerificationResult(AutofillClient::TRY_AGAIN_FAILURE);
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics
          ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE,
      1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogClosedFailedToUnmaskNonRetriable)
    {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  controller()->OnVerificationResult(AutofillClient::PERMANENT_FAILURE);
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics
          ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
      1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogUnmaskedCardFirstAttempt) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  base::HistogramTester histogram_tester;

  controller()->OnVerificationResult(AutofillClient::SUCCESS);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogUnmaskedCardAfterFailure) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  controller()->OnVerificationResult(AutofillClient::TRY_AGAIN_FAILURE);
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("444"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  base::HistogramTester histogram_tester;

  controller()->OnVerificationResult(AutofillClient::SUCCESS);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogSavedCardLocally) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 true /* should_store_pan */);
  base::HistogramTester histogram_tester;

  controller()->OnVerificationResult(AutofillClient::SUCCESS);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_SAVED_CARD_LOCALLY, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidOptIn) {
  SetImportCheckboxState(false);
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 true /* should_store_pan */);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidNotOptIn) {
  SetImportCheckboxState(false);
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidOptOut) {
  SetImportCheckboxState(true);
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidNotOptOut) {
  SetImportCheckboxState(true);
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  base::HistogramTester histogram_tester;

  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 true /* should_store_pan */);
  controller()->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogRealPanResultSuccess) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  base::HistogramTester histogram_tester;

  controller()->OnVerificationResult(AutofillClient::SUCCESS);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult",
      AutofillMetrics::GET_REAL_PAN_RESULT_SUCCESS, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogRealPanTryAgainFailure) {
  controller()->ShowPrompt(CreateCard(), delegate()->GetWeakPtr());
  controller()->OnUnmaskResponse(base::ASCIIToUTF16("123"),
                                 base::ASCIIToUTF16("01"),
                                 base::ASCIIToUTF16("2015"),
                                 false /* should_store_pan */);
  base::HistogramTester histogram_tester;

  controller()->OnVerificationResult(AutofillClient::TRY_AGAIN_FAILURE);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult",
      AutofillMetrics::GET_REAL_PAN_RESULT_TRY_AGAIN_FAILURE, 1);
}

}  // namespace autofill
