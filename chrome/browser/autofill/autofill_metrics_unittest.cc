// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD1(Log, void(ServerQueryMetric metric));
  MOCK_CONST_METHOD2(Log, void(QualityMetric metric,
                               const std::string& experiment_id));
  MOCK_CONST_METHOD1(LogProfileCount, void(size_t num_profiles));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

// TODO(isherman): Move this into autofill_common_test.h?
class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    set_metric_logger(new MockAutofillMetrics);
    CreateTestAutoFillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  virtual void InitializeIfNeeded() {}
  virtual void SaveImportedFormData() {}
  virtual bool IsDataLoaded() const { return true; }

  // Overridden to avoid a trip to the database. This should be a no-op except
  // for the side-effect of logging the profile count.
  virtual void LoadProfiles() {
    std::vector<AutoFillProfile*> profiles;
    web_profiles_.release(&profiles);
    WDResult<std::vector<AutoFillProfile*> > result(AUTOFILL_PROFILES_RESULT,
                                                    profiles);
    ReceiveLoadedProfiles(0, &result);
  }

  // Adds |profile| to |web_profiles_| and takes ownership of the profile's
  // memory.
  virtual void AddProfile(AutoFillProfile* profile) {
    web_profiles_.push_back(profile);
  }

  const MockAutofillMetrics* metric_logger() const {
    return static_cast<const MockAutofillMetrics*>(
        PersonalDataManager::metric_logger());
  }

  static void ResetHasLoggedProfileCount() {
    PersonalDataManager::set_has_logged_profile_count(false);
  }

 private:
  void CreateTestAutoFillProfiles(ScopedVector<AutoFillProfile>* profiles) {
    AutoFillProfile* profile = new AutoFillProfile;
    autofill_test::SetProfileInfo(profile, "Elvis", "Aaron",
                                  "Presley", "theking@gmail.com", "RCA",
                                  "3734 Elvis Presley Blvd.", "Apt. 10",
                                  "Memphis", "Tennessee", "38116", "USA",
                                  "12345678901", "");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_test::SetProfileInfo(profile, "Charles", "Hardin",
                                  "Holley", "buddy@gmail.com", "Decca",
                                  "123 Apple St.", "unit 6", "Lubbock",
                                  "Texas", "79401", "USA", "23456789012",
                                  "");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_test::SetProfileInfo(profile, "", "", "", "", "", "", "",
                                  "", "", "", "", "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Elvis Presley",
                                     "4234567890123456", // Visa
                                     "04", "2012");
    credit_card->set_guid("00000000-0000-0000-0000-000000000004");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Buddy Holly",
                                     "5187654321098765", // Mastercard
                                     "10", "2014");
    credit_card->set_guid("00000000-0000-0000-0000-000000000005");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "", "", "", "");
    credit_card->set_guid("00000000-0000-0000-0000-000000000006");
    credit_cards->push_back(credit_card);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(TabContents* tab_contents,
                      TestPersonalDataManager* personal_manager)
      : AutofillManager(tab_contents, personal_manager),
        autofill_enabled_(true) {
    set_metric_logger(new MockAutofillMetrics);
  }
  virtual ~TestAutofillManager() {}

  virtual bool IsAutoFillEnabled() const { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  const MockAutofillMetrics* metric_logger() const {
    return static_cast<const MockAutofillMetrics*>(
        AutofillManager::metric_logger());
  }

  void AddSeenForm(FormStructure* form) {
    form_structures()->push_back(form);
  }

 private:
  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

class TestFormStructure : public FormStructure {
 public:
  explicit TestFormStructure(const FormData& form) : FormStructure(form) {}
  virtual ~TestFormStructure() {}

  void SetFieldTypes(const std::vector<AutofillFieldType>& heuristic_types,
                     const std::vector<AutofillFieldType>& server_types) {
    ASSERT_EQ(field_count(), heuristic_types.size());
    ASSERT_EQ(field_count(), server_types.size());

    for (size_t i = 0; i < field_count(); ++i) {
      AutofillField* field = (*fields())[i];
      ASSERT_TRUE(field);
      field->set_heuristic_type(heuristic_types[i]);
      field->set_server_type(server_types[i]);
    }

    UpdateAutoFillCount();
  }

  virtual std::string server_experiment_id() const OVERRIDE {
    return server_experiment_id_;
  }
  void set_server_experiment_id(const std::string& server_experiment_id) {
    server_experiment_id_ = server_experiment_id;
  }

 private:
  std::string server_experiment_id_;
  DISALLOW_COPY_AND_ASSIGN(TestFormStructure);
};

}  // namespace

