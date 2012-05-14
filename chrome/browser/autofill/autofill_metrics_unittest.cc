// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_cc_infobar_delegate.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/tab_contents/test_tab_contents_wrapper.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/rect.h"
#include "webkit/forms/form_data.h"
#include "webkit/forms/form_field.h"

using content::BrowserThread;
using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using base::TimeTicks;
using base::TimeDelta;
using webkit::forms::FormData;
using webkit::forms::FormField;

namespace {

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD1(LogCreditCardInfoBarMetric, void(InfoBarMetric metric));
  MOCK_CONST_METHOD3(LogHeuristicTypePrediction,
                     void(FieldTypeQualityMetric metric,
                          AutofillFieldType field_type,
                          const std::string& experiment_id));
  MOCK_CONST_METHOD3(LogOverallTypePrediction,
                     void(FieldTypeQualityMetric metric,
                          AutofillFieldType field_type,
                          const std::string& experiment_id));
  MOCK_CONST_METHOD3(LogServerTypePrediction,
                     void(FieldTypeQualityMetric metric,
                          AutofillFieldType field_type,
                          const std::string& experiment_id));
  MOCK_CONST_METHOD2(LogQualityMetric, void(QualityMetric metric,
                                            const std::string& experiment_id));
  MOCK_CONST_METHOD1(LogServerQueryMetric, void(ServerQueryMetric metric));
  MOCK_CONST_METHOD1(LogUserHappinessMetric, void(UserHappinessMetric metric));
  MOCK_CONST_METHOD1(LogFormFillDurationFromLoadWithAutofill,
                     void(const TimeDelta& duration));
  MOCK_CONST_METHOD1(LogFormFillDurationFromLoadWithoutAutofill,
                     void(const TimeDelta& duration));
  MOCK_CONST_METHOD1(LogFormFillDurationFromInteractionWithAutofill,
                     void(const TimeDelta& duration));
  MOCK_CONST_METHOD1(LogFormFillDurationFromInteractionWithoutAutofill,
                     void(const TimeDelta& duration));
  MOCK_CONST_METHOD1(LogIsAutofillEnabledAtPageLoad, void(bool enabled));
  MOCK_CONST_METHOD1(LogIsAutofillEnabledAtStartup, void(bool enabled));
  MOCK_CONST_METHOD1(LogStoredProfileCount, void(size_t num_profiles));
  MOCK_CONST_METHOD1(LogAddressSuggestionsCount, void(size_t num_suggestions));
  MOCK_CONST_METHOD1(LogServerExperimentIdForQuery,
                     void(const std::string& experiment_id));
  MOCK_CONST_METHOD1(LogServerExperimentIdForUpload,
                     void(const std::string& experiment_id));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() : autofill_enabled_(true) {
    set_metric_logger(new MockAutofillMetrics);
    CreateTestAutofillProfiles(&web_profiles_);
  }

  // Overridden to avoid a trip to the database. This should be a no-op except
  // for the side-effect of logging the profile count.
  virtual void LoadProfiles() OVERRIDE {
    std::vector<AutofillProfile*> profiles;
    web_profiles_.release(&profiles);
    WDResult<std::vector<AutofillProfile*> > result(AUTOFILL_PROFILES_RESULT,
                                                    profiles);
    ReceiveLoadedProfiles(0, &result);
  }

  // Overridden to avoid a trip to the database.
  virtual void LoadCreditCards() OVERRIDE {}

  const MockAutofillMetrics* metric_logger() const {
    return static_cast<const MockAutofillMetrics*>(
        PersonalDataManager::metric_logger());
  }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  virtual bool IsAutofillEnabled() const OVERRIDE {
    return autofill_enabled_;
  }

  MOCK_METHOD1(SaveImportedCreditCard,
               void(const CreditCard& imported_credit_card));

