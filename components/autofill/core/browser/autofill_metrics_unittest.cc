// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_metrics.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/user_action_tester.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_external_delegate.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/core/common/signin_pref_names.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "components/webdata/common/web_data_results.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::Bucket;
using base::TimeTicks;
using ::testing::ElementsAre;
using ::testing::Matcher;
using ::testing::UnorderedPointwise;

namespace autofill {
namespace {

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmDeveloperEngagementType = ukm::builders::Autofill_DeveloperEngagement;
using UkmInteractedWithFormType = ukm::builders::Autofill_InteractedWithForm;
using UkmSuggestionsShownType = ukm::builders::Autofill_SuggestionsShown;
using UkmSelectedMaskedServerCardType =
    ukm::builders::Autofill_SelectedMaskedServerCard;
using UkmSuggestionFilledType = ukm::builders::Autofill_SuggestionFilled;
using UkmTextFieldDidChangeType = ukm::builders::Autofill_TextFieldDidChange;
using UkmFormSubmittedType = ukm::builders::Autofill_FormSubmitted;
using UkmFieldTypeValidationType = ukm::builders::Autofill_FieldTypeValidation;
using UkmFieldFillStatusType = ukm::builders::Autofill_FieldFillStatus;

using ExpectedUkmMetrics =
    std::vector<std::vector<std::pair<const char*, int64_t>>>;

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager()
      : PersonalDataManager("en-US"),
        autofill_enabled_(true) {
    CreateTestAutofillProfiles(&web_profiles_);
  }

  using PersonalDataManager::set_account_tracker;
  using PersonalDataManager::set_signin_manager;
  using PersonalDataManager::set_database;
  using PersonalDataManager::SetPrefService;

  // Overridden to avoid a trip to the database. This should be a no-op except
  // for the side-effect of logging the profile count.
  void LoadProfiles() override {
    pending_profiles_query_ = 123;
    pending_server_profiles_query_ = 124;
    {
      std::vector<std::unique_ptr<AutofillProfile>> profiles;
      web_profiles_.swap(profiles);
      std::unique_ptr<WDTypedResult> result = base::MakeUnique<
          WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
          AUTOFILL_PROFILES_RESULT, std::move(profiles));
      OnWebDataServiceRequestDone(pending_profiles_query_, std::move(result));
    }
    {
      std::vector<std::unique_ptr<AutofillProfile>> profiles;
      server_profiles_.swap(profiles);
      std::unique_ptr<WDTypedResult> result = base::MakeUnique<
          WDResult<std::vector<std::unique_ptr<AutofillProfile>>>>(
          AUTOFILL_PROFILES_RESULT, std::move(profiles));
      OnWebDataServiceRequestDone(pending_server_profiles_query_,
                                  std::move(result));
    }
  }

  // Overridden to avoid a trip to the database.
  void LoadCreditCards() override {
    pending_creditcards_query_ = 125;
    pending_server_creditcards_query_ = 126;
    {
      std::vector<std::unique_ptr<CreditCard>> credit_cards;
      local_credit_cards_.swap(credit_cards);
      std::unique_ptr<WDTypedResult> result =
          base::MakeUnique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
              AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
      OnWebDataServiceRequestDone(pending_creditcards_query_,
                                  std::move(result));
    }
    {
      std::vector<std::unique_ptr<CreditCard>> credit_cards;
      server_credit_cards_.swap(credit_cards);
      std::unique_ptr<WDTypedResult> result =
          base::MakeUnique<WDResult<std::vector<std::unique_ptr<CreditCard>>>>(
              AUTOFILL_CREDITCARDS_RESULT, std::move(credit_cards));
      OnWebDataServiceRequestDone(pending_server_creditcards_query_,
                                  std::move(result));
    }
  }

  // Overridden to add potential new profiles to the |web_profiles_|. Since
  // there is no database set for the test, the original method would do
  // nothing.
  void SetProfiles(std::vector<AutofillProfile>* profiles) override {
    // Only need to copy all the profiles. This adds any new profiles created at
    // form submission.
    web_profiles_.clear();
    for (const auto& profile : *profiles)
      web_profiles_.push_back(base::MakeUnique<AutofillProfile>(profile));
  }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  // Removes all existing profiles
  void ClearProfiles() {
    web_profiles_.clear();
    Refresh();
  }

  // Removes all existing profiles and creates one profile.
  void RecreateProfile() {
    web_profiles_.clear();

    std::unique_ptr<AutofillProfile> profile =
        base::MakeUnique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "Elvis", "Aaron", "Presley",
                         "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                         "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    web_profiles_.push_back(std::move(profile));

    Refresh();
  }

  // Removes all existing credit cards and creates 0 or 1 local profiles and
  // 0 or 1 server profile according to the parameters.
  void RecreateCreditCards(bool include_local_credit_card,
                           bool include_masked_server_credit_card,
                           bool include_full_server_credit_card) {
    local_credit_cards_.clear();
    server_credit_cards_.clear();
    if (include_local_credit_card) {
      std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>(
          "10000000-0000-0000-0000-000000000001", std::string());
      test::SetCreditCardInfo(credit_card.get(), nullptr, "4111111111111111",
                              "12", "24", "1");
      local_credit_cards_.push_back(std::move(credit_card));
    }
    if (include_masked_server_credit_card) {
      std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>(
          CreditCard::MASKED_SERVER_CARD, "server_id");
      credit_card->set_guid("10000000-0000-0000-0000-000000000002");
      credit_card->SetNetworkForMaskedCard(kDiscoverCard);
      credit_card->SetNumber(ASCIIToUTF16("9424"));
      server_credit_cards_.push_back(std::move(credit_card));
    }
    if (include_full_server_credit_card) {
      std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>(
          CreditCard::FULL_SERVER_CARD, "server_id");
      credit_card->set_guid("10000000-0000-0000-0000-000000000003");
      server_credit_cards_.push_back(std::move(credit_card));
    }
    Refresh();
  }

  // Removes all existing credit cards and creates 1 server card with a bank
  // name.
  void RecreateServerCreditCardsWithBankName() {
    server_credit_cards_.clear();
    std::unique_ptr<CreditCard> credit_card =
        base::MakeUnique<CreditCard>(CreditCard::FULL_SERVER_CARD, "server_id");
    test::SetCreditCardInfo(credit_card.get(), "name", "4111111111111111", "12",
                            "24", "1");
    credit_card->set_guid("10000000-0000-0000-0000-000000000003");
    credit_card->set_bank_name("Chase");
    server_credit_cards_.push_back(std::move(credit_card));
    Refresh();
  }

  bool IsAutofillEnabled() const override { return autofill_enabled_; }

  void CreateAmbiguousProfiles() {
    web_profiles_.clear();
    CreateTestAutofillProfiles(&web_profiles_);

    auto profile = base::MakeUnique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "John", "Decca", "Public",
                         "john@gmail.com", "Company", "123 Main St.", "unit 7",
                         "Springfield", "Texas", "79401", "US", "2345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    web_profiles_.push_back(std::move(profile));
    Refresh();
  }