class AutofillMetricsTest : public RenderViewHostTestHarness {
 public:
  AutofillMetricsTest() {}
  virtual ~AutofillMetricsTest() {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset(NULL);
    test_personal_data_ = NULL;
  }

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    test_personal_data_ = new TestPersonalDataManager();
    autofill_manager_.reset(new TestAutofillManager(contents(),
                                                    test_personal_data_.get()));
  }

 protected:
  scoped_ptr<TestAutofillManager> autofill_manager_;
  scoped_refptr<TestPersonalDataManager> test_personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetricsTest);
};

// Test that we log quality metrics appropriately.
TEST_F(AutofillMetricsTest, QualityMetrics) {
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
  field.is_autofilled = true;
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

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  autofill_manager_->AddSeenForm(form_structure);

  // Establish our expectations.
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILLED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILL_FAILED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SERVER_TYPE_UNKNOWN, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->OnFormSubmitted(form));
}

// Test that we log the appropriate additional metrics when AutoFill failed.
TEST_F(AutofillMetricsTest, QualityMetricsForFailure) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  struct {
    const char* label;
    const char* name;
    const char* value;
    AutofillFieldType heuristic_type;
    AutofillFieldType server_type;
    AutofillMetrics::QualityMetric heuristic_metric;
    AutofillMetrics::QualityMetric server_metric;
  } failure_cases[] = {
    {
      "Heuristics unknown, server unknown", "0,0", "Elvis",
      UNKNOWN_TYPE, NO_SERVER_DATA,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
      AutofillMetrics::FIELD_SERVER_TYPE_UNKNOWN
    },
    {
      "Heuristics match, server unknown", "1,0", "Aaron",
      NAME_MIDDLE, NO_SERVER_DATA,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_MATCH,
      AutofillMetrics::FIELD_SERVER_TYPE_UNKNOWN
    },
    {
      "Heuristics mismatch, server unknown", "2,0", "Presley",
      PHONE_HOME_NUMBER, NO_SERVER_DATA,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_MISMATCH,
      AutofillMetrics::FIELD_SERVER_TYPE_UNKNOWN
    },
    {
      "Heuristics unknown, server match", "0,1", "theking@gmail.com",
      UNKNOWN_TYPE, EMAIL_ADDRESS,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
      AutofillMetrics::FIELD_SERVER_TYPE_MATCH
    },
    {
      "Heuristics match, server match", "1,1", "3734 Elvis Presley Blvd.",
      ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE1,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_MATCH,
      AutofillMetrics::FIELD_SERVER_TYPE_MATCH
    },
    {
      "Heuristics mismatch, server match", "2,1", "Apt. 10",
      PHONE_HOME_NUMBER, ADDRESS_HOME_LINE2,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_MISMATCH,
      AutofillMetrics::FIELD_SERVER_TYPE_MATCH
    },
    {
      "Heuristics unknown, server mismatch", "0,2", "Memphis",
      UNKNOWN_TYPE, PHONE_HOME_NUMBER,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
      AutofillMetrics::FIELD_SERVER_TYPE_MISMATCH
    },
    {
      "Heuristics match, server mismatch", "1,2", "Tennessee",
      ADDRESS_HOME_STATE, PHONE_HOME_NUMBER,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_MATCH,
      AutofillMetrics::FIELD_SERVER_TYPE_MISMATCH
    },
    {
      "Heuristics mismatch, server mismatch", "2,2", "38116",
      PHONE_HOME_NUMBER, PHONE_HOME_NUMBER,
      AutofillMetrics::FIELD_HEURISTIC_TYPE_MISMATCH,
      AutofillMetrics::FIELD_SERVER_TYPE_MISMATCH
    }
  };

  std::vector<AutofillFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(failure_cases); ++i) {
    FormField field;
    autofill_test::CreateTestFormField(failure_cases[i].label,
                                       failure_cases[i].name,
                                       failure_cases[i].value, "text", &field);
    form.fields.push_back(field);
    heuristic_types.push_back(failure_cases[i].heuristic_type);
    server_types.push_back(failure_cases[i].server_type);

  }

  // Simulate having seen this form with the desired heuristic and server types.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenForm(form_structure);

  // Establish our expectations.
  ::testing::InSequence dummy;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(failure_cases); ++i) {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                Log(AutofillMetrics::FIELD_AUTOFILL_FAILED, std::string()));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                Log(failure_cases[i].heuristic_metric, std::string()));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                Log(failure_cases[i].server_metric, std::string()));
  }

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->OnFormSubmitted(form));
}