 private:
  void CreateTestAutofillProfiles(ScopedVector<AutofillProfile>* profiles) {
    AutofillProfile* profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "Elvis", "Aaron",
                                  "Presley", "theking@gmail.com", "RCA",
                                  "3734 Elvis Presley Blvd.", "Apt. 10",
                                  "Memphis", "Tennessee", "38116", "USA",
                                  "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "Charles", "Hardin",
                                  "Holley", "buddy@gmail.com", "Decca",
                                  "123 Apple St.", "unit 6", "Lubbock",
                                  "Texas", "79401", "USA", "2345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
  }

  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
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
      AutofillField* form_field = field(i);
      ASSERT_TRUE(form_field);
      form_field->set_heuristic_type(heuristic_types[i]);
      form_field->set_server_type(server_types[i]);
    }

    UpdateAutofillCount();
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

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(TabContentsWrapper* tab_contents,
                      TestPersonalDataManager* personal_manager)
      : AutofillManager(tab_contents, personal_manager),
        autofill_enabled_(true),
        did_finish_async_form_submit_(false),
        message_loop_is_running_(false) {
    set_metric_logger(new MockAutofillMetrics);
  }

  virtual bool IsAutofillEnabled() const { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  MockAutofillMetrics* metric_logger() {
    return static_cast<MockAutofillMetrics*>(const_cast<AutofillMetrics*>(
        AutofillManager::metric_logger()));
  }

  void AddSeenForm(const FormData& form,
                   const std::vector<AutofillFieldType>& heuristic_types,
                   const std::vector<AutofillFieldType>& server_types,
                   const std::string& experiment_id) {
    FormData empty_form = form;
    for (size_t i = 0; i < empty_form.fields.size(); ++i) {
      empty_form.fields[i].value = string16();
    }

    // |form_structure| will be owned by |form_structures()|.
    TestFormStructure* form_structure = new TestFormStructure(empty_form);
    form_structure->SetFieldTypes(heuristic_types, server_types);
    form_structure->set_server_experiment_id(experiment_id);
    form_structures()->push_back(form_structure);
  }

  void FormSubmitted(const FormData& form, const TimeTicks& timestamp) {
    if (!OnFormSubmitted(form, timestamp))
      return;

    // Wait for the asynchronous FormSubmitted() call to complete.
    if (!did_finish_async_form_submit_) {
      // TODO(isherman): It seems silly to need this variable.  Is there some
      // way I can just query the message loop's state?
      message_loop_is_running_ = true;
      MessageLoop::current()->Run();
    } else {
      did_finish_async_form_submit_ = false;
    }
  }

  virtual void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time) OVERRIDE {
    if (message_loop_is_running_) {
      MessageLoop::current()->Quit();
      message_loop_is_running_ = false;
    } else {
      did_finish_async_form_submit_ = true;
    }

    AutofillManager::UploadFormDataAsyncCallback(submitted_form,
                                                 load_time,
                                                 interaction_time,
                                                 submission_time);
  }

 private:
  // AutofillManager is ref counted.
  virtual ~TestAutofillManager() {}

  bool autofill_enabled_;
  bool did_finish_async_form_submit_;
  bool message_loop_is_running_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

}  // namespace

class AutofillMetricsTest : public TabContentsWrapperTestHarness {
 public:
  AutofillMetricsTest();
  virtual ~AutofillMetricsTest();

  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

 protected:
  AutofillCCInfoBarDelegate* CreateDelegate(MockAutofillMetrics* metric_logger,
                                            CreditCard** created_card);

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_refptr<TestAutofillManager> autofill_manager_;
  TestPersonalDataManager personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillMetricsTest);
};

AutofillMetricsTest::AutofillMetricsTest()
  : TabContentsWrapperTestHarness(),
    ui_thread_(BrowserThread::UI, &message_loop_),
    file_thread_(BrowserThread::FILE) {
}

AutofillMetricsTest::~AutofillMetricsTest() {
  // Order of destruction is important as AutofillManager relies on
  // PersonalDataManager to be around when it gets destroyed.
  autofill_manager_ = NULL;
}

void AutofillMetricsTest::SetUp() {
  Profile* profile = new TestingProfile();
  browser_context_.reset(profile);
  PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
      profile, NULL);

  TabContentsWrapperTestHarness::SetUp();
  autofill_manager_ = new TestAutofillManager(contents_wrapper(),
                                              &personal_data_);

  file_thread_.Start();
}

void AutofillMetricsTest::TearDown() {
  file_thread_.Stop();
  TabContentsWrapperTestHarness::TearDown();
}