 private:
  void CreateTestAutofillProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
    std::unique_ptr<AutofillProfile> profile =
        base::MakeUnique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "Elvis", "Aaron", "Presley",
                         "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                         "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(std::move(profile));
    profile = base::MakeUnique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "2345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(std::move(profile));
  }

  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestFormStructure : public FormStructure {
 public:
  explicit TestFormStructure(const FormData& form) : FormStructure(form) {}
  ~TestFormStructure() override {}

  void SetFieldTypes(const std::vector<ServerFieldType>& heuristic_types,
                     const std::vector<ServerFieldType>& server_types) {
    ASSERT_EQ(field_count(), heuristic_types.size());
    ASSERT_EQ(field_count(), server_types.size());

    for (size_t i = 0; i < field_count(); ++i) {
      AutofillField* form_field = field(i);
      ASSERT_TRUE(form_field);
      form_field->set_heuristic_type(heuristic_types[i]);
      form_field->set_overall_server_type(server_types[i]);
    }

    UpdateAutofillCount();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFormStructure);
};

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* autofill_client,
                      TestPersonalDataManager* personal_manager)
      : AutofillManager(driver, autofill_client, personal_manager),
        autofill_enabled_(true) {}
  ~TestAutofillManager() override {}

  bool IsAutofillEnabled() const override { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  void AddSeenForm(const FormData& form,
                   const std::vector<ServerFieldType>& heuristic_types,
                   const std::vector<ServerFieldType>& server_types) {
    FormData empty_form = form;
    for (size_t i = 0; i < empty_form.fields.size(); ++i) {
      empty_form.fields[i].value = base::string16();
    }

    std::unique_ptr<TestFormStructure> form_structure =
        base::MakeUnique<TestFormStructure>(empty_form);
    form_structure->SetFieldTypes(heuristic_types, server_types);
    form_structure->set_form_parsed_timestamp(TimeTicks::Now());
    form_structures()->push_back(std::move(form_structure));

    form_interactions_ukm_logger()->OnFormsParsed(form.origin);
  }

  // Calls AutofillManager::OnWillSubmitForm and waits for it to complete.
  void WillSubmitForm(const FormData& form, const TimeTicks& timestamp) {
    ResetRunLoop();
    if (!OnWillSubmitForm(form, timestamp))
      return;

    // Wait for the asynchronous OnWillSubmitForm() call to complete.
    RunRunLoop();
  }

  // Calls both AutofillManager::OnWillSubmitForm and
  // AutofillManager::OnFormSubmitted.
  void SubmitForm(const FormData& form, const TimeTicks& timestamp) {
    WillSubmitForm(form, timestamp);
    OnFormSubmitted(form);
  }

  // Control the run loop from within tests.
  void ResetRunLoop() { run_loop_.reset(new base::RunLoop()); }
  void RunRunLoop() { run_loop_->Run(); }

  void UploadFormDataAsyncCallback(const FormStructure* submitted_form,
                                   const TimeTicks& load_time,
                                   const TimeTicks& interaction_time,
                                   const TimeTicks& submission_time,
                                   bool observed_submission) override {
    run_loop_->Quit();

    AutofillManager::UploadFormDataAsyncCallback(
        submitted_form, load_time, interaction_time, submission_time,
        observed_submission);
  }

 private:
  bool autofill_enabled_;
  std::unique_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

MATCHER(CompareMetrics, "") {
  const ukm::mojom::UkmMetric* lhs = ::testing::get<0>(arg).get();
  const std::pair<const char*, int64_t>& rhs = ::testing::get<1>(arg);
  return lhs->metric_hash == base::HashMetricName(rhs.first) &&
         lhs->value == rhs.second;
}

void VerifyDeveloperEngagementUkm(
    const ukm::TestAutoSetUkmRecorder& ukm_recorder,
    const FormData& form,
    const std::vector<int64_t>& expected_metric_values) {
  const ukm::mojom::UkmEntry* entry =
      ukm_recorder.GetEntryForEntryName(UkmDeveloperEngagementType::kEntryName);
  ASSERT_NE(nullptr, entry);
  const ukm::UkmSource* source =
      ukm_recorder.GetSourceForSourceId(entry->source_id);
  ASSERT_NE(nullptr, source);
  EXPECT_EQ(form.origin, source->url());

  int expected_metric_value = 0;
  for (const auto it : expected_metric_values)
    expected_metric_value |= 1 << it;

  const std::vector<std::pair<const char*, int64_t>> expected_metrics{
      {UkmDeveloperEngagementType::kDeveloperEngagementName,
       expected_metric_value}};

  EXPECT_THAT(entry->metrics,
              UnorderedPointwise(CompareMetrics(), expected_metrics));
}

MATCHER(CompareMetricsIgnoringMillisecondsSinceFormParsed, "") {
  const ukm::mojom::UkmMetric* lhs = ::testing::get<0>(arg).get();
  const std::pair<const char*, int64_t>& rhs = ::testing::get<1>(arg);
  return lhs->metric_hash == base::HashMetricName(rhs.first) &&
         (lhs->value == rhs.second ||
          (lhs->value > 0 &&
           rhs.first ==
               UkmSuggestionFilledType::kMillisecondsSinceFormParsedName));
}

void VerifyFormInteractionUkm(const ukm::TestAutoSetUkmRecorder& ukm_recorder,
                              const FormData& form,
                              const char* event_name,
                              const ExpectedUkmMetrics& expected_metrics) {
  size_t expected_metrics_index = 0;
  for (size_t i = 0; i < ukm_recorder.entries_count(); ++i) {
    const ukm::mojom::UkmEntry* entry = ukm_recorder.GetEntry(i);
    if (entry->event_hash != base::HashMetricName(event_name))
      continue;

    const ukm::UkmSource* source =
        ukm_recorder.GetSourceForSourceId(entry->source_id);
    ASSERT_NE(nullptr, source);
    EXPECT_EQ(form.origin, source->url());

    ASSERT_LT(expected_metrics_index, expected_metrics.size());
    EXPECT_THAT(
        entry->metrics,
        UnorderedPointwise(CompareMetricsIgnoringMillisecondsSinceFormParsed(),
                           expected_metrics[expected_metrics_index++]));
  }
}

void VerifySubmitFormUkm(const ukm::TestAutoSetUkmRecorder& ukm_recorder,
                         const FormData& form,
                         AutofillMetrics::AutofillFormSubmittedState state) {
  VerifyFormInteractionUkm(
      ukm_recorder, form, UkmFormSubmittedType::kEntryName,
      {{{UkmFormSubmittedType::kAutofillFormSubmittedStateName, state},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
}

void AppendFieldFillStatusUkm(const FormData& form,
                              ExpectedUkmMetrics* expected_metrics) {
  int64_t form_signature = static_cast<int64_t>(CalculateFormSignature(form));
  int64_t metric_type = static_cast<int64_t>(AutofillMetrics::TYPE_SUBMISSION);
  for (const FormFieldData& field : form.fields) {
    int64_t field_signature =
        static_cast<int64_t>(CalculateFieldSignatureForField(field));
    expected_metrics->push_back(
        {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
         {UkmFieldFillStatusType::kFormSignatureName, form_signature},
         {UkmFieldFillStatusType::kFieldSignatureName, field_signature},
         {UkmFieldFillStatusType::kValidationEventName, metric_type},
         {UkmTextFieldDidChangeType::kIsAutofilledName,
          field.is_autofilled ? 1 : 0},
         {UkmFieldFillStatusType::kWasPreviouslyAutofilledName, 0}});
  }
}

void AppendFieldTypeUkm(const FormData& form,
                        const std::vector<ServerFieldType>& heuristic_types,
                        const std::vector<ServerFieldType>& server_types,
                        const std::vector<ServerFieldType>& actual_types,
                        ExpectedUkmMetrics* expected_metrics) {
  ASSERT_EQ(heuristic_types.size(), form.fields.size());
  ASSERT_EQ(server_types.size(), form.fields.size());
  ASSERT_EQ(actual_types.size(), form.fields.size());
  int64_t form_signature = static_cast<int64_t>(CalculateFormSignature(form));
  int64_t metric_type = static_cast<int64_t>(AutofillMetrics::TYPE_SUBMISSION);
  std::vector<int64_t> prediction_sources{
      AutofillMetrics::PREDICTION_SOURCE_HEURISTIC,
      AutofillMetrics::PREDICTION_SOURCE_SERVER,
      AutofillMetrics::PREDICTION_SOURCE_OVERALL};
  for (size_t i = 0; i < form.fields.size(); ++i) {
    const FormFieldData& field = form.fields[i];
    int64_t field_signature =
        static_cast<int64_t>(CalculateFieldSignatureForField(field));
    for (int64_t source : prediction_sources) {
      int64_t predicted_type = static_cast<int64_t>(
          (source == AutofillMetrics::PREDICTION_SOURCE_SERVER
               ? server_types
               : heuristic_types)[i]);
      int64_t actual_type = static_cast<int64_t>(actual_types[i]);
      expected_metrics->push_back(
          {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
           {UkmFieldFillStatusType::kFormSignatureName, form_signature},
           {UkmFieldFillStatusType::kFieldSignatureName, field_signature},
           {UkmFieldFillStatusType::kValidationEventName, metric_type},
           {UkmFieldTypeValidationType::kPredictionSourceName, source},
           {UkmFieldTypeValidationType::kPredictedTypeName, predicted_type},
           {UkmFieldTypeValidationType::kActualTypeName, actual_type}});
    }
  }
}

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {}
  MOCK_METHOD1(ExecuteCommand, void(int));
};

}  // namespace

// This is defined in the autofill_metrics.cc implementation file.
int GetFieldTypeGroupMetric(ServerFieldType field_type,
                            AutofillMetrics::FieldTypeQualityMetric metric);

class AutofillMetricsTest : public testing::Test {
 public:
  ~AutofillMetricsTest() override;

  void SetUp() override;
  void TearDown() override;

 protected:
  void EnableWalletSync();

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  MockAutofillClient autofill_client_;
  std::unique_ptr<AccountTrackerService> account_tracker_;
  std::unique_ptr<FakeSigninManagerBase> signin_manager_;
  std::unique_ptr<TestSigninClient> signin_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestAutofillManager> autofill_manager_;
  std::unique_ptr<TestPersonalDataManager> personal_data_;
  std::unique_ptr<AutofillExternalDelegate> external_delegate_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

AutofillMetricsTest::~AutofillMetricsTest() {
  // Order of destruction is important as AutofillManager relies on
  // PersonalDataManager to be around when it gets destroyed.
  autofill_manager_.reset();
}

void AutofillMetricsTest::SetUp() {
  autofill_client_.SetPrefs(test::PrefServiceForTesting());

  // Ensure Mac OS X does not pop up a modal dialog for the Address Book.
  test::DisableSystemServices(autofill_client_.GetPrefs());

  // Setup identity services.
  signin_client_.reset(new TestSigninClient(autofill_client_.GetPrefs()));
  account_tracker_.reset(new AccountTrackerService());
  account_tracker_->Initialize(signin_client_.get());

  signin_manager_.reset(new FakeSigninManagerBase(signin_client_.get(),
                                                  account_tracker_.get()));
  signin_manager_->Initialize(autofill_client_.GetPrefs());

  personal_data_.reset(new TestPersonalDataManager());
  personal_data_->set_database(autofill_client_.GetDatabase());
  personal_data_->SetPrefService(autofill_client_.GetPrefs());
  personal_data_->set_account_tracker(account_tracker_.get());
  personal_data_->set_signin_manager(signin_manager_.get());
  autofill_driver_.reset(new TestAutofillDriver());
  autofill_manager_.reset(new TestAutofillManager(
      autofill_driver_.get(), &autofill_client_, personal_data_.get()));

  external_delegate_.reset(new AutofillExternalDelegate(
      autofill_manager_.get(),
      autofill_driver_.get()));
  autofill_manager_->SetExternalDelegate(external_delegate_.get());
}

void AutofillMetricsTest::TearDown() {
  // Order of destruction is important as AutofillManager relies on
  // PersonalDataManager to be around when it gets destroyed.
  autofill_manager_.reset();
  autofill_driver_.reset();
  personal_data_.reset();
  signin_manager_->Shutdown();
  signin_manager_.reset();
  account_tracker_->Shutdown();
  account_tracker_.reset();
  signin_client_.reset();
  test::ReenableSystemServices();
  test_ukm_recorder_.Purge();
}

void AutofillMetricsTest::EnableWalletSync() {
  signin_manager_->SetAuthenticatedAccountInfo("12345", "syncuser@example.com");
}

// Test that we log quality metrics appropriately.
TEST_F(AutofillMetricsTest, QualityMetrics) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField(
      "Autofilled", "autofilled", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Empty", "empty", "", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Select", "select", "USA", "select-one", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // Heuristic predictions.
  {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate.Heuristic";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType.Heuristic";

    // Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TRUE_POSITIVE), 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_CITY_AND_NUMBER,
                                AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS,
                                AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_NUMBER,
                                AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);
    // False Positive Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_NUMBER,
                                AutofillMetrics::FALSE_POSITIVE_UNKNOWN),
        1);
    // False Positive Empty:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL,
                                AutofillMetrics::FALSE_POSITIVE_EMPTY),
        1);

    // Sanity Check:
    histogram_tester.ExpectTotalCount(aggregate_histogram, 6);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 7);
  }

  // Server overrides heuristic so Overall and Server are the same predictions
  // (as there were no test fields where server == NO_SERVER_DATA and heuristic
  // != UNKNOWN_TYPE).
  for (const std::string source : {"Server", "Overall"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source;
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source;

    // Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TRUE_POSITIVE),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL,
                                AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FIRST,
                                AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);

    // False Positive Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS,
                                AutofillMetrics::FALSE_POSITIVE_UNKNOWN),
        1);
    // False Positive Empty:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FIRST,
                                AutofillMetrics::FALSE_POSITIVE_EMPTY),
        1);

    // Sanity Check:
    histogram_tester.ExpectTotalCount(aggregate_histogram, 6);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 7);
  }
}

// Tests the true negatives (empty + no prediction and unknown + no prediction)
// and false positives (empty + bad prediction and unknown + bad prediction)
// are counted correctly.

struct QualityMetricsTestCase {
  const ServerFieldType predicted_field_type;
  const ServerFieldType actual_field_type;
};

class QualityMetricsTest
    : public AutofillMetricsTest,
      public testing::WithParamInterface<QualityMetricsTestCase> {
 public:
  const char* ValueForType(ServerFieldType type) {
    switch (type) {
      case EMPTY_TYPE:
        return "";
      case UNKNOWN_TYPE:
        return "unknown";
      case COMPANY_NAME:
        return "RCA";
      case NAME_FIRST:
        return "Elvis";
      case NAME_MIDDLE:
        return "Aaron";
      case NAME_LAST:
        return "Presley";
      case NAME_FULL:
        return "Elvis Aaron Presley";
      case EMAIL_ADDRESS:
        return "buddy@gmail.com";
      case PHONE_HOME_NUMBER:
      case PHONE_HOME_WHOLE_NUMBER:
      case PHONE_HOME_CITY_AND_NUMBER:
        return "2345678901";
      case ADDRESS_HOME_STREET_ADDRESS:
        return "123 Apple St.\nunit 6";
      case ADDRESS_HOME_LINE1:
        return "123 Apple St.";
      case ADDRESS_HOME_LINE2:
        return "unit 6";
      case ADDRESS_HOME_CITY:
        return "Lubbock";
      case ADDRESS_HOME_STATE:
        return "Texas";
      case ADDRESS_HOME_ZIP:
        return "79401";
      case ADDRESS_HOME_COUNTRY:
        return "US";
      case AMBIGUOUS_TYPE:
        // This occurs as both a company name and a middle name once ambiguous
        // profiles are created.
        personal_data_->CreateAmbiguousProfiles();
        return "Decca";

      default:
        NOTREACHED();  // Fall through
        return "unexpected!";
    }
  }

  bool IsExampleOf(AutofillMetrics::FieldTypeQualityMetric metric,
                   ServerFieldType predicted_type,
                   ServerFieldType actual_type) {
    switch (metric) {
      case AutofillMetrics::TRUE_POSITIVE:
        return unknown_equivalent_types_.count(actual_type) == 0 &&
               predicted_type == actual_type;

      case AutofillMetrics::TRUE_NEGATIVE_AMBIGUOUS:
        return actual_type == AMBIGUOUS_TYPE && predicted_type == UNKNOWN_TYPE;

      case AutofillMetrics::TRUE_NEGATIVE_UNKNOWN:
        return actual_type == UNKNOWN_TYPE && predicted_type == UNKNOWN_TYPE;

      case AutofillMetrics::TRUE_NEGATIVE_EMPTY:
        return actual_type == EMPTY_TYPE && predicted_type == UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_POSITIVE_AMBIGUOUS:
        return actual_type == AMBIGUOUS_TYPE && predicted_type != UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_POSITIVE_UNKNOWN:
        return actual_type == UNKNOWN_TYPE && predicted_type != UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_POSITIVE_EMPTY:
        return actual_type == EMPTY_TYPE && predicted_type != UNKNOWN_TYPE;

      // False negative mismatch and false positive mismatch trigger on the same
      // conditions:
      //   - False positive prediction of predicted type
      //   - False negative prediction of actual type
      case AutofillMetrics::FALSE_POSITIVE_MISMATCH:
      case AutofillMetrics::FALSE_NEGATIVE_MISMATCH:
        return unknown_equivalent_types_.count(actual_type) == 0 &&
               actual_type != predicted_type && predicted_type != UNKNOWN_TYPE;

      case AutofillMetrics::FALSE_NEGATIVE_UNKNOWN:
        return unknown_equivalent_types_.count(actual_type) == 0 &&
               actual_type != predicted_type && predicted_type == UNKNOWN_TYPE;

      default:
        NOTREACHED();
    }
    return false;
  }

  static int FieldTypeCross(ServerFieldType predicted_type,
                            ServerFieldType actual_type) {
    EXPECT_LE(predicted_type, UINT16_MAX);
    EXPECT_LE(actual_type, UINT16_MAX);
    return (predicted_type << 16) | actual_type;
  }

  const ServerFieldTypeSet unknown_equivalent_types_{UNKNOWN_TYPE, EMPTY_TYPE,
                                                     AMBIGUOUS_TYPE};
};

