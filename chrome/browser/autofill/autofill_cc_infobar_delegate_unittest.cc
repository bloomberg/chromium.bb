// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;

namespace autofill {

namespace {

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD1(LogCreditCardInfoBarMetric, void(InfoBarMetric metric));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() : PersonalDataManager("en-US") {}

  using PersonalDataManager::set_database;
  using PersonalDataManager::SetPrefService;

  // Overridden to avoid a trip to the database.
  virtual void LoadProfiles() OVERRIDE {}
  virtual void LoadCreditCards() OVERRIDE {}

  MOCK_METHOD1(SaveImportedCreditCard,
               std::string(const CreditCard& imported_credit_card));

 private:
  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

}  // namespace

class AutofillCCInfobarDelegateTest : public ChromeRenderViewHostTestHarness {
 public:
  virtual ~AutofillCCInfobarDelegateTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  scoped_ptr<ConfirmInfoBarDelegate> CreateDelegate(
      MockAutofillMetrics* metric_logger);

  scoped_ptr<TestPersonalDataManager> personal_data_;
};

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
AutofillCCInfobarDelegateTest::CreateDelegate(
    MockAutofillMetrics* metric_logger) {
  EXPECT_CALL(*metric_logger,
              LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN));

  CreditCard credit_card;
  return AutofillCCInfoBarDelegate::Create(
      metric_logger,
      base::Bind(
          base::IgnoreResult(&TestPersonalDataManager::SaveImportedCreditCard),
          base::Unretained(personal_data_.get()),
          credit_card));
}

// Test that credit card infobar metrics are logged correctly.
TEST_F(AutofillCCInfobarDelegateTest, Metrics) {
  MockAutofillMetrics metric_logger;
  ::testing::InSequence dummy;

  // Accept the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(&metric_logger));
    ASSERT_TRUE(infobar);
    EXPECT_CALL(*personal_data_, SaveImportedCreditCard(_));
    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_ACCEPTED));

    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED))
        .Times(0);
    EXPECT_TRUE(infobar->Accept());
  }

  // Cancel the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(&metric_logger));
    ASSERT_TRUE(infobar);
    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_DENIED))
        .Times(1);
    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED))
        .Times(0);
    EXPECT_TRUE(infobar->Cancel());
  }

  // Dismiss the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(&metric_logger));
    ASSERT_TRUE(infobar);
    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_DENIED))
        .Times(1);
    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED))
        .Times(0);
    infobar->InfoBarDismissed();
  }

  // Ignore the infobar.
  {
    scoped_ptr<ConfirmInfoBarDelegate> infobar(CreateDelegate(&metric_logger));
    ASSERT_TRUE(infobar);
    EXPECT_CALL(metric_logger,
                LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED))
        .Times(1);
  }
}

}  // namespace autofill