AutofillCCInfoBarDelegate* AutofillMetricsTest::CreateDelegate(
    MockAutofillMetrics* metric_logger,
    CreditCard** created_card) {
  EXPECT_CALL(*metric_logger,
              LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_SHOWN));
  // The delegate created below will take ownership of this object.
  CreditCard* credit_card = new CreditCard();
  if (created_card)
    *created_card = credit_card;
  return new AutofillCCInfoBarDelegate(contents_wrapper()->infobar_tab_helper(),
                                       credit_card,
                                       &personal_data_,
                                       metric_logger);
}

// Test that we log quality metrics appropriately.
TEST_F(AutofillMetricsTest, QualityMetrics) {
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
      "Autofilled", "autofilled", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  autofill_test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  autofill_test::CreateTestFormField(
      "Empty", "empty", "", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  autofill_test::CreateTestFormField(
      "Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  autofill_test::CreateTestFormField(
      "Select", "select", "USA", "select-one", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  autofill_test::CreateTestFormField(
      "Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_WHOLE_NUMBER);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types,
                                 std::string());

  // Establish our expectations.
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerExperimentIdForUpload(std::string()));
  // Autofilled field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                  NAME_FULL, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  NAME_FULL, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  NAME_FULL, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_AUTOFILLED,
                               std::string()));
  // Non-autofilled field for which we had data
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  EMAIL_ADDRESS, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                  EMAIL_ADDRESS, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                  EMAIL_ADDRESS, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MATCH,
                  std::string()));
  // Empty field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  // Unknown field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  // <select> field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                  ADDRESS_HOME_COUNTRY, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                  ADDRESS_HOME_COUNTRY, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                  ADDRESS_HOME_COUNTRY, std::string()));
  // Phone field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                  PHONE_HOME_WHOLE_NUMBER, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                  PHONE_HOME_WHOLE_NUMBER, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                  PHONE_HOME_WHOLE_NUMBER, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_AUTOFILLED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogUserHappinessMetric(
                  AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form,
                                                           TimeTicks::Now()));
}