TEST_P(QualityMetricsTest, Classification) {
  const std::vector<std::string> prediction_sources{"Heuristic", "Server",
                                                    "Overall"};
  // Setup the test parameters.
  ServerFieldType actual_field_type = GetParam().actual_field_type;
  ServerFieldType predicted_type = GetParam().predicted_field_type;

  VLOG(2) << "Test Case = Predicted: "
          << AutofillType(predicted_type).ToString() << "; "
          << "Actual: " << AutofillType(actual_field_type).ToString();

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types, actual_types;
  AutofillField field;

  // Add a first name field, that is predicted correctly.
  test::CreateTestFormField("first", "first", ValueForType(NAME_FIRST), "text",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FIRST);
  server_types.push_back(NAME_FIRST);
  actual_types.push_back(NAME_FIRST);

  // Add a last name field, that is predicted correctly.
  test::CreateTestFormField("last", "last", ValueForType(NAME_LAST), "test",
                            &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_LAST);
  server_types.push_back(NAME_LAST);
  actual_types.push_back(NAME_LAST);

  // Add an empty or unknown field, that is predicted as per the test params.
  test::CreateTestFormField("Unknown", "Unknown",
                            ValueForType(actual_field_type), "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(predicted_type);
  server_types.push_back(predicted_type == UNKNOWN_TYPE ? NO_SERVER_DATA
                                                        : predicted_type);
  // Resolve any field type ambiguity.
  if (actual_field_type == AMBIGUOUS_TYPE) {
    if (predicted_type == COMPANY_NAME || predicted_type == NAME_MIDDLE)
      actual_field_type = predicted_type;
  }
  actual_types.push_back(actual_field_type);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Run the form submission code while tracking the histograms.
  base::HistogramTester histogram_tester;
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  ExpectedUkmMetrics expected_ukm_metrics;
  AppendFieldTypeUkm(form, heuristic_types, server_types, actual_types,
                     &expected_ukm_metrics);
  VerifyFormInteractionUkm(test_ukm_recorder_, form,
                           UkmFieldTypeValidationType::kEntryName,
                           expected_ukm_metrics);

  // Validate the total samples and the crossed (predicted-to-actual) samples.
  for (const auto& source : prediction_sources) {
    const std::string crossed_histogram = "Autofill.FieldPrediction." + source;
    const std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source;
    const std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source;

    // Sanity Check:
    histogram_tester.ExpectTotalCount(crossed_histogram, 3);
    histogram_tester.ExpectTotalCount(aggregate_histogram, 3);
    histogram_tester.ExpectTotalCount(
        by_field_type_histogram,
        2 +
            (predicted_type != UNKNOWN_TYPE &&
             predicted_type != actual_field_type) +
            (unknown_equivalent_types_.count(actual_field_type) == 0));

    // The Crossed Histogram:
    histogram_tester.ExpectBucketCount(
        crossed_histogram, FieldTypeCross(NAME_FIRST, NAME_FIRST), 1);
    histogram_tester.ExpectBucketCount(crossed_histogram,
                                       FieldTypeCross(NAME_LAST, NAME_LAST), 1);
    histogram_tester.ExpectBucketCount(
        crossed_histogram,
        FieldTypeCross((source == "Server" && predicted_type == UNKNOWN_TYPE
                            ? NO_SERVER_DATA
                            : predicted_type),
                       actual_field_type),
        1);
  }

  // Validate the individual histogram counter values.
  for (int i = 0; i < AutofillMetrics::NUM_FIELD_TYPE_QUALITY_METRICS; ++i) {
    // The metric enum value we're currently examining.
    auto metric = static_cast<AutofillMetrics::FieldTypeQualityMetric>(i);

    // The type specific expected count is 1 if (predicted, actual) is an
    // example
    int basic_expected_count =
        IsExampleOf(metric, predicted_type, actual_field_type) ? 1 : 0;

    // For aggregate metrics don't capture aggregate FALSE_POSITIVE_MISMATCH.
    // Note there are two true positive values (first and last name) hard-
    // coded into the test.
    int aggregate_expected_count =
        (metric == AutofillMetrics::TRUE_POSITIVE ? 2 : 0) +
        (metric == AutofillMetrics::FALSE_POSITIVE_MISMATCH
             ? 0
             : basic_expected_count);

    // If this test exercises the ambiguous middle name match, then validation
    // of the name-specific metrics must include the true-positives created by
    // the first and last name fields.
    if (metric == AutofillMetrics::TRUE_POSITIVE &&
        predicted_type == NAME_MIDDLE && actual_field_type == NAME_MIDDLE) {
      basic_expected_count += 2;
    }

    // For metrics keyed to the actual field type, we don't capture unknown,
    // empty or ambiguous and we don't capture false positive mismatches.
    int expected_count_for_actual_type =
        (unknown_equivalent_types_.count(actual_field_type) == 0 &&
         metric != AutofillMetrics::FALSE_POSITIVE_MISMATCH)
            ? basic_expected_count
            : 0;

    // For metrics keyed to the predicted field type, we don't capture unknown
    // (empty is not a predictable value) and we don't capture false negative
    // mismatches.
    int expected_count_for_predicted_type =
        (predicted_type != UNKNOWN_TYPE &&
         metric != AutofillMetrics::FALSE_NEGATIVE_MISMATCH)
            ? basic_expected_count
            : 0;

    for (const auto& source : prediction_sources) {
      std::string aggregate_histogram =
          "Autofill.FieldPredictionQuality.Aggregate." + source;
      std::string by_field_type_histogram =
          "Autofill.FieldPredictionQuality.ByFieldType." + source;
      histogram_tester.ExpectBucketCount(aggregate_histogram, metric,
                                         aggregate_expected_count);
      histogram_tester.ExpectBucketCount(
          by_field_type_histogram,
          GetFieldTypeGroupMetric(actual_field_type, metric),
          expected_count_for_actual_type);
      histogram_tester.ExpectBucketCount(
          by_field_type_histogram,
          GetFieldTypeGroupMetric(predicted_type, metric),
          expected_count_for_predicted_type);
    }
  }
}

INSTANTIATE_TEST_CASE_P(
    AutofillMetricsTest,
    QualityMetricsTest,
    testing::Values(QualityMetricsTestCase{UNKNOWN_TYPE, EMPTY_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, UNKNOWN_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{UNKNOWN_TYPE, EMAIL_ADDRESS},
                    QualityMetricsTestCase{EMAIL_ADDRESS, EMPTY_TYPE},
                    QualityMetricsTestCase{EMAIL_ADDRESS, UNKNOWN_TYPE},
                    QualityMetricsTestCase{EMAIL_ADDRESS, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{EMAIL_ADDRESS, EMAIL_ADDRESS},
                    QualityMetricsTestCase{EMAIL_ADDRESS, COMPANY_NAME},
                    QualityMetricsTestCase{COMPANY_NAME, EMAIL_ADDRESS},
                    QualityMetricsTestCase{NAME_MIDDLE, AMBIGUOUS_TYPE},
                    QualityMetricsTestCase{COMPANY_NAME, AMBIGUOUS_TYPE}));

// Ensures that metrics that measure timing some important Autofill functions
// actually are recorded and retrieved.
TEST_F(AutofillMetricsTest, TimingMetrics) {
  base::HistogramTester histogram_tester;

  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField(
      "Autofilled", "autofilled", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);

  test::CreateTestFormField(
      "Autofill Failed", "autofillfailed", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);

  // Simulate a OnFormsSeen() call that should trigger the recording.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());

  // Because these metrics are related to timing, it is not possible to know in
  // advance which bucket the sample will fall into, so we just need to make
  // sure we have valid samples.
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.DetermineHeuristicTypes")
          .empty());
  EXPECT_FALSE(
      histogram_tester.GetAllSamples("Autofill.Timing.ParseForm").empty());
}

// Test that we log quality metrics appropriately when an upload is triggered
// but no submission event is sent.
TEST_F(AutofillMetricsTest, QualityMetrics_NoSubmission) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Empty", "empty", "", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FIRST);

  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Select", "select", "USA", "select-one", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(NO_SERVER_DATA);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate text input on one of the fields.
  autofill_manager_->OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());

  // Trigger a form upload and metrics by Resetting the manager.
  base::HistogramTester histogram_tester;

  autofill_manager_->ResetRunLoop();
  autofill_manager_->Reset();
  autofill_manager_->RunRunLoop();

  // Heuristic predictions.
  {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate.Heuristic.NoSubmission";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType.Heuristic.NoSubmission";
    // False Negative:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TRUE_POSITIVE), 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS,
                                AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_NUMBER,
                                AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);
    // False Positives:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL,
                                AutofillMetrics::FALSE_POSITIVE_EMPTY),
        1);
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_NUMBER,
                                AutofillMetrics::FALSE_POSITIVE_UNKNOWN),
        1);

    // Sanity Check:
    histogram_tester.ExpectTotalCount(aggregate_histogram, 6);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 7);
  }

  // Server predictions override heuristics, so server and overall will be the
  // same.
  for (const std::string source : {"Server", "Overall"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source + ".NoSubmission";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source +
        ".NoSubmission";

    // Unknown.
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS, AutofillMetrics::TRUE_POSITIVE),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::TRUE_POSITIVE),
        1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL,
                                AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FIRST,
                                AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);

    // False Positives:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_EMPTY, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FIRST,
                                AutofillMetrics::FALSE_POSITIVE_EMPTY),
        1);
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_POSITIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS,
                                AutofillMetrics::FALSE_POSITIVE_UNKNOWN),
        1);

    // Sanity Check:
    histogram_tester.ExpectTotalCount(aggregate_histogram, 6);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 7);
  }
}

// Test that we log quality metrics for heuristics and server predictions based
// on autocomplete attributes present on the fields.
TEST_F(AutofillMetricsTest, QualityMetrics_BasedOnAutocomplete) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  // Heuristic value will match with Autocomplete attribute.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  // Heuristic value will NOT match with Autocomplete attribute.
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "additional-name";
  form.fields.push_back(field);

  // Heuristic value will be unknown.
  test::CreateTestFormField("Garbage label", "garbage", "", "text", &field);
  field.autocomplete_attribute = "postal-code";
  form.fields.push_back(field);

  // No autocomplete attribute. No metric logged.
  test::CreateTestFormField("Address", "address", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  std::unique_ptr<TestFormStructure> form_structure =
      base::MakeUnique<TestFormStructure>(form);
  TestFormStructure* form_structure_ptr = form_structure.get();
  form_structure->DetermineHeuristicTypes(nullptr /* ukm_recorder */);
  autofill_manager_->form_structures()->push_back(std::move(form_structure));

  AutofillQueryResponseContents response;
  // Server response will match with autocomplete.
  response.add_field()->set_overall_type_prediction(NAME_LAST);
  // Server response will NOT match with autocomplete.
  response.add_field()->set_overall_type_prediction(NAME_FIRST);
  // Server response will have no data.
  response.add_field()->set_overall_type_prediction(NO_SERVER_DATA);
  // Not logged.
  response.add_field()->set_overall_type_prediction(NAME_MIDDLE);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::vector<std::string> signatures;
  signatures.push_back(form_structure_ptr->FormSignatureAsStr());

  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(response_string, signatures);

  // Verify that FormStructure::ParseQueryResponse was called (here and below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);

  // Autocomplete-derived types are eventually what's inferred.
  EXPECT_EQ(NAME_LAST, form_structure_ptr->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE,
            form_structure_ptr->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure_ptr->field(2)->Type().GetStorableType());

  for (const std::string source : {"Heuristic", "Server"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source +
        ".BasedOnAutocomplete";
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source +
        ".BasedOnAutocomplete";

    // Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_ZIP,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_LAST, AutofillMetrics::TRUE_POSITIVE), 1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_MISMATCH, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FIRST,
                                AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_MIDDLE,
                                AutofillMetrics::FALSE_POSITIVE_MISMATCH),
        1);

    // Sanity check.
    histogram_tester.ExpectTotalCount(aggregate_histogram, 3);
    histogram_tester.ExpectTotalCount(by_field_type_histogram, 4);
  }
}

// Test that we log UPI Virtual Payment Address.
TEST_F(AutofillMetricsTest, UpiVirtualPaymentAddress) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;
  FormFieldData field;

  // Heuristic value will match with Autocomplete attribute.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_LAST);
  server_types.push_back(NAME_LAST);

  // Heuristic value will NOT match with Autocomplete attribute.
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FIRST);
  server_types.push_back(NAME_FIRST);

  // Heuristic value will NOT match with Autocomplete attribute.
  test::CreateTestFormField("Payment Address", "payment_address", "user@upi",
                            "text", &field);
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_LINE1);
  server_types.push_back(ADDRESS_HOME_LINE1);

  // Simulate having seen this form on page load.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  histogram_tester.ExpectBucketCount(
      "Autofill.UserHappiness", AutofillMetrics::USER_DID_ENTER_UPI_VPA, 1);
  histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                     AutofillMetrics::USER_DID_ENTER_UPI_VPA,
                                     1);
  histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
  histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Password", 0);
  histogram_tester.ExpectTotalCount("Autofill.UserHappiness.UnknownFormType",
                                    0);
}