// Test that we behave sanely when the cached form differs from the submitted
// one.
TEST_F(AutofillMetricsTest, SaneMetricsWithCacheMismatch) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  std::vector<AutofillFieldType> heuristic_types, server_types;

  FormField field;
  autofill_test::CreateTestFormField(
      "Both match", "match", "Elvis Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);
  autofill_test::CreateTestFormField(
      "Both mismatch", "mismatch", "buddy@gmail.com", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(PHONE_HOME_NUMBER);
  autofill_test::CreateTestFormField(
      "Only heuristics match", "mixed", "Memphis", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_CITY);
  server_types.push_back(PHONE_HOME_NUMBER);
  autofill_test::CreateTestFormField(
      "Unknown", "unknown", "garbage", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form with the desired heuristic and server types.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenForm(form_structure);

  // Add a field and re-arrange the remaining form fields before submitting.
  std::vector<FormField> cached_fields = form.fields;
  form.fields.clear();
  autofill_test::CreateTestFormField(
      "New field", "new field", "Tennessee", "text", &field);
  form.fields.push_back(field);
  form.fields.push_back(cached_fields[2]);
  form.fields.push_back(cached_fields[1]);
  form.fields.push_back(cached_fields[3]);
  form.fields.push_back(cached_fields[0]);

  // Establish our expectations.
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILL_FAILED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SERVER_TYPE_UNKNOWN, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILL_FAILED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_HEURISTIC_TYPE_MATCH, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SERVER_TYPE_MISMATCH, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILL_FAILED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_HEURISTIC_TYPE_MISMATCH,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SERVER_TYPE_MISMATCH, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILLED, std::string()));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->OnFormSubmitted(form));
}

// Test that we don't log quality metrics for non-autofillable forms.
TEST_F(AutofillMetricsTest, NoQualityMetricsForNonAutofillableForms) {
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
  field.is_autofilled = true;
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  form.fields.push_back(field);

  // Simulate form submission.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string())).Times(0);
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->OnFormSubmitted(form));

  // Search forms are not auto-fillable.
  form.action = GURL("http://example.com/search?q=Elvis%20Presley");
  autofill_test::CreateTestFormField(
      "Empty", "empty", "", "text", &field);
  form.fields.push_back(field);

  // Simulate form submission.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, std::string())).Times(0);
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->OnFormSubmitted(form));
}

// Test that we recored the experiment id appropriately.
TEST_F(AutofillMetricsTest, QualityMetricsWithExperimentId) {
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
  field.is_autofilled = true;
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

  const std::string experiment_id = "ThatOughtaDoIt";

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->set_server_experiment_id(experiment_id);
  autofill_manager_->AddSeenForm(form_structure);

  // Establish our expectations.
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILLED, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_AUTOFILL_FAILED, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_HEURISTIC_TYPE_UNKNOWN,
                  experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SERVER_TYPE_UNKNOWN, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              Log(AutofillMetrics::FIELD_SUBMITTED, experiment_id));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->OnFormSubmitted(form));
}

// Test that the profile count is logged correctly.
TEST_F(AutofillMetricsTest, ProfileCount) {
  TestPersonalDataManager::ResetHasLoggedProfileCount();

  // The metric should be logged when the profiles are first loaded.
  EXPECT_CALL(*test_personal_data_->metric_logger(),
              LogProfileCount(3)).Times(1);
  test_personal_data_->LoadProfiles();

  // The metric should only be logged once.
  EXPECT_CALL(*test_personal_data_->metric_logger(),
              LogProfileCount(::testing::_)).Times(0);
  test_personal_data_->LoadProfiles();
}