// Test that we log the appropriate additional metrics when Autofill failed.
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
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN
    },
    {
      "Heuristics match, server unknown", "1,0", "Aaron",
      NAME_MIDDLE, NO_SERVER_DATA,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN
    },
    {
      "Heuristics mismatch, server unknown", "2,0", "Presley",
      PHONE_HOME_NUMBER, NO_SERVER_DATA,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN
    },
    {
      "Heuristics unknown, server match", "0,1", "theking@gmail.com",
      UNKNOWN_TYPE, EMAIL_ADDRESS,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MATCH
    },
    {
      "Heuristics match, server match", "1,1", "3734 Elvis Presley Blvd.",
      ADDRESS_HOME_LINE1, ADDRESS_HOME_LINE1,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MATCH
    },
    {
      "Heuristics mismatch, server match", "2,1", "Apt. 10",
      PHONE_HOME_NUMBER, ADDRESS_HOME_LINE2,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MATCH
    },
    {
      "Heuristics unknown, server mismatch", "0,2", "Memphis",
      UNKNOWN_TYPE, PHONE_HOME_NUMBER,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MISMATCH
    },
    {
      "Heuristics match, server mismatch", "1,2", "Tennessee",
      ADDRESS_HOME_STATE, PHONE_HOME_NUMBER,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MISMATCH
    },
    {
      "Heuristics mismatch, server mismatch", "2,2", "38116",
      PHONE_HOME_NUMBER, PHONE_HOME_NUMBER,
      AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
      AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MISMATCH
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
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types,
                                 std::string());


  // Establish our expectations.
  ::testing::FLAGS_gmock_verbose = "error";
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerExperimentIdForUpload(std::string()));
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(failure_cases); ++i) {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                                 std::string()));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                                 std::string()));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogQualityMetric(failure_cases[i].heuristic_metric,
                                 std::string()));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogQualityMetric(failure_cases[i].server_metric,
                                 std::string()));
  }

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form,
                                                           TimeTicks::Now()));
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
      "Both match", "match", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);
  autofill_test::CreateTestFormField(
      "Both mismatch", "mismatch", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(PHONE_HOME_NUMBER);
  autofill_test::CreateTestFormField(
      "Only heuristics match", "mixed", "Memphis", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_CITY);
  server_types.push_back(PHONE_HOME_NUMBER);
  autofill_test::CreateTestFormField(
      "Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form with the desired heuristic and server types.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types,
                                 std::string());


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
  // New field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerExperimentIdForUpload(std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                  ADDRESS_HOME_STATE, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                  ADDRESS_HOME_STATE, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                  ADDRESS_HOME_STATE, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_UNKNOWN,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_UNKNOWN,
                  std::string()));
  // Only heuristics match
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                  ADDRESS_HOME_CITY, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  ADDRESS_HOME_CITY, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  ADDRESS_HOME_CITY, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MATCH,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MISMATCH,
                  std::string()));
  // Both mismatch
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  EMAIL_ADDRESS, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  EMAIL_ADDRESS, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                  EMAIL_ADDRESS, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
                  std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MISMATCH,
                  std::string()));
  // Unknown
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  // Both match
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                  NAME_FULL, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                  NAME_FULL, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                  NAME_FULL, std::string()));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_AUTOFILLED,
                               std::string()));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form,
                                                           TimeTicks::Now()));
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
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string())).Times(0);
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form,
                                                           TimeTicks::Now()));

  // Search forms are not auto-fillable.
  form.action = GURL("http://example.com/search?q=Elvis%20Presley");
  autofill_test::CreateTestFormField(
      "Empty", "empty", "", "text", &field);
  form.fields.push_back(field);

  // Simulate form submission.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               std::string())).Times(0);
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form,
                                                           TimeTicks::Now()));
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

  std::vector<AutofillFieldType> heuristic_types, server_types;
  FormField field;

  autofill_test::CreateTestFormField(
      "Autofilled", "autofilled", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  autofill_test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  autofill_test::CreateTestFormField(
      "Empty", "empty", "", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  autofill_test::CreateTestFormField(
      "Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  autofill_test::CreateTestFormField(
      "Select", "select", "USA", "select-one", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  const std::string experiment_id = "ThatOughtaDoIt";

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types,
                                 experiment_id);

  // Establish our expectations.
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerExperimentIdForUpload(experiment_id));
  // Autofilled field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MATCH,
                                         NAME_FULL, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                      NAME_FULL, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                       NAME_FULL, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_AUTOFILLED,
                               experiment_id));
  // Non-autofilled field for which we had data
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_MISMATCH,
                                         EMAIL_ADDRESS, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_MATCH,
                                      EMAIL_ADDRESS, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_MATCH,
                                       EMAIL_ADDRESS, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_NOT_AUTOFILLED,
                               experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_HEURISTIC_TYPE_MISMATCH,
                  experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(
                  AutofillMetrics::NOT_AUTOFILLED_SERVER_TYPE_MATCH,
                  experiment_id));
  // Empty field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               experiment_id));
  // Unknown field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               experiment_id));
  // <select> field
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogQualityMetric(AutofillMetrics::FIELD_SUBMITTED,
                               experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogHeuristicTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                         ADDRESS_HOME_COUNTRY, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogServerTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                      ADDRESS_HOME_COUNTRY, experiment_id));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogOverallTypePrediction(AutofillMetrics::TYPE_UNKNOWN,
                                       ADDRESS_HOME_COUNTRY, experiment_id));

  // Simulate form submission.
  EXPECT_NO_FATAL_FAILURE(autofill_manager_->FormSubmitted(form,
                                                           TimeTicks::Now()));
}

// Test that the profile count is logged correctly.
TEST_F(AutofillMetricsTest, StoredProfileCount) {
  // The metric should be logged when the profiles are first loaded.
  EXPECT_CALL(*personal_data_.metric_logger(),
              LogStoredProfileCount(2)).Times(1);
  personal_data_.LoadProfiles();

  // The metric should only be logged once.
  EXPECT_CALL(*personal_data_.metric_logger(),
              LogStoredProfileCount(::testing::_)).Times(0);
  personal_data_.LoadProfiles();
}