// Verify that when a field is annotated with the autocomplete attribute, its
// predicted type is remembered when quality metrics are logged.
TEST_F(AutofillMetricsTest, PredictedMetricsWithAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field1;
  test::CreateTestFormField("Select", "select", "USA", "select-one", &field1);
  field1.autocomplete_attribute = "country";
  form.fields.push_back(field1);

  // Two other fields to have the minimum of 3 to be parsed by autofill. Note
  // that they have default values not found in the user profiles. They will be
  // changed between the time the form is seen/parsed, and the time it is
  // submitted.
  FormFieldData field2;
  test::CreateTestFormField("Unknown", "Unknown", "", "text", &field2);
  form.fields.push_back(field2);
  FormFieldData field3;
  test::CreateTestFormField("Phone", "phone", "", "tel", &field3);
  form.fields.push_back(field3);

  std::vector<FormData> forms(1, form);

  base::HistogramTester histogram_tester;
  autofill_manager_->OnFormsSeen(forms, TimeTicks());

  // We change the value of the text fields to change the default/seen values
  // (hence the values are not cleared in UpdateFromCache). The new values
  // match what is in the test profile.
  form.fields[1].value = base::ASCIIToUTF16("79401");
  form.fields[2].value = base::ASCIIToUTF16("2345678901");
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  for (const std::string source : {"Heuristic", "Server", "Overall"}) {
    std::string histogram_name =
        "Autofill.FieldPredictionQuality.ByFieldType." + source;
    // First verify that country was not predicted by client or server.
    histogram_tester.ExpectBucketCount(
        histogram_name,
        GetFieldTypeGroupMetric(ADDRESS_HOME_COUNTRY,
                                source == "Overall"
                                    ? AutofillMetrics::TRUE_POSITIVE
                                    : AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);

    // We did not predict zip code or phone number, because they did not have
    // |autocomplete_attribute|, nor client or server predictions.
    histogram_tester.ExpectBucketCount(
        histogram_name,
        GetFieldTypeGroupMetric(ADDRESS_HOME_ZIP,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    histogram_tester.ExpectBucketCount(
        histogram_name,
        GetFieldTypeGroupMetric(PHONE_HOME_WHOLE_NUMBER,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);

    // Sanity check.
    histogram_tester.ExpectTotalCount(histogram_name, 3);
  }
}

// Test that we behave sanely when the cached form differs from the submitted
// one.
TEST_F(AutofillMetricsTest, SaneMetricsWithCacheMismatch) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;

  FormFieldData field;
  test::CreateTestFormField(
      "Both match", "match", "Elvis Aaron Presley", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);
  test::CreateTestFormField(
      "Both mismatch", "mismatch", "buddy@gmail.com", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_NUMBER);
  server_types.push_back(PHONE_HOME_NUMBER);
  test::CreateTestFormField(
      "Only heuristics match", "mixed", "Memphis", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(ADDRESS_HOME_CITY);
  server_types.push_back(PHONE_HOME_NUMBER);
  test::CreateTestFormField("Unknown", "unknown", "garbage", "text", &field);
  field.is_autofilled = false;
  form.fields.push_back(field);
  heuristic_types.push_back(UNKNOWN_TYPE);
  server_types.push_back(UNKNOWN_TYPE);

  // Simulate having seen this form with the desired heuristic and server types.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);


  // Add a field and re-arrange the remaining form fields before submitting.
  std::vector<FormFieldData> cached_fields = form.fields;
  form.fields.clear();
  test::CreateTestFormField(
      "New field", "new field", "Tennessee", "text", &field);
  form.fields.push_back(field);
  form.fields.push_back(cached_fields[2]);
  form.fields.push_back(cached_fields[1]);
  form.fields.push_back(cached_fields[3]);
  form.fields.push_back(cached_fields[0]);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  for (const std::string source : {"Heuristic", "Server", "Overall"}) {
    std::string aggregate_histogram =
        "Autofill.FieldPredictionQuality.Aggregate." + source;
    std::string by_field_type_histogram =
        "Autofill.FieldPredictionQuality.ByFieldType." + source;

    // Unknown:
    histogram_tester.ExpectBucketCount(
        aggregate_histogram, AutofillMetrics::FALSE_NEGATIVE_UNKNOWN, 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_STATE,
                                AutofillMetrics::FALSE_NEGATIVE_UNKNOWN),
        1);
    // Match:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::TRUE_POSITIVE,
                                       source == "Heuristic" ? 2 : 1);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(NAME_FULL, AutofillMetrics::TRUE_POSITIVE), 1);
    // Mismatch:
    histogram_tester.ExpectBucketCount(aggregate_histogram,
                                       AutofillMetrics::FALSE_NEGATIVE_MISMATCH,
                                       source == "Heuristic" ? 1 : 2);
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(EMAIL_ADDRESS,
                                AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
    // Source dependent:
    histogram_tester.ExpectBucketCount(
        by_field_type_histogram,
        GetFieldTypeGroupMetric(ADDRESS_HOME_CITY,
                                source == "Heuristic"
                                    ? AutofillMetrics::TRUE_POSITIVE
                                    : AutofillMetrics::FALSE_NEGATIVE_MISMATCH),
        1);
  }
}

// Verify that when submitting an autofillable form, the stored profile metric
// is logged.
TEST_F(AutofillMetricsTest, StoredProfileCountAutofillableFormSubmission) {
  // Construct a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->OnFormsSeen(forms, TimeTicks());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // An autofillable form was submitted, and the number of stored profiles is
  // logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 2, 1);
}

// Verify that when submitting a non-autofillable form, the stored profile
// metric is not logged.
TEST_F(AutofillMetricsTest, StoredProfileCountNonAutofillableFormSubmission) {
  // Construct a non-fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  // Two fields is not enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Simulate form submission.
  base::HistogramTester histogram_tester;
  autofill_manager_->OnFormsSeen(forms, TimeTicks());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // A non-autofillable form was submitted, and number of stored profiles is NOT
  // logged.
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredProfileCountAtAutofillableFormSubmission", 0);
}

// Verify that when submitting an autofillable form, the proper number of edited
// fields is logged.
TEST_F(AutofillMetricsTest, NumberOfEditedAutofilledFields) {
  // Construct a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first and second fields.
  autofill_manager_->OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());
  autofill_manager_->OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                          TimeTicks());

  // Simulate form submission.
  autofill_manager_->SubmitForm(form, TimeTicks::Now());

  // An autofillable form was submitted, and the number of edited autofilled
  // fields is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfEditedAutofilledFieldsAtSubmission", 2, 1);
}

// Verify that when resetting the autofill manager (such as during a
// navigation), the proper number of edited fields is logged.
TEST_F(AutofillMetricsTest, NumberOfEditedAutofilledFields_NoSubmission) {
  // Construct a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<ServerFieldType> heuristic_types, server_types;

  // Three fields is enough to make it an autofillable form.
  FormFieldData field;
  test::CreateTestFormField("Autofilled", "autofilled", "Elvis Aaron Presley",
                            "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(NAME_FULL);
  server_types.push_back(NAME_FULL);

  test::CreateTestFormField("Autofill Failed", "autofillfailed",
                            "buddy@gmail.com", "text", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(EMAIL_ADDRESS);
  server_types.push_back(EMAIL_ADDRESS);

  test::CreateTestFormField("Phone", "phone", "2345678901", "tel", &field);
  field.is_autofilled = true;
  form.fields.push_back(field);
  heuristic_types.push_back(PHONE_HOME_CITY_AND_NUMBER);
  server_types.push_back(PHONE_HOME_CITY_AND_NUMBER);

  autofill_manager_->AddSeenForm(form, heuristic_types, server_types);

  base::HistogramTester histogram_tester;
  // Simulate text input in the first field.
  autofill_manager_->OnTextFieldDidChange(form, form.fields[0], gfx::RectF(),
                                          TimeTicks());

  // We expect metrics to be logged when the manager is reset.
  autofill_manager_->ResetRunLoop();
  autofill_manager_->Reset();
  autofill_manager_->RunRunLoop();

  // An autofillable form was uploaded, and the number of edited autofilled
  // fields is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.NumberOfEditedAutofilledFieldsAtSubmission.NoSubmission", 1, 1);
}

// Verify that we correctly log metrics regarding developer engagement.
TEST_F(AutofillMetricsTest, DeveloperEngagement) {
  // Start with a non-fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Ensure no metrics are logged when loading a non-fillable form.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectTotalCount("Autofill.DeveloperEngagement", 0);
  }

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Expect the "form parsed without hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectUniqueSample(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS, 1);
  }

  // Add some fields with an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "email";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  forms.back().fields.push_back(field);

  // Expect the "form parsed with field type hints" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1);

    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT, 0);
  }

  // Add a field with an author-specified UPI-VPA field type in the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "upi-vpa";
  forms.back().fields.push_back(field);

  // Expect the "form parsed with type hints" metric, and the
  // "author-specified upi-vpa type" metric to be logged.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    autofill_manager_->Reset();
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.DeveloperEngagement",
        AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT, 1);
  }
}

// Verify that we correctly log UKM for form parsed without type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithoutTypeHints) {
  // Start with a non-fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Ensure no metrics are logged when loading a non-fillable form.
  {
    autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());
    autofill_manager_->Reset();

    EXPECT_EQ(0U, test_ukm_recorder_.sources_count());
    EXPECT_EQ(0U, test_ukm_recorder_.entries_count());
  }

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());
    autofill_manager_->Reset();

    ASSERT_EQ(1U, test_ukm_recorder_.entries_count());
    ASSERT_EQ(1U, test_ukm_recorder_.sources_count());
    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form,
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS});
  }
}

// Verify that we correctly log UKM for form parsed with type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest,
       UkmDeveloperEngagement_LogFillableFormParsedWithTypeHints) {
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Add another field to the form, so that it becomes fillable.
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  forms.back().fields.push_back(field);

  // Add some fields with an author-specified field type to the form.
  // We need to add at least three fields, because a form must have at least
  // three fillable fields to be considered to be autofillable; and if at least
  // one field specifies an explicit type hint, we don't apply any of our usual
  // local heuristics to detect field types in the rest of the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "email";
  forms.back().fields.push_back(field);
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  forms.back().fields.push_back(field);

  // Expect the "form parsed without field type hints" metric and the
  // "form loaded" form interaction event to be logged.
  {
    autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());
    autofill_manager_->Reset();

    ASSERT_EQ(1U, test_ukm_recorder_.entries_count());
    ASSERT_EQ(1U, test_ukm_recorder_.sources_count());
    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form,
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS});
  }
}

// Verify that we correctly log UKM for form parsed with type hints regarding
// developer engagement.
TEST_F(AutofillMetricsTest, UkmDeveloperEngagement_LogUpiVpaTypeHint) {
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Payment", "payment", "", "text", &field);
  field.autocomplete_attribute = "upi-vpa";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect the "upi-vpa hint" metric to be logged and the "form loaded" form
  // interaction event to be logged.
  {
    autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());
    autofill_manager_->Reset();

    ASSERT_EQ(1U, test_ukm_recorder_.entries_count());
    ASSERT_EQ(1U, test_ukm_recorder_.sources_count());
    VerifyDeveloperEngagementUkm(test_ukm_recorder_, form,
                                 {AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT});
    test_ukm_recorder_.Purge();
  }

  // Add another field with an author-specified field type to the form.
  test::CreateTestFormField("", "", "", "text", &field);
  field.autocomplete_attribute = "address-line1";
  forms.back().fields.push_back(field);

  {
    autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());
    autofill_manager_->Reset();

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form,
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITH_TYPE_HINTS,
         AutofillMetrics::FORM_CONTAINS_UPI_VPA_HINT});
  }
}

