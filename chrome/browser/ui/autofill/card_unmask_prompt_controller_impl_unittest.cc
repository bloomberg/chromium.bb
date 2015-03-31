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

using base::ASCIIToUTF16;

class TestCardUnmaskDelegate : public CardUnmaskDelegate {
 public:
  TestCardUnmaskDelegate() : weak_factory_(this) {}

  virtual ~TestCardUnmaskDelegate() {}

  // CardUnmaskDelegate implementation.
  void OnUnmaskResponse(const UnmaskResponse& response) override {
    response_ = response;
  }
  void OnUnmaskPromptClosed() override {}

  const UnmaskResponse& response() { return response_; }

  base::WeakPtr<TestCardUnmaskDelegate> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  UnmaskResponse response_;
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
        can_store_locally_(true),
        runner_(runner),
        weak_factory_(this) {}

  CardUnmaskPromptView* CreateAndShowView() override {
    return test_unmask_prompt_view_;
  }
  void LoadRiskFingerprint() override {
    OnDidLoadRiskFingerprint("risk aversion");
  }
  bool CanStoreLocally() const override { return can_store_locally_; }

  void set_can_store_locally(bool can) { can_store_locally_ = can; }

  base::WeakPtr<TestCardUnmaskPromptController> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  TestCardUnmaskPromptView* test_unmask_prompt_view_;
  bool can_store_locally_;
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

  void ShowPrompt() {
    controller_->ShowPrompt(test::GetMaskedServerCard(),
                            delegate_->GetWeakPtr());
  }

  void ShowPromptAmex() {
    controller_->ShowPrompt(test::GetMaskedServerCardAmex(),
                            delegate_->GetWeakPtr());
  }

  void ShowPromptAndSimulateResponse(bool should_store_pan) {
    ShowPrompt();
    controller_->OnUnmaskResponse(ASCIIToUTF16("444"),
                                  ASCIIToUTF16("01"),
                                  ASCIIToUTF16("2015"),
                                  should_store_pan);
    EXPECT_EQ(
        should_store_pan,
        user_prefs::UserPrefs::Get(web_contents()->GetBrowserContext())
            ->GetBoolean(prefs::kAutofillWalletImportStorageCheckboxState));
  }

 protected:
  void SetImportCheckboxState(bool value) {
    user_prefs::UserPrefs::Get(web_contents()->GetBrowserContext())
        ->SetBoolean(prefs::kAutofillWalletImportStorageCheckboxState, value);
  }

  // This member must outlive the controller.
  scoped_refptr<content::MessageLoopRunner> runner_;

  scoped_ptr<TestCardUnmaskPromptView> test_unmask_prompt_view_;
  scoped_ptr<TestCardUnmaskPromptController> controller_;
  scoped_ptr<TestCardUnmaskDelegate> delegate_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CardUnmaskPromptControllerImplTest);
};

