// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_save_card_infobar_delegate_mobile.h"

#include <memory>

#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {

namespace {

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() : PersonalDataManager("en-US") {}

  using PersonalDataManager::set_database;
  using PersonalDataManager::SetPrefService;

  // Overridden to avoid a trip to the database.
  void LoadProfiles() override {}
  void LoadCreditCards() override {}

  MOCK_METHOD1(SaveImportedCreditCard,
               std::string(const CreditCard& imported_credit_card));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

}  // namespace

class AutofillSaveCardInfoBarDelegateMobileTest
    : public ChromeRenderViewHostTestHarness {
 public:
  AutofillSaveCardInfoBarDelegateMobileTest();
  ~AutofillSaveCardInfoBarDelegateMobileTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  std::unique_ptr<ConfirmInfoBarDelegate> CreateDelegate(bool is_uploading);

  std::unique_ptr<TestPersonalDataManager> personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillSaveCardInfoBarDelegateMobileTest);
};

AutofillSaveCardInfoBarDelegateMobileTest::
    AutofillSaveCardInfoBarDelegateMobileTest() {}

AutofillSaveCardInfoBarDelegateMobileTest::
    ~AutofillSaveCardInfoBarDelegateMobileTest() {}

void AutofillSaveCardInfoBarDelegateMobileTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  PersonalDataManagerFactory::GetInstance()->SetTestingFactory(profile(), NULL);

  ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromeAutofillClient* autofill_client =
      ChromeAutofillClient::FromWebContents(web_contents());

  personal_data_.reset(new TestPersonalDataManager());
  personal_data_->set_database(autofill_client->GetDatabase());
  personal_data_->SetPrefService(profile()->GetPrefs());
}

void AutofillSaveCardInfoBarDelegateMobileTest::TearDown() {
  personal_data_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

std::unique_ptr<ConfirmInfoBarDelegate>
AutofillSaveCardInfoBarDelegateMobileTest::CreateDelegate(bool is_uploading) {
  base::HistogramTester histogram_tester;
  CreditCard credit_card;
  std::unique_ptr<base::DictionaryValue> legal_message;
  std::unique_ptr<ConfirmInfoBarDelegate> delegate(
      new AutofillSaveCardInfoBarDelegateMobile(
          is_uploading, credit_card, std::move(legal_message),
          base::Bind(base::IgnoreResult(
                         &TestPersonalDataManager::SaveImportedCreditCard),
                     base::Unretained(personal_data_.get()), credit_card)));
  std::string destination = is_uploading ? ".Server" : ".Local";
  histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar"
                                          + destination,
                                      AutofillMetrics::INFOBAR_SHOWN, 1);
  return delegate;
}

// Test that local credit card save infobar metrics are logged correctly.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, Metrics_Local) {
  ::testing::InSequence dummy;

  // Accept the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ false));
    EXPECT_CALL(*personal_data_, SaveImportedCreditCard(_));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // Cancel the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Cancel());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ false));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Local",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
  }
}

// Test that server credit card save infobar metrics are logged correctly.
TEST_F(AutofillSaveCardInfoBarDelegateMobileTest, Metrics_Server) {
  ::testing::InSequence dummy;

  // Accept the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ true));
    EXPECT_CALL(*personal_data_, SaveImportedCreditCard(_));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // Cancel the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Cancel());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Dismiss the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    std::unique_ptr<ConfirmInfoBarDelegate> infobar(
        CreateDelegate(/* is_uploading= */ true));

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar.Server",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
  }
}

}  // namespace autofill