TEST_F(AutofillMetricsTest, LogStoredCreditCardMetrics) {
  // Helper timestamps for setting up the test data.
  base::Time now = AutofillClock::Now();
  base::Time one_month_ago = now - base::TimeDelta::FromDays(30);
  base::Time::Exploded now_exploded;
  base::Time::Exploded one_month_ago_exploded;
  now.LocalExplode(&now_exploded);
  one_month_ago.LocalExplode(&one_month_ago_exploded);

  std::vector<std::unique_ptr<CreditCard>> local_cards;
  std::vector<std::unique_ptr<CreditCard>> server_cards;
  local_cards.reserve(2);
  server_cards.reserve(10);

  // Create in-use and in-disuse cards of each record type: 1 of each for local,
  // 2 of each for masked, and 3 of each for unmasked.
  const std::vector<CreditCard::RecordType> record_types{
      CreditCard::LOCAL_CARD, CreditCard::MASKED_SERVER_CARD,
      CreditCard::FULL_SERVER_CARD};
  int num_cards_of_type = 0;
  for (auto record_type : record_types) {
    num_cards_of_type += 1;
    for (int i = 0; i < num_cards_of_type; ++i) {
      // Create a card that's still in active use.
      CreditCard card_in_use = test::GetRandomCreditCard(record_type);
      card_in_use.set_use_date(now - base::TimeDelta::FromDays(30));
      card_in_use.set_use_count(10);

      // Create a card that's not in active use.
      CreditCard card_in_disuse = test::GetRandomCreditCard(record_type);
      card_in_disuse.SetExpirationYear(one_month_ago_exploded.year);
      card_in_disuse.SetExpirationMonth(one_month_ago_exploded.month);
      card_in_disuse.set_use_date(now - base::TimeDelta::FromDays(200));
      card_in_disuse.set_use_count(10);

      // Add the cards to the personal data manager in the appropriate way.
      auto& repo =
          (record_type == CreditCard::LOCAL_CARD) ? local_cards : server_cards;
      repo.push_back(base::MakeUnique<CreditCard>(std::move(card_in_use)));
      repo.push_back(base::MakeUnique<CreditCard>(std::move(card_in_disuse)));
    }
  }

  // Log the stored credit card metrics for the cards configured above.
  base::HistogramTester histogram_tester;
  AutofillMetrics::LogStoredCreditCardMetrics(local_cards, server_cards,
                                              base::TimeDelta::FromDays(180));

  // Validate the basic count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Local", 1);
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount", 12, 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Local", 2,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardCount.Server",
                                     10, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Masked", 4, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardCount.Server.Unmasked", 6, 1);

  // Validate the disused count metrics.
  histogram_tester.ExpectTotalCount("Autofill.StoredCreditCardDisusedCount", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Masked", 1);
  histogram_tester.ExpectTotalCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Unmasked", 1);
  histogram_tester.ExpectBucketCount("Autofill.StoredCreditCardDisusedCount", 6,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Local", 1, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server", 5, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Masked", 2, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.StoredCreditCardDisusedCount.Server.Unmasked", 3, 1);

  // Validate the days-since-last-use metrics.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 12);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 2);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 10);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked", 4);
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked", 6);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 30, 6);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard", 200, 6);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 30, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Local", 200, 1);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 30, 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server", 200, 5);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked", 30, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Masked", 200, 2);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked", 30, 3);
  histogram_tester.ExpectBucketCount(
      "Autofill.DaysSinceLastUse.StoredCreditCard.Server.Unmasked", 200, 3);
}

// Test that the profile count is logged correctly.
TEST_F(AutofillMetricsTest, StoredProfileCount) {
  // The metric should be logged when the profiles are first loaded.
  {
    base::HistogramTester histogram_tester;
    personal_data_->LoadProfiles();
    histogram_tester.ExpectUniqueSample("Autofill.StoredProfileCount", 2, 1);
  }

  // The metric should only be logged once.
  {
    base::HistogramTester histogram_tester;
    personal_data_->LoadProfiles();
    histogram_tester.ExpectTotalCount("Autofill.StoredProfileCount", 0);
  }
}

// Test that the local credit card count is logged correctly.
TEST_F(AutofillMetricsTest, StoredLocalCreditCardCount) {
  // The metric should be logged when the credit cards are first loaded.
  {
    base::HistogramTester histogram_tester;
    personal_data_->RecreateCreditCards(
        true /* include_local_credit_card */,
        false /* include_masked_server_credit_card */,
        false /* include_full_server_credit_card */);
    histogram_tester.ExpectUniqueSample("Autofill.StoredLocalCreditCardCount",
                                        1, 1);
  }

  // The metric should only be logged once.
  {
    base::HistogramTester histogram_tester;
    personal_data_->RecreateCreditCards(
        true /* include_local_credit_card */,
        false /* include_masked_server_credit_card */,
        false /* include_full_server_credit_card */);
    histogram_tester.ExpectTotalCount("Autofill.StoredLocalCreditCardCount", 0);
  }
}

// Test that the masked server credit card counts are logged correctly.
TEST_F(AutofillMetricsTest, StoredServerCreditCardCounts_Masked) {
  // The metrics should be logged when the credit cards are first loaded.
  {
    base::HistogramTester histogram_tester;
    personal_data_->RecreateCreditCards(
        false /* include_local_credit_card */,
        true /* include_masked_server_credit_card */,
        false /* include_full_server_credit_card */);
    histogram_tester.ExpectUniqueSample(
        "Autofill.StoredServerCreditCardCount.Masked", 1, 1);
  }

  // The metrics should only be logged once.
  {
    base::HistogramTester histogram_tester;
    personal_data_->RecreateCreditCards(
        false /* include_local_credit_card */,
        true /* include_masked_server_credit_card */,
        true /* include_full_server_credit_card */);
    histogram_tester.ExpectTotalCount(
        "Autofill.StoredServerCreditCardCount.Masked", 0);
  }
}

// Test that the unmasked (full) server credit card counts are logged correctly.
TEST_F(AutofillMetricsTest, StoredServerCreditCardCounts_Unmasked) {
  // The metrics should be logged when the credit cards are first loaded.
  {
    base::HistogramTester histogram_tester;
    personal_data_->RecreateCreditCards(
        false /* include_local_credit_card */,
        false /* include_masked_server_credit_card */,
        true /* include_full_server_credit_card */);
    histogram_tester.ExpectUniqueSample(
        "Autofill.StoredServerCreditCardCount.Unmasked", 1, 1);
  }

  // The metrics should only be logged once.
  {
    base::HistogramTester histogram_tester;
    personal_data_->RecreateCreditCards(
        false /* include_local_credit_card */,
        false /* include_masked_server_credit_card */,
        true /* include_full_server_credit_card */);
    histogram_tester.ExpectTotalCount(
        "Autofill.StoredServerCreditCardCount.Unmasked", 0);
  }
}

// Test that we correctly log when Autofill is enabled.
TEST_F(AutofillMetricsTest, AutofillIsEnabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data_->set_autofill_enabled(true);
  personal_data_->Init(
      autofill_client_.GetDatabase(), autofill_client_.GetPrefs(),
      account_tracker_.get(), signin_manager_.get(), false);
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.Startup", true, 1);
}

// Test that we correctly log when Autofill is disabled.
TEST_F(AutofillMetricsTest, AutofillIsDisabledAtStartup) {
  base::HistogramTester histogram_tester;
  personal_data_->set_autofill_enabled(false);
  personal_data_->Init(
      autofill_client_.GetDatabase(), autofill_client_.GetPrefs(),
      account_tracker_.get(), signin_manager_.get(), false);
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.Startup", false, 1);
}

// Test that we log the number of Autofill suggestions when filling a form.
TEST_F(AutofillMetricsTest, AddressSuggestionsCount) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(NAME_FULL);
  test::CreateTestFormField("Email", "email", "", "email", &field);
  form.fields.push_back(field);
  field_types.push_back(EMAIL_ADDRESS);
  test::CreateTestFormField("Phone", "phone", "", "tel", &field);
  form.fields.push_back(field);
  field_types.push_back(PHONE_HOME_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the phone field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 2,
                                        1);
  }

  {
    // Simulate activating the autofill popup for the email field after typing.
    // No new metric should be logged, since we're still on the same page.
    test::CreateTestFormField("Email", "email", "b", "email", &field);
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the email field after typing.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample("Autofill.AddressSuggestionsCount", 1,
                                        1);
  }

  // Reset the autofill manager state again.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the email field after typing.
    form.fields[0].is_autofilled = true;
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectTotalCount("Autofill.AddressSuggestionsCount", 0);
  }
}

// Test that the credit card checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, CreditCardCheckoutFlowUserActions) {
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Name on card" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate showing a credit card suggestion polled from "Credit card number"
  // field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedCreditCardSuggestions"));
  }

  // Simulate selecting a credit card suggestions.
  {
    base::UserActionTester user_action_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    external_delegate_->DidAcceptSuggestion(
        ASCIIToUTF16("Test"),
        autofill_manager_->MakeFrontendID(guid, std::string()), 0);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a credit card suggestion.
  {
    base::UserActionTester user_action_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledCreditCardSuggestion"));
  }

  // Simulate submitting the credit card form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));
  }

  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NAME_FULL},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NAME_FULL}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER}}});
  // Expect 2 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate_->DidAcceptSuggestion|. Second, from call to
  // |autofill_manager_->FillOrPreviewForm|.
  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
      {{{UkmSuggestionFilledType::kRecordTypeName, CreditCard::LOCAL_CARD},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
       {{UkmSuggestionFilledType::kRecordTypeName, CreditCard::LOCAL_CARD},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  // Expect |NON_FILLABLE_FORM_OR_NEW_DATA| in |AutofillFormSubmittedState|
  // because |field.value| is empty in |DeterminePossibleFieldTypesForUpload|.
  VerifySubmitFormUkm(test_ukm_recorder_, form,
                      AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
}

// Test that the profile checkout flow user actions are correctly logged.
TEST_F(AutofillMetricsTest, ProfileCheckoutFlowUserActions) {
  // Create a profile.
  personal_data_->RecreateProfile();

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a profile field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "State" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[0]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate showing a profile suggestion polled from "City" field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form,
                                          form.fields[1]);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_ShowedProfileSuggestions"));
  }

  // Simulate selecting a profile suggestions.
  {
    base::UserActionTester user_action_tester;
    std::string guid("00000000-0000-0000-0000-000000000001");  // local profile.
    external_delegate_->DidAcceptSuggestion(
        ASCIIToUTF16("Test"),
        autofill_manager_->MakeFrontendID(std::string(), guid), 0);
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_SelectedSuggestion"));
  }

  // Simulate filling a profile suggestion.
  {
    base::UserActionTester user_action_tester;
    std::string guid("00000000-0000-0000-0000-000000000001");  // local profile.
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FilledProfileSuggestion"));
  }

  // Simulate submitting the profile form.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    EXPECT_EQ(1,
              user_action_tester.GetActionCount("Autofill_OnWillSubmitForm"));
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));
  }

  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_STATE},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_STATE}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, ADDRESS_HOME_CITY},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, ADDRESS_HOME_CITY}}});
  // Expect 2 |FORM_EVENT_LOCAL_SUGGESTION_FILLED| events. First, from
  // call to |external_delegate_->DidAcceptSuggestion|. Second, from call to
  // |autofill_manager_->FillOrPreviewForm|.
  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
      {{{UkmSuggestionFilledType::kRecordTypeName,
         AutofillProfile::LOCAL_PROFILE},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
       {{UkmSuggestionFilledType::kRecordTypeName,
         AutofillProfile::LOCAL_PROFILE},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  // Expect |NON_FILLABLE_FORM_OR_NEW_DATA| in |AutofillFormSubmittedState|
  // because |field.value| is empty in |DeterminePossibleFieldTypesForUpload|.
  VerifySubmitFormUkm(test_ukm_recorder_, form,
                      AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
}

// Tests that the Autofill_PolledCreditCardSuggestions user action is only
// logged once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledCreditCardSuggestions_DebounceLogs) {
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[0],
                                              gfx::RectF());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a second query on the same field. There should still only be one
  // logged poll.
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[0],
                                              gfx::RectF());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[1],
                                              gfx::RectF());
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[0],
                                              gfx::RectF());
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledCreditCardSuggestions"));
}

// Tests that the Autofill.QueriedCreditCardFormIsSecure histogram is logged
// properly.
TEST_F(AutofillMetricsTest, QueriedCreditCardFormIsSecure) {
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  autofill_client_.set_form_origin(form.origin);

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  {
    // Simulate having seen this insecure form on page load.
    form.origin = GURL("http://example.com/form.html");
    form.action = GURL("http://example.com/submit.html");
    autofill_manager_->AddSeenForm(form, field_types, field_types);

    // Simulate an Autofill query on a credit card field (HTTP, non-secure
    // form).
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[1],
                                                gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", false, 1);
  }

  {
    // Simulate having seen this secure form on page load.
    autofill_manager_->Reset();
    form.origin = GURL("https://example.com/form.html");
    form.action = GURL("https://example.com/submit.html");
    autofill_client_.set_form_origin(form.origin);
    autofill_manager_->AddSeenForm(form, field_types, field_types);

    // Simulate an Autofill query on a credit card field (HTTPS form).
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[1],
                                                gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.QueriedCreditCardFormIsSecure", true, 1);
  }
}

// Tests that the Autofill_PolledProfileSuggestions user action is only logged
// once if the field is queried repeatedly.
TEST_F(AutofillMetricsTest, PolledProfileSuggestions_DebounceLogs) {
  personal_data_->RecreateProfile();

  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a profile field. A poll should be logged.
  base::UserActionTester user_action_tester;
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[0],
                                              gfx::RectF());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a second query on the same field. There should still only be poll
  // logged.
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[0],
                                              gfx::RectF());
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query to another field. There should be a second poll logged.
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[1],
                                              gfx::RectF());
  EXPECT_EQ(2, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));

  // Simulate a query back to the initial field. There should be a third poll
  // logged.
  autofill_manager_->OnQueryFormFieldAutofill(0, form, form.fields[0],
                                              gfx::RectF());
  EXPECT_EQ(3, user_action_tester.GetActionCount(
                   "Autofill_PolledProfileSuggestions"));
}

