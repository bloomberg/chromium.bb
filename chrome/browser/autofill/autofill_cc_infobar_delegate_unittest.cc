// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_cc_infobar_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
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

class AutofillCCInfobarDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillCCInfobarDelegateTest();
  ~AutofillCCInfobarDelegateTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  scoped_ptr<ConfirmInfoBarDelegate> CreateDelegate();

  scoped_ptr<TestPersonalDataManager> personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillCCInfobarDelegateTest);
};

AutofillCCInfobarDelegateTest::AutofillCCInfobarDelegateTest() {
}

AutofillCCInfobarDelegateTest::~AutofillCCInfobarDelegateTest() {}

void AutofillCCInfobarDelegateTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Ensure Mac OS X does not pop up a modal dialog for the Address Book.
  test::DisableSystemServices(profile()->GetPrefs());

  PersonalDataManagerFactory::GetInstance()->SetTestingFactory(profile(), NULL);

  ChromeAutofillClient::CreateForWebContents(web_contents());
  ChromeAutofillClient* autofill_client =
      ChromeAutofillClient::FromWebContents(web_contents());

  personal_data_.reset(new TestPersonalDataManager());
  personal_data_->set_database(autofill_client->GetDatabase());
  personal_data_->SetPrefService(profile()->GetPrefs());
}

void AutofillCCInfobarDelegateTest::TearDown() {
  personal_data_.reset();
  ChromeRenderViewHostTestHarness::TearDown();
}

scoped_ptr<ConfirmInfoBarDelegate>
AutofillCCInfobarDelegateTest::CreateDelegate() {
  base::HistogramTester histogram_tester;
  CreditCard credit_card;
  scoped_ptr<ConfirmInfoBarDelegate> delegate(AutofillCCInfoBarDelegate::Create(
      ChromeAutofillClient::FromWebContents(web_contents()),
      base::Bind(
          base::IgnoreResult(&TestPersonalDataManager::SaveImportedCreditCard),
          base::Unretained(personal_data_.get()), credit_card)));
  histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar",
                                      AutofillMetrics::INFOBAR_SHOWN, 1);
  return delegate.Pass();
}

// Test that credit card infobar metrics are logged correctly.
TEST_F(AutofillCCInfobarDelegateTest, Metrics) {
  ::testing::InSequence dummy;

  // Accept the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate());
    EXPECT_CALL(*personal_data_, SaveImportedCreditCard(_));

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Accept());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar",
                                        AutofillMetrics::INFOBAR_ACCEPTED, 1);
  }

  // Cancel the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate());

    base::HistogramTester histogram_tester;
    EXPECT_TRUE(infobar->Cancel());
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Dismiss the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate());

    base::HistogramTester histogram_tester;
    infobar->InfoBarDismissed();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar",
                                        AutofillMetrics::INFOBAR_DENIED, 1);
  }

  // Ignore the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate());

    base::HistogramTester histogram_tester;
    infobar.reset();
    histogram_tester.ExpectUniqueSample("Autofill.CreditCardInfoBar",
                                        AutofillMetrics::INFOBAR_IGNORED, 1);
  }
}

}  // namespace autofill