TEST_F(CardUnmaskPromptControllerImplTest, LogShown) {
  base::HistogramTester histogram_tester;
  ShowPrompt();

  histogram_tester.ExpectUniqueSample(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_SHOWN, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogClosedNoAttempts) {
  ShowPrompt();
  base::HistogramTester histogram_tester;
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_CLOSED_NO_ATTEMPTS, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogClosedFailedToUnmaskRetriable) {
  ShowPromptAndSimulateResponse(false);
  controller_->OnVerificationResult(AutofillClient::TRY_AGAIN_FAILURE);
  base::HistogramTester histogram_tester;

  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics
          ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_RETRIABLE_FAILURE,
      1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogClosedFailedToUnmaskNonRetriable)
    {
  ShowPromptAndSimulateResponse(false);
  controller_->OnVerificationResult(AutofillClient::PERMANENT_FAILURE);
  base::HistogramTester histogram_tester;

  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics
          ::UNMASK_PROMPT_CLOSED_FAILED_TO_UNMASK_NON_RETRIABLE_FAILURE,
      1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogUnmaskedCardFirstAttempt) {
  ShowPromptAndSimulateResponse(false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(AutofillClient::SUCCESS);
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_FIRST_ATTEMPT, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogUnmaskedCardAfterFailure) {
  ShowPromptAndSimulateResponse(false);
  controller_->OnVerificationResult(AutofillClient::TRY_AGAIN_FAILURE);
  controller_->OnUnmaskResponse(ASCIIToUTF16("444"),
                                ASCIIToUTF16("01"),
                                ASCIIToUTF16("2015"),
                                false /* should_store_pan */);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(AutofillClient::SUCCESS);
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_UNMASKED_CARD_AFTER_FAILED_ATTEMPTS, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogSavedCardLocally) {
  ShowPromptAndSimulateResponse(true);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(AutofillClient::SUCCESS);
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_SAVED_CARD_LOCALLY, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidOptIn) {
  SetImportCheckboxState(false);
  ShowPromptAndSimulateResponse(true);
  base::HistogramTester histogram_tester;
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidNotOptIn) {
  SetImportCheckboxState(false);
  ShowPromptAndSimulateResponse(false);
  base::HistogramTester histogram_tester;
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidOptOut) {
  SetImportCheckboxState(true);
  ShowPromptAndSimulateResponse(false);
  base::HistogramTester histogram_tester;
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogDidNotOptOut) {
  SetImportCheckboxState(true);
  ShowPromptAndSimulateResponse(true);
  base::HistogramTester histogram_tester;
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, DontLogForHiddenCheckbox) {
  controller_->set_can_store_locally(false);
  ShowPromptAndSimulateResponse(false);
  base::HistogramTester histogram_tester;
  controller_->OnUnmaskDialogClosed();

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_IN, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_IN, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_OPT_OUT, 0);
  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.Events",
      AutofillMetrics::UNMASK_PROMPT_LOCAL_SAVE_DID_NOT_OPT_OUT, 0);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogRealPanResultSuccess) {
  ShowPromptAndSimulateResponse(false);
  base::HistogramTester histogram_tester;
  controller_->OnVerificationResult(AutofillClient::SUCCESS);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult",
      AutofillMetrics::GET_REAL_PAN_RESULT_SUCCESS, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, LogRealPanTryAgainFailure) {
  ShowPromptAndSimulateResponse(false);
  base::HistogramTester histogram_tester;

  controller_->OnVerificationResult(AutofillClient::TRY_AGAIN_FAILURE);

  histogram_tester.ExpectBucketCount(
      "Autofill.UnmaskPrompt.GetRealPanResult",
      AutofillMetrics::GET_REAL_PAN_RESULT_TRY_AGAIN_FAILURE, 1);
}

TEST_F(CardUnmaskPromptControllerImplTest, CvcInputValidation) {
  struct CvcCase {
    const char* input;
    bool valid;
    // null when |valid| is false.
    const char* canonicalized_input;
  };
  CvcCase cvc_cases[] = {
    { "123", true, "123" },
    { "123 ", true, "123" },
    { " 1234 ", false  },
    { "IOU", false  },
  };

  ShowPrompt();

  for (size_t i = 0; i < arraysize(cvc_cases); ++i) {
    EXPECT_EQ(cvc_cases[i].valid,
              controller_->InputCvcIsValid(ASCIIToUTF16(cvc_cases[i].input)));
    if (!cvc_cases[i].valid)
      continue;

    controller_->OnUnmaskResponse(ASCIIToUTF16(cvc_cases[i].input),
                                  base::string16(), base::string16(), false);
    EXPECT_EQ(ASCIIToUTF16(cvc_cases[i].canonicalized_input),
              delegate_->response().cvc);
  }

  CvcCase cvc_cases_amex[] = {
    { "123", false },
    { "123 ", false },
    { "1234", true, "1234" },
    { "\t1234 ", true, "1234" },
    { " 1234", true, "1234" },
    { "IOU$", false  },
  };

  ShowPromptAmex();

  for (size_t i = 0; i < arraysize(cvc_cases_amex); ++i) {
    EXPECT_EQ(
        cvc_cases_amex[i].valid,
        controller_->InputCvcIsValid(ASCIIToUTF16(cvc_cases_amex[i].input)));
    if (!cvc_cases_amex[i].valid)
      continue;

    controller_->OnUnmaskResponse(ASCIIToUTF16(cvc_cases_amex[i].input),
                                  base::string16(), base::string16(), false);
    EXPECT_EQ(ASCIIToUTF16(cvc_cases_amex[i].canonicalized_input),
              delegate_->response().cvc);
  }
}

TEST_F(CardUnmaskPromptControllerImplTest, ExpirationDateValidation) {
  struct {
    const char* input_month;
    const char* input_year;
    bool valid;
  } exp_cases[] = {
      {"01", "2040", true},
      {"1", "2040", true},
      {"1", "40", true},
      {"10", "40", true},
      {"01", "1940", false},
      {"13", "2040", false},
  };

  ShowPrompt();

  for (size_t i = 0; i < arraysize(exp_cases); ++i) {
    EXPECT_EQ(exp_cases[i].valid, controller_->InputExpirationIsValid(
                                      ASCIIToUTF16(exp_cases[i].input_month),
                                      ASCIIToUTF16(exp_cases[i].input_year)));
  }
}

}  // namespace autofill