// Test that we log interacted form event for credit cards related.
TEST_F(AutofillMetricsTest, CreditCardInteractedFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the credit card field twice.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->OnQueryFormFieldAutofill(1, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log suggestion shown form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardShownFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }

  // Recreate server cards with bank names.
  personal_data_->RecreateServerCreditCardsWithBankName();

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.BankNameDisplayed",
        AutofillMetrics::
            FORM_EVENT_SUGGESTIONS_SHOWN_WITH_BANK_NAME_AVAILABLE_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.BankNameDisplayed",
        AutofillMetrics::
            FORM_EVENT_SUGGESTIONS_SHOWN_WITH_BANK_NAME_AVAILABLE_ONCE,
        1);
  }
}

// Test that we log selected form event for credit cards.
TEST_F(AutofillMetricsTest, CreditCardSelectedFormEvents) {
  EnableWalletSync();
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting a masked card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields[2],
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting multiple times a masked card server.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields[2],
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields[2],
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SELECTED_ONCE,
        1);
  }
}

// Test that we log filled form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardFilledFormEvents) {
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a local card suggestion.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.back(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
        1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a full card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000003");  // full server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_FILLED_ONCE, 1);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }

  // Recreate server cards with bank names.
  personal_data_->RecreateServerCreditCardsWithBankName();

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a full card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000003");  // full server card
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, field,
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.BankNameDisplayed",
        AutofillMetrics::
            FORM_EVENT_SERVER_SUGGESTION_FILLED_WITH_BANK_NAME_AVAILABLE_ONCE,
        1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling multiple times.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000003");  // full server card
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, field,
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, field,
        autofill_manager_->MakeFrontendID(guid, std::string()));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.BankNameDisplayed",
        AutofillMetrics::
            FORM_EVENT_SERVER_SUGGESTION_FILLED_WITH_BANK_NAME_AVAILABLE_ONCE,
        1);
  }
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardGetRealPanDuration) {
  EnableWalletSync();
  // Creating masked card
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.back(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.Success", 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  // Creating masked card
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulating filling a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.back(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::PERMANENT_FAILURE,
                                       std::string());
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration", 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.UnmaskPrompt.GetRealPanDuration.Failure", 1);
  }
}

TEST_F(AutofillMetricsTest,
       CreditCardSubmittedWithoutSelectingSuggestionsNoCard) {
  EnableWalletSync();
  // Create a local card for testing, card number is 4111111111111111.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      AutofillMetrics::FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD,
      1);
}

TEST_F(AutofillMetricsTest,
       CreditCardSubmittedWithoutSelectingSuggestionsUnknownCard) {
  EnableWalletSync();
  // Create a local card for testing, card number is 4111111111111111.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "5105105105105100", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD,
      1);
}

TEST_F(AutofillMetricsTest,
       CreditCardSubmittedWithoutSelectingSuggestionsKnownCard) {
  EnableWalletSync();
  // Create a local card for testing, card number is 4111111111111111.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "4111111111111111", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown, but not selected.
  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD,
      1);
}

TEST_F(AutofillMetricsTest,
       ShouldNotLogSubmitWithoutSelectingSuggestionsIfSuggestionFilled) {
  EnableWalletSync();
  // Create a local card for testing, card number is 4111111111111111.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "4111111111111111", "text",
                            &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulating submission with suggestion shown and selected.
  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
  std::string guid("10000000-0000-0000-0000-000000000001");
  autofill_manager_->FillOrPreviewForm(
      AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.back(),
      autofill_manager_->MakeFrontendID(guid, std::string()));

  autofill_manager_->SubmitForm(form, TimeTicks::Now());
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_KNOWN_CARD,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      AutofillMetrics::
          FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_UNKNOWN_CARD,
      0);
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.CreditCard",
      AutofillMetrics::FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD,
      0);
}

TEST_F(AutofillMetricsTest, ShouldNotLogFormEventNoCardForAddressForm) {
  EnableWalletSync();
  // Create a profile.
  personal_data_->RecreateProfile();
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulating submission with no filled data.
  base::HistogramTester histogram_tester;
  autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
  autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
  autofill_manager_->SubmitForm(form, TimeTicks::Now());
  histogram_tester.ExpectBucketCount(
      "Autofill.FormEvents.Address",
      AutofillMetrics::FORM_EVENT_SUBMIT_WITHOUT_SELECTING_SUGGESTIONS_NO_CARD,
      0);
}

// Test that we log submitted form events for credit cards.
TEST_F(AutofillMetricsTest, CreditCardSubmittedFormEvents) {
  EnableWalletSync();
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown. Form is submmitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->ResetRunLoop();
    autofill_manager_->OnWillSubmitForm(form, TimeTicks::Now());
    autofill_manager_->OnFormSubmitted(form);
    autofill_manager_->Reset();
    // Trigger UploadFormDataAsyncCallback.
    autofill_manager_->RunRunLoop();
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName, CreditCard::LOCAL_CARD},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    std::string guid(
        "10000000-0000-0000-0000-000000000003");  // full server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 1);

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           CreditCard::FULL_SERVER_CARD},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    std::string guid(
        "10000000-0000-0000-0000-000000000002");  // masked server card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.back(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
        1);

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
        {{{UkmSuggestionFilledType::kRecordTypeName,
           CreditCard::MASKED_SERVER_CARD},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSelectedMaskedServerCardType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
        {{{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
           AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});

    autofill_manager_->SubmitForm(form, TimeTicks::Now());

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmFormSubmittedType::kEntryName,
        {{{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
           AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
         {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
           AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
          {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});

    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName, CREDIT_CARD_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, CREDIT_CARD_NUMBER}}});
    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }
}

// Test that we log "will submit" (but not submitted) form events for credit
// cards. Mirrors CreditCardSubmittedFormEvents test but does not expect any
// "submitted" metrics.
TEST_F(AutofillMetricsTest, CreditCardWillSubmitFormEvents) {
  EnableWalletSync();
  // Creating all kinds of cards.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    std::string guid("10000000-0000-0000-0000-000000000001");  // local card
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled server data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    // Full server card.
    std::string guid("10000000-0000-0000-0000-000000000003");
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with a masked card server suggestion.
    base::HistogramTester histogram_tester;
    // Masked server card.
    std::string guid("10000000-0000-0000-0000-000000000002");
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.back(),
        autofill_manager_->MakeFrontendID(guid, std::string()));
    autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                       "6011000990139424");
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_FILLED_ONCE,
        1);
  }

  // Recreating cards as the previous test should have upgraded the masked
  // card to a full card.
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics
            ::FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
  }
}

// Test that we log interacted form events for address.
TEST_F(AutofillMetricsTest, AddressInteractedFormEvents) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulate activating the autofill popup for the street field twice.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->OnQueryFormFieldAutofill(1, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log suggestion shown form events for address.
TEST_F(AutofillMetricsTest, AddressShownFormEvents) {
  EnableWalletSync();
  // Create a profile.
  personal_data_->RecreateProfile();
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating new popup being shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating two popups in the same page load.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 1);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating same popup being refreshed.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(false /* is_new_popup */, form,
                                          field);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTIONS_SHOWN_ONCE, 0);
    // Check that the bank name histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(0, histogram_tester.GetTotalCountsForPrefix(
                     "Autofill.FormEvents.CreditCard")
                     ["Autofill.FormEvents.CreditCard.BankNameDisplayed"]);
  }
}

// Test that we log filled form events for address.
TEST_F(AutofillMetricsTest, AddressFilledFormEvents) {
  EnableWalletSync();
  // Create a profile.
  personal_data_->RecreateProfile();
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    std::string guid("00000000-0000-0000-0000-000000000001");  // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating selecting/filling a local profile suggestion.
    base::HistogramTester histogram_tester;
    std::string guid("00000000-0000-0000-0000-000000000001");  // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_FILLED_ONCE, 1);
  }
}

// Test that we log submitted form events for address.
TEST_F(AutofillMetricsTest, AddressSubmittedFormEvents) {
  EnableWalletSync();
  // Create a profile.
  personal_data_->RecreateProfile();
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data. Form is submmitted and
    // autofill manager is reset before UploadFormDataAsyncCallback is
    // triggered.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->ResetRunLoop();
    autofill_manager_->OnWillSubmitForm(form, TimeTicks::Now());
    autofill_manager_->OnFormSubmitted(form);
    autofill_manager_->Reset();
    // Trigger UploadFormDataAsyncCallback.
    autofill_manager_->RunRunLoop();
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);

    VerifySubmitFormUkm(test_ukm_recorder_, form,
                        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA);
  }

  // Reset the autofill manager state and purge UKM logs.
  autofill_manager_->Reset();
  test_ukm_recorder_.Purge();

  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    std::string guid("00000000-0000-0000-0000-000000000001");  // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    autofill_manager_->SubmitForm(form, TimeTicks::Now());

    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion show but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->SubmitForm(form, TimeTicks::Now());

    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }
}

// Test that we log "will submit" (but not submitted) form events for address.
// Mirrors AddressSubmittedFormEvents test but does not expect any "submitted"
// metrics.
TEST_F(AutofillMetricsTest, AddressWillSubmitFormEvents) {
  EnableWalletSync();
  // Create a profile.
  personal_data_->RecreateProfile();
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with no filled data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with filled local data.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    std::string guid("00000000-0000-0000-0000-000000000001");  // local profile
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating multiple submissions.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  {
    // Simulating submission with suggestion shown but without previous
    // interaction.
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true /* is_new_popup */, form, field);
    autofill_manager_->WillSubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_WILL_SUBMIT_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_WILL_SUBMIT_ONCE,
        0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SUGGESTION_SHOWN_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_LOCAL_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::FORM_EVENT_SERVER_SUGGESTION_SUBMITTED_ONCE, 0);
    histogram_tester.ExpectBucketCount(
        "Autofill.FormEvents.Address",
        AutofillMetrics::
            FORM_EVENT_MASKED_SERVER_CARD_SUGGESTION_SUBMITTED_ONCE,
        0);
  }
}

// Test that we log interacted form event for credit cards only once.
TEST_F(AutofillMetricsTest, CreditCardFormEventsAreSegmented) {
  EnableWalletSync();

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Year", "card_year", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithNoData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      true /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      false /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithOnlyServerData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      true /* include_full_server_credit_card */);

  {
    // Simulate activating the autofill popup for the credit card field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.CreditCard.WithBothServerAndLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log interacted form event for address only once.
TEST_F(AutofillMetricsTest, AddressFormEventsAreSegmented) {
  EnableWalletSync();

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STATE);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_CITY);
  test::CreateTestFormField("Street", "street", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(ADDRESS_HOME_STREET_ADDRESS);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->ClearProfiles();

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithNoData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }

  // Reset the autofill manager state.
  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);
  personal_data_->RecreateProfile();

  {
    // Simulate activating the autofill popup for the street field.
    base::HistogramTester histogram_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormEvents.Address.WithOnlyLocalData",
        AutofillMetrics::FORM_EVENT_INTERACTED_ONCE, 1);
  }
}

// Test that we log that Autofill is enabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillIsEnabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager_->set_autofill_enabled(true);
  autofill_manager_->OnFormsSeen(std::vector<FormData>(), TimeTicks());
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.PageLoad", true, 1);
}

// Test that we log that Autofill is disabled when filling a form.
TEST_F(AutofillMetricsTest, AutofillIsDisabledAtPageLoad) {
  base::HistogramTester histogram_tester;
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->OnFormsSeen(std::vector<FormData>(), TimeTicks());
  histogram_tester.ExpectUniqueSample("Autofill.IsEnabled.PageLoad", false, 1);
}

// Test that we log the days since last use of a credit card when it is used.
TEST_F(AutofillMetricsTest, DaysSinceLastUse_CreditCard) {
  base::HistogramTester histogram_tester;
  CreditCard credit_card;
  credit_card.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(21));
  credit_card.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.CreditCard", 21,
                                     1);
}

// Test that we log the days since last use of a profile when it is used.
TEST_F(AutofillMetricsTest, DaysSinceLastUse_Profile) {
  base::HistogramTester histogram_tester;
  AutofillProfile profile;
  profile.set_use_date(base::Time::Now() - base::TimeDelta::FromDays(13));
  profile.RecordAndLogUse();
  histogram_tester.ExpectBucketCount("Autofill.DaysSinceLastUse.Profile", 13,
                                     1);
}