// Test that we correctly log whether Autofill is enabled.
TEST_F(AutofillMetricsTest, AutofillIsEnabledAtStartup) {
  personal_data_.set_autofill_enabled(true);
  EXPECT_CALL(*personal_data_.metric_logger(),
              LogIsAutofillEnabledAtStartup(true)).Times(1);
  personal_data_.Init(profile());
  personal_data_.Shutdown();

  personal_data_.set_autofill_enabled(false);
  EXPECT_CALL(*personal_data_.metric_logger(),
              LogIsAutofillEnabledAtStartup(false)).Times(1);
  personal_data_.Init(profile());
}

// Test that we log the number of Autofill suggestions when filling a form.
TEST_F(AutofillMetricsTest, AddressSuggestionsCount) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormField field;
  std::vector<AutofillFieldType> field_types;
  autofill_test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(NAME_FULL);
  autofill_test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  field_types.push_back(EMAIL_ADDRESS);
  autofill_test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  form.fields.push_back(field);
  field_types.push_back(PHONE_HOME_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types,
                                 std::string());

  // Establish our expectations.
  ::testing::FLAGS_gmock_verbose = "error";
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogAddressSuggestionsCount(2)).Times(1);

  // Simulate activating the autofill popup for the phone field.
  autofill_manager_->OnQueryFormFieldAutofill(
      0, form, field, gfx::Rect(), false);

  // Simulate activating the autofill popup for the email field after typing.
  // No new metric should be logged, since we're still on the same page.
  autofill_test::CreateTestFormField("Email", "email", "b", "email", &field);
  autofill_manager_->OnQueryFormFieldAutofill(
      0, form, field, gfx::Rect(), false);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types,
                                 std::string());

  // Establish our expectations.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogAddressSuggestionsCount(1)).Times(1);

  // Simulate activating the autofill popup for the email field after typing.
  autofill_manager_->OnQueryFormFieldAutofill(
      0, form, field, gfx::Rect(), false);

  // Reset the autofill manager state again.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types,
                                 std::string());

  // Establish our expectations.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogAddressSuggestionsCount(::testing::_)).Times(0);

  // Simulate activating the autofill popup for the email field after typing.
  form.fields[0].is_autofilled = true;
  autofill_manager_->OnQueryFormFieldAutofill(
      0, form, field, gfx::Rect(), false);
}

// Test that we log whether Autofill is enabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillIsEnabledAtPageLoad) {
  // Establish our expectations.
  ::testing::FLAGS_gmock_verbose = "error";
  ::testing::InSequence dummy;
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogIsAutofillEnabledAtPageLoad(true)).Times(1);

  autofill_manager_->set_autofill_enabled(true);
  autofill_manager_->OnFormsSeen(std::vector<FormData>(), TimeTicks());

  // Reset the autofill manager state.
  autofill_manager_->Reset();

  // Establish our expectations.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogIsAutofillEnabledAtPageLoad(false)).Times(1);

  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->OnFormsSeen(std::vector<FormData>(), TimeTicks());
}

// Test that credit card infobar metrics are logged correctly.
TEST_F(AutofillMetricsTest, CreditCardInfoBar) {
  MockAutofillMetrics metric_logger;
  ::testing::InSequence dummy;

  // Accept the infobar.
  {
    CreditCard* credit_card;
    scoped_ptr<InfoBarDelegate> infobar(CreateDelegate(&metric_logger,
                                                       &credit_card));
    EXPECT_CALL(personal_data_, SaveImportedCreditCard(*credit_card));
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_ACCEPTED)).Times(1);
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED)).Times(0);
    EXPECT_TRUE(static_cast<ConfirmInfoBarDelegate*>(infobar.get())->Accept());
  }

  // Cancel the infobar.
  {
    scoped_ptr<InfoBarDelegate> infobar(CreateDelegate(&metric_logger, NULL));
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_DENIED)).Times(1);
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED)).Times(0);
    EXPECT_TRUE(static_cast<ConfirmInfoBarDelegate*>(infobar.get())->Cancel());
  }

  // Dismiss the infobar.
  {
    scoped_ptr<InfoBarDelegate> infobar(CreateDelegate(&metric_logger, NULL));
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_DENIED)).Times(1);
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED)).Times(0);
    infobar->InfoBarDismissed();
  }

  // Ignore the infobar.
  {
    scoped_ptr<InfoBarDelegate> infobar(CreateDelegate(&metric_logger, NULL));
    EXPECT_CALL(metric_logger,
        LogCreditCardInfoBarMetric(AutofillMetrics::INFOBAR_IGNORED)).Times(1);
  }
}

