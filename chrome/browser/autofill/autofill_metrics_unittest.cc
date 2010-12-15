// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

// TODO(isherman): Move this into autofill_common_test.h?
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    CreateTestAutoFillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  virtual void InitializeIfNeeded() {}
  virtual void SaveImportedFormData() {}
  virtual bool IsDataLoaded() const { return true; }

 private:
  void CreateTestAutoFillProfiles(ScopedVector<AutoFillProfile>* profiles) {
    AutoFillProfile* profile = new AutoFillProfile;
    autofill_test::SetProfileInfo(profile, "Home", "Elvis", "Aaron",
                                  "Presley", "theking@gmail.com", "RCA",
                                  "3734 Elvis Presley Blvd.", "Apt. 10",
                                  "Memphis", "Tennessee", "38116", "USA",
                                  "12345678901", "");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_test::SetProfileInfo(profile, "Work", "Charles", "Hardin",
                                  "Holley", "buddy@gmail.com", "Decca",
                                  "123 Apple St.", "unit 6", "Lubbock",
                                  "Texas", "79401", "USA", "23456789012",
                                  "");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_test::SetProfileInfo(profile, "Empty", "", "", "", "", "", "", "",
                                  "", "", "", "", "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "First", "Elvis Presley",
                                     "4234567890123456", // Visa
                                     "04", "2012");
    credit_card->set_guid("00000000-0000-0000-0000-000000000004");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Second", "Buddy Holly",
                                     "5187654321098765", // Mastercard
                                     "10", "2014");
    credit_card->set_guid("00000000-0000-0000-0000-000000000005");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Empty", "", "", "", "");
    credit_card->set_guid("00000000-0000-0000-0000-000000000006");
    credit_cards->push_back(credit_card);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class MockAutoFillMetrics : public AutoFillMetrics {
 public:
  MockAutoFillMetrics() {}
  MOCK_CONST_METHOD1(Log, void(ServerQueryMetric metric));
  MOCK_CONST_METHOD1(Log, void(QualityMetric metric));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutoFillMetrics);
};

class TestAutoFillManager : public AutoFillManager {
 public:
  TestAutoFillManager(TabContents* tab_contents,
                      TestPersonalDataManager* personal_manager)
      : AutoFillManager(tab_contents, personal_manager),
        autofill_enabled_(true) {
    set_metric_logger(new MockAutoFillMetrics);
  }
  virtual ~TestAutoFillManager() {}

  virtual bool IsAutoFillEnabled() const { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  const MockAutoFillMetrics* metric_logger() const {
    return static_cast<const MockAutoFillMetrics*>(
        AutoFillManager::metric_logger());
  }

 private:
  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestAutoFillManager);
};

}  // namespace

class AutoFillMetricsTest : public RenderViewHostTestHarness {
 public:
  AutoFillMetricsTest() {}
  virtual ~AutoFillMetricsTest() {
    // Order of destruction is important as AutoFillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset(NULL);
    test_personal_data_ = NULL;
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    test_personal_data_ = new TestPersonalDataManager();
    autofill_manager_.reset(new TestAutoFillManager(contents(),
                                                    test_personal_data_.get()));
  }

 protected:
  scoped_ptr<TestAutoFillManager> autofill_manager_;
  scoped_refptr<TestPersonalDataManager> test_personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillMetricsTest);
};

// Test that we log quality metrics appropriately.
TEST_F(AutoFillMetricsTest, QualityMetrics) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormField field;
  autofill_test::CreateTestFormField(
      "Autofilled", "autofilled", "Elvis Presley", "text", &field);
  field.set_autofilled(true);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Empty", "empty", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Unknown", "unknown", "garbage", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Select", "select", "USA", "select-one", &field);
  form.fields.push_back(field);

  // Establish our expectations.
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_SUBMITTED));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_AUTOFILLED));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_SUBMITTED));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_AUTOFILL_FAILED ));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_SUBMITTED));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_SUBMITTED));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form));
}

// Test that we don't log quality metrics for non-autofillable forms.
TEST_F(AutoFillMetricsTest, NoQualityMetricsForNonAutoFillableForms) {
  // Forms must include at least three fields to be auto-fillable.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormField field;
  autofill_test::CreateTestFormField(
      "Autofilled", "autofilled", "Elvis Presley", "text", &field);
  field.set_autofilled(true);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  form.fields.push_back(field);

  // Simulate form submission.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_SUBMITTED)).Times(0);
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form));

  // Search forms are not auto-fillable.
  form.action = GURL("http://example.com/search?q=Elvis%20Presley");
  autofill_test::CreateTestFormField(
      "Empty", "empty", "", "text", &field);
  form.fields.push_back(field);

  // Simulate form submission.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutoFillMetrics::FIELD_SUBMITTED)).Times(0);
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form));
}