// Verify that we correctly log the submitted form's state.
TEST_F(AutofillMetricsTest, AutofillFormSubmittedState) {
  // Start with a form with insufficiently many fields.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Unknown", "unknown", "", "text", &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);

  // Expect no notifications when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::Now());
    histogram_tester.ExpectTotalCount("Autofill.FormSubmittedState", 0);

    EXPECT_EQ(1U, test_ukm_recorder_.entries_count());
    EXPECT_EQ(1U, test_ukm_recorder_.sources_count());

    VerifyDeveloperEngagementUkm(
        test_ukm_recorder_, form,
        {AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS});
  }

  ExpectedUkmMetrics expected_form_submission_ukm_metrics;
  ExpectedUkmMetrics expected_field_fill_status_ukm_metrics;

  // No data entered in the form.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }

  // Non fillable form.
  form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  forms.front() = form;

  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }

  // Fill in the third field.
  form.fields[2].value = ASCIIToUTF16("12345678901");
  forms.front() = form;

  // Autofilled none with no suggestions shown.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS,
        1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledNone_SuggestionsNotShown"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::
              FILLABLE_FORM_AUTOFILLED_NONE_DID_NOT_SHOW_SUGGESTIONS},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});

    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }

  // Autofilled none with suggestions shown.
  autofill_manager_->DidShowSuggestions(true, form, form.fields[2]);
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledNone_SuggestionsShown"));

    VerifyFormInteractionUkm(
        test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
        {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
          {UkmTextFieldDidChangeType::kHeuristicTypeName,
           PHONE_HOME_WHOLE_NUMBER},
          {UkmTextFieldDidChangeType::kHtmlFieldTypeName,
           HTML_TYPE_UNSPECIFIED},
          {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA}}});

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_NONE_DID_SHOW_SUGGESTIONS},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }

  // Mark one of the fields as autofilled.
  form.fields[1].is_autofilled = true;
  forms.front() = form;

  // Autofilled some of the fields.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledSome"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_SOME},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }

  // Mark all of the fillable fields as autofilled.
  form.fields[0].is_autofilled = true;
  form.fields[2].is_autofilled = true;
  forms.front() = form;

  // Autofilled all the fields.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_FilledAll"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::FILLABLE_FORM_AUTOFILLED_ALL},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }

  // Clear out the third field's value.
  form.fields[2].value = base::string16();
  forms.front() = form;

  // Non fillable form.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histogram_tester.ExpectUniqueSample(
        "Autofill.FormSubmittedState",
        AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA, 1);
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_FormSubmitted_NonFillable"));

    expected_form_submission_ukm_metrics.push_back(
        {{UkmFormSubmittedType::kAutofillFormSubmittedStateName,
          AutofillMetrics::NON_FILLABLE_FORM_OR_NEW_DATA},
         {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}});
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFormSubmittedType::kEntryName,
                             expected_form_submission_ukm_metrics);

    AppendFieldFillStatusUkm(form, &expected_field_fill_status_ukm_metrics);
    VerifyFormInteractionUkm(test_ukm_recorder_, form,
                             UkmFieldFillStatusType::kEntryName,
                             expected_field_fill_status_ukm_metrics);
  }
}

TEST_F(AutofillMetricsTest, LogUserHappinessMetric_PasswordForm) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL,
                                            PASSWORD_FIELD);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Password",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.UnknownFormType",
                                      0);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL,
                                            USERNAME_FIELD);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Password",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.UnknownFormType",
                                      0);
  }
}

TEST_F(AutofillMetricsTest, LogUserHappinessMetric_UnknownForm) {
  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL,
                                            NO_GROUP);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.UnknownFormType",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Password", 0);
  }

  {
    base::HistogramTester histogram_tester;
    AutofillMetrics::LogUserHappinessMetric(AutofillMetrics::USER_DID_AUTOFILL,
                                            TRANSACTION);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.UnknownFormType",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Password", 0);
  }
}

// Verify that nothing is logging in happiness metrics if no fields in form.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_EmptyForm) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.CreditCard", 0);
    histogram_tester.ExpectTotalCount("Autofill.UserHappiness.Address", 0);
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_CreditCardForm) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  // Construct a valid credit card form with minimal fields.
  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Card Number", "card_number", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Expiration", "cc_exp", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);
  test::CreateTestFormField("Verification", "verification", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_VERIFICATION_CODE);

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate typing.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate suggestions shown twice with separate popups.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, field);
    autofill_manager_->DidShowSuggestions(true, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  autofill_manager_->Reset();
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, field);
    autofill_manager_->DidShowSuggestions(false, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  // Simulate suggestions shown for a different field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, form.fields[1]);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  // Simulate invoking autofill.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.CreditCard",
                                       AutofillMetrics::USER_DID_AUTOFILL_ONCE,
                                       1);
  }

  // Simulate editing an autofilled field.
  {
    base::HistogramTester histogram_tester;
    std::string guid("00000000-0000-0000-0000-000000000001");
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.CreditCard",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.CreditCard",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
  }

  // Simulate invoking autofill again.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.CreditCard",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  // Simulate editing another autofilled field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                            TimeTicks());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness.CreditCard",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
  }
}

// Verify that we correctly log user happiness metrics dealing with form
// interaction.
TEST_F(AutofillMetricsTest, UserHappinessFormInteraction_AddressForm) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Expect a notification when the form is first seen.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::FORMS_LOADED, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::FORMS_LOADED, 1);
  }

  // Simulate typing.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_TYPE, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::USER_DID_TYPE, 1);
  }

  // Simulate suggestions shown twice with separate popups.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, field);
    autofill_manager_->DidShowSuggestions(true, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 2);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  autofill_manager_->Reset();
  autofill_manager_->OnFormsSeen(forms, TimeTicks());
  // Simulate suggestions shown twice for a single edit (i.e. multiple
  // keystrokes in a single field).
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, field);
    autofill_manager_->DidShowSuggestions(false, form, field);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::SUGGESTIONS_SHOWN_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::SUGGESTIONS_SHOWN_ONCE,
                                       1);
  }

  // Simulate suggestions shown for a different field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->DidShowSuggestions(true, form, form.fields[1]);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::SUGGESTIONS_SHOWN, 1);
  }

  // Simulate invoking autofill.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness", AutofillMetrics::USER_DID_AUTOFILL_ONCE, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectBucketCount("Autofill.UserHappiness.Address",
                                       AutofillMetrics::USER_DID_AUTOFILL_ONCE,
                                       1);
  }

  // Simulate editing an autofilled field.
  {
    base::HistogramTester histogram_tester;
    std::string guid("00000000-0000-0000-0000-000000000001");
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, 0, form, form.fields.front(),
        autofill_manager_->MakeFrontendID(std::string(), guid));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    // Simulate a second keystroke; make sure we don't log the metric twice.
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(), TimeTicks());
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectBucketCount(
        "Autofill.UserHappiness.Address",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD_ONCE, 1);
  }

  // Simulate invoking autofill again.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnDidFillAutofillFormData(form, TimeTicks());
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
    histogram_tester.ExpectUniqueSample("Autofill.UserHappiness.Address",
                                        AutofillMetrics::USER_DID_AUTOFILL, 1);
  }

  // Simulate editing another autofilled field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnTextFieldDidChange(form, form.fields[1], gfx::RectF(),
                                            TimeTicks());
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
    histogram_tester.ExpectUniqueSample(
        "Autofill.UserHappiness.Address",
        AutofillMetrics::USER_DID_EDIT_AUTOFILLED_FIELD, 1);
  }

  autofill_manager_->Reset();

  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmInteractedWithFormType::kEntryName,
      {{{UkmInteractedWithFormType::kIsForCreditCardName, false},
        {UkmInteractedWithFormType::kLocalRecordTypeCountName, 0},
        {UkmInteractedWithFormType::kServerRecordTypeCountName, 0}}});
  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmSuggestionsShownType::kEntryName,
      {{{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName,
         PHONE_HOME_WHOLE_NUMBER},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA}},
       {{UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, EMAIL_ADDRESS},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA}}});
  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmSuggestionFilledType::kEntryName,
      {{{UkmSuggestionFilledType::kRecordTypeName,
         AutofillProfile::LOCAL_PROFILE},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
       {{UkmSuggestionFilledType::kRecordTypeName,
         AutofillProfile::LOCAL_PROFILE},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
  VerifyFormInteractionUkm(
      test_ukm_recorder_, form, UkmTextFieldDidChangeType::kEntryName,
      {{{UkmTextFieldDidChangeType::kFieldTypeGroupName, NAME},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, NAME_FULL},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HTML_MODE_NONE},
        {UkmTextFieldDidChangeType::kIsAutofilledName, false},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
       {{UkmTextFieldDidChangeType::kFieldTypeGroupName, NAME},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, NAME_FULL},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HTML_MODE_NONE},
        {UkmTextFieldDidChangeType::kIsAutofilledName, true},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}},
       {{UkmTextFieldDidChangeType::kFieldTypeGroupName, EMAIL},
        {UkmTextFieldDidChangeType::kHeuristicTypeName, EMAIL_ADDRESS},
        {UkmTextFieldDidChangeType::kServerTypeName, NO_SERVER_DATA},
        {UkmTextFieldDidChangeType::kHtmlFieldTypeName, HTML_TYPE_UNSPECIFIED},
        {UkmTextFieldDidChangeType::kHtmlFieldModeName, HTML_MODE_NONE},
        {UkmTextFieldDidChangeType::kIsAutofilledName, true},
        {UkmTextFieldDidChangeType::kIsEmptyName, true},
        {UkmSuggestionFilledType::kMillisecondsSinceFormParsedName, 0}}});
}

// Verify that we correctly log metrics tracking the duration of form fill.
TEST_F(AutofillMetricsTest, FormFillDuration) {
  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);

  const std::vector<FormData> forms(1, form);

  // Fill additional form.
  FormData second_form = form;
  test::CreateTestFormField("Second Phone", "second_phone", "", "text", &field);
  second_form.fields.push_back(field);

  std::vector<FormData> second_forms(1, second_form);

  // Fill the field values for form submission.
  form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  form.fields[2].value = ASCIIToUTF16("12345678901");

  // Fill the field values for form submission.
  second_form.fields[0].value = ASCIIToUTF16("Elvis Aaron Presley");
  second_form.fields[1].value = ASCIIToUTF16("theking@gmail.com");
  second_form.fields[2].value = ASCIIToUTF16("12345678901");
  second_form.fields[3].value = ASCIIToUTF16("51512345678");

  // Expect only form load metrics to be logged if the form is submitted without
  // user interaction.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }

  // Expect metric to be logged if the user manually edited a form field.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 14, 1);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager_->ResetRunLoop();
    autofill_manager_->Reset();
    autofill_manager_->RunRunLoop();
  }

  // Expect metric to be logged if the user autofilled the form.
  form.fields[0].is_autofilled = true;
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnDidFillAutofillFormData(
        form, TimeTicks::FromInternalValue(5));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager_->ResetRunLoop();
    autofill_manager_->Reset();
    autofill_manager_->RunRunLoop();
  }

  // Expect metric to be logged if the user both manually filled some fields
  // and autofilled others.  Messages can arrive out of order, so make sure they
  // take precedence appropriately.
  {
    base::HistogramTester histogram_tester;

    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnDidFillAutofillFormData(
        form, TimeTicks::FromInternalValue(5));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager_->ResetRunLoop();
    autofill_manager_->Reset();
    autofill_manager_->RunRunLoop();
  }

  // Make sure that loading another form doesn't affect metrics from the first
  // form.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnFormsSeen(second_forms,
                                   TimeTicks::FromInternalValue(3));
    autofill_manager_->OnDidFillAutofillFormData(
        form, TimeTicks::FromInternalValue(5));
    autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                            gfx::RectF(),
                                            TimeTicks::FromInternalValue(3));
    autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithAutofill", 16, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 14, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    // We expected an upload to be triggered when the manager is reset.
    autofill_manager_->ResetRunLoop();
    autofill_manager_->Reset();
    autofill_manager_->RunRunLoop();
  }

  // Make sure that submitting a form that was loaded later will report the
  // later loading time.
  {
    base::HistogramTester histogram_tester;
    autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
    autofill_manager_->OnFormsSeen(second_forms,
                                   TimeTicks::FromInternalValue(5));
    autofill_manager_->SubmitForm(second_form,
                                  TimeTicks::FromInternalValue(17));

    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromLoad.WithAutofill", 0);
    histogram_tester.ExpectUniqueSample(
        "Autofill.FillDuration.FromLoad.WithoutAutofill", 12, 1);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithAutofill", 0);
    histogram_tester.ExpectTotalCount(
        "Autofill.FillDuration.FromInteraction.WithoutAutofill", 0);

    autofill_manager_->Reset();
  }
}