// Test that server query response experiment id metrics are logged correctly.
TEST_F(AutofillMetricsTest, ServerQueryExperimentIdForQuery) {
  MockAutofillMetrics metric_logger;
  ::testing::InSequence dummy;

  // No experiment specified.
  EXPECT_CALL(metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_RECEIVED));
  EXPECT_CALL(metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED));
  EXPECT_CALL(metric_logger,
              LogServerExperimentIdForQuery(std::string()));
  EXPECT_CALL(metric_logger,
              LogServerQueryMetric(
                  AutofillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS));
  FormStructure::ParseQueryResponse(
      "<autofillqueryresponse></autofillqueryresponse>",
      std::vector<FormStructure*>(), metric_logger);

  // Experiment "ar1" specified.
  EXPECT_CALL(metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_RECEIVED));
  EXPECT_CALL(metric_logger,
              LogServerQueryMetric(AutofillMetrics::QUERY_RESPONSE_PARSED));
  EXPECT_CALL(metric_logger,
              LogServerExperimentIdForQuery("ar1"));
  EXPECT_CALL(metric_logger,
              LogServerQueryMetric(
                  AutofillMetrics::QUERY_RESPONSE_MATCHED_LOCAL_HEURISTICS));
  FormStructure::ParseQueryResponse(
      "<autofillqueryresponse experimentid=\"ar1\"></autofillqueryresponse>",
      std::vector<FormStructure*>(), metric_logger);
}

// Verify that we correctly log user happiness metrics dealing with form loading
// and form submission.
TEST_F(AutofillMetricsTest, UserHappinessFormLoadAndSubmission) {
  // Start with a form with insufficiently many fields.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormField field;
  autofill_test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect no notifications when the form is first seen.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED)).Times(0);
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
  }


  // Expect no notifications when the form is submitted.
  {
    EXPECT_CALL(
        *autofill_manager_->metric_logger(),
        LogUserHappinessMetric(
            AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL)).Times(0);
    EXPECT_CALL(
        *autofill_manager_->metric_logger(),
        LogUserHappinessMetric(
            AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME)).Times(0);
    EXPECT_CALL(
        *autofill_manager_->metric_logger(),
        LogUserHappinessMetric(
            AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE)).Times(0);
    EXPECT_CALL(
        *autofill_manager_->metric_logger(),
        LogUserHappinessMetric(
            AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM)).Times(0);
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }

  // Add more fields to the form.
  autofill_test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Unknown", "unknown", "", "text", &field);
  form.fields.push_back(field);
  forms.front() = form;

  // Expect a notification when the form is first seen.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED));
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
  }

  // Expect a notification when the form is submitted.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM));
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }

  // Fill in two of the fields.
  form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  forms.front() = form;

  // Expect a notification when the form is submitted.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM));
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }

  // Fill in the third field.
  form.fields[2].value = ASCIIToUTF16("12345678901");
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_NONE));
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }


  // Mark one of the fields as autofilled.
  form.fields[1].is_autofilled = true;
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_SOME));
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }

  // Mark all of the fillable fields as autofilled.
  form.fields[0].is_autofilled = true;
  form.fields[2].is_autofilled = true;
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUBMITTED_FILLABLE_FORM_AUTOFILLED_ALL));
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }

  // Clear out the third field's value.
  form.fields[2].value = string16();
  forms.front() = form;

  // Expect notifications when the form is submitted.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUBMITTED_NON_FILLABLE_FORM));
    autofill_manager_->FormSubmitted(form, TimeTicks::Now());
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormField field;
  autofill_test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(AutofillMetrics::FORMS_LOADED));
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
  }

  // Simulate typing.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(AutofillMetrics::USER_DID_TYPE));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks());
  }

  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUGGESTIONS_SHOWN)).Times(1);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUGGESTIONS_SHOWN_ONCE)).Times(1);
    autofill_manager_->OnDidShowAutofillSuggestions(true);
    autofill_manager_->OnDidShowAutofillSuggestions(false);
  }

  // Simulate suggestions shown for a different field.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(AutofillMetrics::SUGGESTIONS_SHOWN));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::SUGGESTIONS_SHOWN_ONCE)).Times(0);
    autofill_manager_->OnDidShowAutofillSuggestions(true);
  }

  // Simulate invoking autofill.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::USER_DID_AUTOFILL_ONCE));
    autofill_manager_->OnDidFillAutofillFormData(TimeTicks());
  }

  // Simulate editing an autofilled field.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE));
    AutofillManager::GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
    AutofillManager::GUIDPair empty(std::string(), 0);
    autofill_manager_->OnFillAutofillFormData(
        0, form, form.fields.front(),
        autofill_manager_->PackGUIDs(empty, guid));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks());
  }

  // Simulate invoking autofill again.
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL));
  EXPECT_CALL(*autofill_manager_->metric_logger(),
              LogUserHappinessMetric(
                  AutofillMetrics::USER_DID_AUTOFILL_ONCE)).Times(0);
  autofill_manager_->OnDidFillAutofillFormData(TimeTicks());

  // Simulate editing another autofilled field.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogUserHappinessMetric(
                    AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD));
    autofill_manager_->OnTextFieldDidChange(form, form.fields[1], TimeTicks());
  }
}