// Verify that we correctly log metrics for profile action on form submission.
TEST_F(AutofillMetricsTest, ProfileActionOnFormSubmitted) {
  base::HistogramTester histogram_tester;

  // Load a fillable form.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  // Create the form's fields.
  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Phone", "phone", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("City", "city", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("State", "state", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Zip", "zip", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Organization", "organization", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);

  // Fill second form.
  FormData second_form = form;
  std::vector<FormData> second_forms(1, second_form);

  // Fill a third form.
  FormData third_form = form;
  std::vector<FormData> third_forms(1, third_form);

  // Fill a fourth form.
  FormData fourth_form = form;
  std::vector<FormData> fourth_forms(1, fourth_form);

  // Fill the field values for the first form submission.
  form.fields[0].value = ASCIIToUTF16("Albert Canuck");
  form.fields[1].value = ASCIIToUTF16("can@gmail.com");
  form.fields[2].value = ASCIIToUTF16("12345678901");
  form.fields[3].value = ASCIIToUTF16("1234 McGill street.");
  form.fields[4].value = ASCIIToUTF16("Montreal");
  form.fields[5].value = ASCIIToUTF16("Canada");
  form.fields[6].value = ASCIIToUTF16("Quebec");
  form.fields[7].value = ASCIIToUTF16("A1A 1A1");

  // Fill the field values for the second form submission (same as first form).
  second_form.fields = form.fields;

  // Fill the field values for the third form submission.
  third_form.fields[0].value = ASCIIToUTF16("Jean-Paul Canuck");
  third_form.fields[1].value = ASCIIToUTF16("can2@gmail.com");
  third_form.fields[2].value = ASCIIToUTF16("");
  third_form.fields[3].value = ASCIIToUTF16("1234 McGill street.");
  third_form.fields[4].value = ASCIIToUTF16("Montreal");
  third_form.fields[5].value = ASCIIToUTF16("Canada");
  third_form.fields[6].value = ASCIIToUTF16("Quebec");
  third_form.fields[7].value = ASCIIToUTF16("A1A 1A1");

  // Fill the field values for the fourth form submission (same as third form
  // plus phone info).
  fourth_form.fields = third_form.fields;
  fourth_form.fields[2].value = ASCIIToUTF16("12345678901");

  // Expect to log NEW_PROFILE_CREATED for the metric since a new profile is
  // submitted.
  autofill_manager_->OnFormsSeen(forms, TimeTicks::FromInternalValue(1));
  autofill_manager_->SubmitForm(form, TimeTicks::FromInternalValue(17));
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::NEW_PROFILE_CREATED, 1);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_USED, 0);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_UPDATED,
                                     0);

  // Expect to log EXISTING_PROFILE_USED for the metric since the same profile
  // is submitted.
  autofill_manager_->OnFormsSeen(second_forms, TimeTicks::FromInternalValue(1));
  autofill_manager_->SubmitForm(second_form, TimeTicks::FromInternalValue(17));
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::NEW_PROFILE_CREATED, 1);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_USED, 1);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_UPDATED,
                                     0);

  // Expect to log NEW_PROFILE_CREATED for the metric since a new profile is
  // submitted.
  autofill_manager_->OnFormsSeen(third_forms, TimeTicks::FromInternalValue(1));
  autofill_manager_->SubmitForm(third_form, TimeTicks::FromInternalValue(17));
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::NEW_PROFILE_CREATED, 2);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_USED, 1);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_UPDATED,
                                     0);

  // Expect to log EXISTING_PROFILE_UPDATED for the metric since the profile was
  // updated.
  autofill_manager_->OnFormsSeen(fourth_forms, TimeTicks::FromInternalValue(1));
  autofill_manager_->SubmitForm(fourth_form, TimeTicks::FromInternalValue(17));
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::NEW_PROFILE_CREATED, 2);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_USED, 1);
  histogram_tester.ExpectBucketCount("Autofill.ProfileActionOnFormSubmitted",
                                     AutofillMetrics::EXISTING_PROFILE_UPDATED,
                                     1);
}

// Test class that shares setup code for testing ParseQueryResponse.
class AutofillMetricsParseQueryResponseTest : public testing::Test {
 public:
  void SetUp() override {
    FormData form;
    form.origin = GURL("http://foo.com");
    FormFieldData field;
    field.form_control_type = "text";

    field.label = ASCIIToUTF16("fullname");
    field.name = ASCIIToUTF16("fullname");
    form.fields.push_back(field);

    field.label = ASCIIToUTF16("address");
    field.name = ASCIIToUTF16("address");
    form.fields.push_back(field);

    // Checkable fields should be ignored in parsing.
    FormFieldData checkable_field;
    checkable_field.label = ASCIIToUTF16("radio_button");
    checkable_field.form_control_type = "radio";
    checkable_field.check_status = FormFieldData::CHECKABLE_BUT_UNCHECKED;
    form.fields.push_back(checkable_field);

    owned_forms_.push_back(base::MakeUnique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());

    field.label = ASCIIToUTF16("email");
    field.name = ASCIIToUTF16("email");
    form.fields.push_back(field);

    field.label = ASCIIToUTF16("password");
    field.name = ASCIIToUTF16("password");
    field.form_control_type = "password";
    form.fields.push_back(field);

    owned_forms_.push_back(base::MakeUnique<FormStructure>(form));
    forms_.push_back(owned_forms_.back().get());
  }

 protected:
  std::vector<std::unique_ptr<FormStructure>> owned_forms_;
  std::vector<FormStructure*> forms_;
};

TEST_F(AutofillMetricsParseQueryResponseTest, ServerHasData) {
  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(7);
  response.add_field()->set_overall_type_prediction(30);
  response.add_field()->set_overall_type_prediction(9);
  response.add_field()->set_overall_type_prediction(0);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  base::HistogramTester histogram_tester;
  FormStructure::ParseQueryResponse(response_string, forms_);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// If the server returns NO_SERVER_DATA for one of the forms, expect proper
// logging.
TEST_F(AutofillMetricsParseQueryResponseTest, OneFormNoServerData) {
  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(0);
  response.add_field()->set_overall_type_prediction(0);
  response.add_field()->set_overall_type_prediction(9);
  response.add_field()->set_overall_type_prediction(0);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  base::HistogramTester histogram_tester;
  FormStructure::ParseQueryResponse(response_string, forms_);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(false, 1), Bucket(true, 1)));
}

// If the server returns NO_SERVER_DATA for both of the forms, expect proper
// logging.
TEST_F(AutofillMetricsParseQueryResponseTest, AllFormsNoServerData) {
  AutofillQueryResponseContents response;
  for (int i = 0; i < 4; ++i) {
    response.add_field()->set_overall_type_prediction(0);
  }

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  base::HistogramTester histogram_tester;
  FormStructure::ParseQueryResponse(response_string, forms_);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(false, 2)));
}

// If the server returns NO_SERVER_DATA for only some of the fields, expect the
// UMA metric to say there is data.
TEST_F(AutofillMetricsParseQueryResponseTest, PartialNoServerData) {
  AutofillQueryResponseContents response;
  response.add_field()->set_overall_type_prediction(0);
  response.add_field()->set_overall_type_prediction(10);
  response.add_field()->set_overall_type_prediction(0);
  response.add_field()->set_overall_type_prediction(11);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  base::HistogramTester histogram_tester;
  FormStructure::ParseQueryResponse(response_string, forms_);
  EXPECT_THAT(
      histogram_tester.GetAllSamples("Autofill.ServerResponseHasDataForForm"),
      ElementsAre(Bucket(true, 2)));
}

// Test that the Form-Not-Secure warning user action is recorded.
TEST_F(AutofillMetricsTest, ShowHttpNotSecureExplanationUserAction) {
  EXPECT_CALL(autofill_client_,
              ExecuteCommand(POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE));
  external_delegate_->DidAcceptSuggestion(
      ASCIIToUTF16("Test"), POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE, 0);
}

// Tests that credit card form submissions are logged specially when the form is
// on a non-secure page.
TEST_F(AutofillMetricsTest, NonsecureCreditCardForm) {
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");
  autofill_client_.set_form_origin(form.origin);

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.OnNonsecurePage",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard.WithOnlyLocalData",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
  }
}

// Tests that credit card form submissions are *not* logged specially when the
// form is *not* on a non-secure page.
TEST_F(AutofillMetricsTest,
       NonsecureCreditCardFormMetricsNotRecordedOnSecurePage) {
  personal_data_->RecreateCreditCards(
      true /* include_local_credit_card */,
      false /* include_masked_server_credit_card */,
      false /* include_full_server_credit_card */);

  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("https://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  std::vector<ServerFieldType> field_types;
  test::CreateTestFormField("Name on card", "cc-name", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NAME_FULL);
  test::CreateTestFormField("Credit card", "card", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_NUMBER);
  test::CreateTestFormField("Month", "card_month", "", "text", &field);
  form.fields.push_back(field);
  field_types.push_back(CREDIT_CARD_EXP_MONTH);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  autofill_manager_->AddSeenForm(form, field_types, field_types);

  // Simulate an Autofill query on a credit card field.
  {
    base::UserActionTester user_action_tester;
    autofill_manager_->OnQueryFormFieldAutofill(0, form, field, gfx::RectF());
    EXPECT_EQ(1, user_action_tester.GetActionCount(
                     "Autofill_PolledCreditCardSuggestions"));
  }

  // Simulate submitting the credit card form.
  {
    base::HistogramTester histograms;
    autofill_manager_->SubmitForm(form, TimeTicks::Now());
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_WILL_SUBMIT_ONCE, 1);
    histograms.ExpectBucketCount(
        "Autofill.FormEvents.CreditCard",
        AutofillMetrics::FORM_EVENT_NO_SUGGESTION_SUBMITTED_ONCE, 1);
    // Check that the nonsecure histogram was not recorded. ExpectBucketCount()
    // can't be used here because it expects the histogram to exist.
    EXPECT_EQ(
        0, histograms.GetTotalCountsForPrefix("Autofill.FormEvents.CreditCard")
               ["Autofill.FormEvents.CreditCard.OnNonsecurePage"]);
  }
}

// Tests that logging CardUploadDecision UKM works as expected.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric) {
  GURL url("https://www.google.com");
  int upload_decision = 1;

  AutofillMetrics::LogCardUploadDecisionsUkm(&test_ukm_recorder_, url,
                                             upload_decision);

  ASSERT_EQ(1U, test_ukm_recorder_.sources_count());
  const ukm::UkmSource* source =
      test_ukm_recorder_.GetSourceForUrl(url.spec().c_str());
  EXPECT_EQ(url.spec(), source->url().spec());

  ASSERT_EQ(1U, test_ukm_recorder_.entries_count());
  const ukm::mojom::UkmEntry* entry = test_ukm_recorder_.GetEntry(0);

  // Make sure that a card upload decision entry was logged.
  EXPECT_EQ(source->id(), entry->source_id);
  EXPECT_EQ(base::HashMetricName(UkmCardUploadDecisionType::kEntryName),
            entry->event_hash);
  EXPECT_EQ(1U, entry->metrics.size());

  // Make sure that the correct upload decision was logged.
  const ukm::mojom::UkmMetric* metric = ukm::TestUkmRecorder::FindMetric(
      entry, UkmCardUploadDecisionType::kUploadDecisionName);
  ASSERT_NE(nullptr, metric);
  EXPECT_EQ(upload_decision, metric->value);
}

// Tests that logging DeveloperEngagement UKM works as expected.
TEST_F(AutofillMetricsTest, RecordDeveloperEngagementMetric) {
  GURL url("https://www.google.com");
  int form_structure_metric = 1;

  AutofillMetrics::LogDeveloperEngagementUkm(&test_ukm_recorder_, url,
                                             form_structure_metric);

  ASSERT_EQ(1U, test_ukm_recorder_.sources_count());
  const ukm::UkmSource* source =
      test_ukm_recorder_.GetSourceForUrl(url.spec().c_str());
  EXPECT_EQ(url.spec(), source->url().spec());

  ASSERT_EQ(1U, test_ukm_recorder_.entries_count());
  const ukm::mojom::UkmEntry* entry = test_ukm_recorder_.GetEntry(0);

  // Make sure that a developer engagement entry was logged.
  EXPECT_EQ(source->id(), entry->source_id);
  EXPECT_EQ(base::HashMetricName(UkmDeveloperEngagementType::kEntryName),
            entry->event_hash);
  EXPECT_EQ(1U, entry->metrics.size());

  // Make sure that the correct developer engagement metric was logged.
  const ukm::mojom::UkmMetric* metric = ukm::TestUkmRecorder::FindMetric(
      entry, UkmDeveloperEngagementType::kDeveloperEngagementName);
  ASSERT_NE(nullptr, metric);
  EXPECT_EQ(form_structure_metric, metric->value);
}

// Tests that no UKM is logged when the URL is not valid.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_InvalidUrl) {
  GURL url("");
  AutofillMetrics::LogCardUploadDecisionsUkm(&test_ukm_recorder_, url, 1);
  EXPECT_EQ(0U, test_ukm_recorder_.sources_count());
}

// Tests that no UKM is logged when the ukm service is null.
TEST_F(AutofillMetricsTest, RecordCardUploadDecisionMetric_NoUkmService) {
  GURL url("https://www.google.com");
  AutofillMetrics::LogCardUploadDecisionsUkm(nullptr, url, 1);
  ASSERT_EQ(0U, test_ukm_recorder_.sources_count());
}

}  // namespace autofill