// Verify that we correctly log metrics tracking the duration of form fill.
TEST_F(AutofillMetricsTest, FormFillDuration) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  form.user_submitted = true;

  FormField field;
  autofill_test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Fill the field values for form submission.
  form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  form.fields[2].value = ASCIIToUTF16("12345678901");

  // Ignore any non-timing metrics.
  // CAUTION: This is a global variable.  So as to not affect other tests, this
  // _must_ be restored to "warning" at the end of the test.
  ::testing::FLAGS_gmock_verbose = "error";

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithAutofill(_)).Times(0);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithoutAutofill(
                    TimeDelta::FromInternalValue(16)));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithAutofill(_)).Times(0);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithoutAutofill(_)).Times(0);
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->FormSubmitted(form, TimeTicks::FromInternalValue(17));
    autofill_manager_->Reset();
    Mock::VerifyAndClearExpectations(autofill_manager_->metric_logger());
  }

  // Expect metric to be logged if the user manually edited a form field.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithAutofill(_)).Times(0);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithoutAutofill(
                    TimeDelta::FromInternalValue(16)));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithAutofill(_)).Times(0);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithoutAutofill(
                    TimeDelta::FromInternalValue(14)));
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->FormSubmitted(form, TimeTicks::FromInternalValue(17));
    autofill_manager_->Reset();
    Mock::VerifyAndClearExpectations(autofill_manager_->metric_logger());
  }

  // Expect metric to be logged if the user autofilled the form.
  form.fields[0].is_autofilled = true;
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithAutofill(
                    TimeDelta::FromInternalValue(16)));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithoutAutofill(_)).Times(0);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithAutofill(
                    TimeDelta::FromInternalValue(12)));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithoutAutofill(_)).Times(0);
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnDidFillAutofillFormData(
        TimeTicks::FromInternalValue(5));
    autofill_manager_->FormSubmitted(form, TimeTicks::FromInternalValue(17));
    autofill_manager_->Reset();
    Mock::VerifyAndClearExpectations(autofill_manager_->metric_logger());
  }

  // Expect metric to be logged if the user both manually filled some fields
  // and autofilled others.  Messages can arrive out of order, so make sure they
  // take precedence appropriately.
  {
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithAutofill(
                    TimeDelta::FromInternalValue(16)));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromLoadWithoutAutofill(_)).Times(0);
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithAutofill(
                    TimeDelta::FromInternalValue(14)));
    EXPECT_CALL(*autofill_manager_->metric_logger(),
                LogFormFillDurationFromInteractionWithoutAutofill(_)).Times(0);
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnDidFillAutofillFormData(
        TimeTicks::FromInternalValue(5));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->FormSubmitted(form, TimeTicks::FromInternalValue(17));
    autofill_manager_->Reset();
    Mock::VerifyAndClearExpectations(autofill_manager_->metric_logger());
  }

  // Restore the global Gmock verbosity level to its default value.
  ::testing::FLAGS_gmock_verbose = "warning";
}
