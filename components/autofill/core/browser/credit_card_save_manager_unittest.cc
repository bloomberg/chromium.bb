// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/credit_card_save_manager.h"

#include <stddef.h>

#include <algorithm>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/guid.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using testing::_;
using testing::AtLeast;
using testing::Return;
using testing::SaveArg;
using testing::UnorderedElementsAre;

namespace autofill {
namespace {

using UkmCardUploadDecisionType = ukm::builders::Autofill_CardUploadDecision;
using UkmDeveloperEngagementType = ukm::builders::Autofill_DeveloperEngagement;

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);
const base::Time kMuchLaterTime = base::Time::FromDoubleT(5000);

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {}

  ~MockAutofillClient() override {}

  MOCK_METHOD2(ConfirmSaveCreditCardLocally,
               void(const CreditCard& card, const base::Closure& callback));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class TestPaymentsClient : public payments::PaymentsClient {
 public:
  TestPaymentsClient(net::URLRequestContextGetter* context_getter,
                     PrefService* pref_service,
                     IdentityProvider* identity_provider,
                     payments::PaymentsClientUnmaskDelegate* unmask_delegate,
                     payments::PaymentsClientSaveDelegate* save_delegate)
      : PaymentsClient(context_getter,
                       pref_service,
                       identity_provider,
                       unmask_delegate,
                       save_delegate),
        save_delegate_(save_delegate) {}

  ~TestPaymentsClient() override {}

  void GetUploadDetails(const std::vector<AutofillProfile>& addresses,
                        const std::vector<const char*>& active_experiments,
                        const std::string& app_locale) override {
    active_experiments_ = active_experiments;
    save_delegate_->OnDidGetUploadDetails(
        app_locale == "en-US" ? AutofillClient::SUCCESS
                              : AutofillClient::PERMANENT_FAILURE,
        ASCIIToUTF16("this is a context token"),
        std::unique_ptr<base::DictionaryValue>(nullptr));
  }

  void UploadCard(const payments::PaymentsClient::UploadRequestDetails&
                      request_details) override {
    active_experiments_ = request_details.active_experiments;
    save_delegate_->OnDidUploadCard(AutofillClient::SUCCESS, server_id_);
  }

  std::string server_id_;
  std::vector<const char*> active_experiments_;

  void SetSaveDelegate(payments::PaymentsClientSaveDelegate* save_delegate) {
    save_delegate_ = save_delegate;
    payments::PaymentsClient::SetSaveDelegate(save_delegate);
  }

 private:
  payments::PaymentsClientSaveDelegate* save_delegate_;

  DISALLOW_COPY_AND_ASSIGN(TestPaymentsClient);
};

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager()
      : PersonalDataManager("en-US"),
        num_times_save_imported_profile_called_(0) {
    CreateTestAutofillProfiles(&web_profiles_);
    CreateTestCreditCards(&local_credit_cards_);
  }

  using PersonalDataManager::set_database;
  using PersonalDataManager::SetPrefService;

  int num_times_save_imported_profile_called() {
    return num_times_save_imported_profile_called_;
  }

  std::string SaveImportedProfile(const AutofillProfile& profile) override {
    num_times_save_imported_profile_called_++;
    AddProfile(profile);
    return profile.guid();
  }

  AutofillProfile* GetProfileWithGUID(const char* guid) {
    for (AutofillProfile* profile : GetProfiles()) {
      if (!profile->guid().compare(guid))
        return profile;
    }
    return nullptr;
  }

  CreditCard* GetCreditCardWithGUID(const char* guid) {
    for (CreditCard* card : GetCreditCards()) {
      if (!card->guid().compare(guid))
        return card;
    }
    return nullptr;
  }

  void AddProfile(const AutofillProfile& profile) override {
    std::unique_ptr<AutofillProfile> profile_ptr =
        std::make_unique<AutofillProfile>(profile);
    profile_ptr->set_modification_date(AutofillClock::Now());
    web_profiles_.push_back(std::move(profile_ptr));
  }

  void AddCreditCard(const CreditCard& credit_card) override {
    std::unique_ptr<CreditCard> local_credit_card =
        std::make_unique<CreditCard>(credit_card);
    local_credit_card->set_modification_date(AutofillClock::Now());
    local_credit_cards_.push_back(std::move(local_credit_card));
  }

  void AddFullServerCreditCard(const CreditCard& credit_card) override {
    AddServerCreditCard(credit_card);
  }

  void RecordUseOf(const AutofillDataModel& data_model) override {
    CreditCard* credit_card = GetCreditCardWithGUID(data_model.guid().c_str());
    if (credit_card)
      credit_card->RecordAndLogUse();

    AutofillProfile* profile = GetProfileWithGUID(data_model.guid().c_str());
    if (profile)
      profile->RecordAndLogUse();
  }

  void RemoveByGUID(const std::string& guid) override {
    CreditCard* credit_card = GetCreditCardWithGUID(guid.c_str());
    if (credit_card) {
      local_credit_cards_.erase(
          std::find_if(local_credit_cards_.begin(), local_credit_cards_.end(),
                       [credit_card](const std::unique_ptr<CreditCard>& ptr) {
                         return ptr.get() == credit_card;
                       }));
    }

    AutofillProfile* profile = GetProfileWithGUID(guid.c_str());
    if (profile) {
      web_profiles_.erase(
          std::find_if(web_profiles_.begin(), web_profiles_.end(),
                       [profile](const std::unique_ptr<AutofillProfile>& ptr) {
                         return ptr.get() == profile;
                       }));
    }
  }

  void ClearAutofillProfiles() { web_profiles_.clear(); }

  void ClearCreditCards() {
    local_credit_cards_.clear();
    server_credit_cards_.clear();
  }

  void AddServerCreditCard(const CreditCard& credit_card) {
    std::unique_ptr<CreditCard> server_credit_card =
        std::make_unique<CreditCard>(credit_card);
    server_credit_card->set_modification_date(AutofillClock::Now());
    server_credit_cards_.push_back(std::move(server_credit_card));
  }

  // Create Elvis card with whitespace in the credit card number.
  void CreateTestCreditCardWithWhitespace() {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Elvis Presley",
                            "4234 5678 9012 3456",  // Visa
                            "04", "2999", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000008");
    local_credit_cards_.push_back(std::move(credit_card));
  }

  // Create Elvis card with separator characters in the credit card number.
  void CreateTestCreditCardWithSeparators() {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Elvis Presley",
                            "4234-5678-9012-3456",  // Visa
                            "04", "2999", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000009");
    local_credit_cards_.push_back(std::move(credit_card));
  }

  void CreateTestCreditCardsYearAndMonth(const char* year, const char* month) {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Miku Hatsune",
                            "4234567890654321",  // Visa
                            month, year, "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000007");
    local_credit_cards_.push_back(std::move(credit_card));
  }

  void CreateTestExpiredCreditCard() {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Homer Simpson",
                            "4234567890654321",  // Visa
                            "05", "2000", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000009");
    local_credit_cards_.push_back(std::move(credit_card));
  }

 private:
  void CreateTestAutofillProfiles(
      std::vector<std::unique_ptr<AutofillProfile>>* profiles) {
    std::unique_ptr<AutofillProfile> profile =
        std::make_unique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "Elvis", "Aaron", "Presley",
                         "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                         "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(std::move(profile));
    profile = std::make_unique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "Charles", "Hardin", "Holley",
                         "buddy@gmail.com", "Decca", "123 Apple St.", "unit 6",
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(std::move(profile));
    profile = std::make_unique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "", "", "", "", "", "", "", "", "", "",
                         "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(std::move(profile));
  }

  void CreateTestCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
    std::unique_ptr<CreditCard> credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000004");
    credit_card->set_use_count(10);
    credit_card->set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(5));
    credit_cards->push_back(std::move(credit_card));

    credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000005");
    credit_card->set_use_count(5);
    credit_card->set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(4));
    credit_cards->push_back(std::move(credit_card));

    credit_card = std::make_unique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "", "", "", "", "");
    credit_card->set_guid("00000000-0000-0000-0000-000000000006");
    credit_cards->push_back(std::move(credit_card));
  }

  size_t num_times_save_imported_profile_called_;

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestFormDataImporter : public FormDataImporter {
 public:
  TestFormDataImporter(AutofillClient* client,
                       payments::PaymentsClient* payments_client,
                       CreditCardSaveManager* credit_card_save_manager,
                       PersonalDataManager* personal_data_manager,
                       const std::string& app_locale)
      : FormDataImporter(client,
                         payments_client,
                         personal_data_manager,
                         app_locale) {
    set_credit_card_save_manager(credit_card_save_manager);
  }
};

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* client,
                      CreditCardSaveManager* credit_card_save_manager,
                      TestPaymentsClient* payments_client,
                      TestPersonalDataManager* personal_data)
      : AutofillManager(driver, client, personal_data),
        personal_data_(personal_data),
        context_getter_(driver->GetURLRequestContext()),
        test_form_data_importer_(
            new TestFormDataImporter(client,
                                     payments_client,
                                     credit_card_save_manager,
                                     personal_data,
                                     "en-US")),
        autofill_enabled_(true),
        credit_card_enabled_(true),
        expected_observed_submission_(true),
        call_parent_upload_form_data_(false) {
    set_payments_client(payments_client);
    set_form_data_importer(test_form_data_importer_);
  }
  ~TestAutofillManager() override {}

  bool IsAutofillEnabled() const override { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  bool IsCreditCardAutofillEnabled() override { return credit_card_enabled_; }

  void set_credit_card_enabled(bool credit_card_enabled) {
    credit_card_enabled_ = credit_card_enabled;
    if (!credit_card_enabled_)
      // Credit card data is refreshed when this pref is changed.
      personal_data_->ClearCreditCards();
  }

  void set_expected_submitted_field_types(
      const std::vector<ServerFieldTypeSet>& expected_types) {
    expected_submitted_field_types_ = expected_types;
  }

  void set_expected_observed_submission(bool expected) {
    expected_observed_submission_ = expected;
  }

  void set_call_parent_upload_form_data(bool value) {
    call_parent_upload_form_data_ = value;
  }

  void UploadFormDataAsyncCallback(const FormStructure* submitted_form,
                                   const base::TimeTicks& load_time,
                                   const base::TimeTicks& interaction_time,
                                   const base::TimeTicks& submission_time,
                                   bool observed_submission) override {
    run_loop_->Quit();

    EXPECT_EQ(expected_observed_submission_, observed_submission);

    // If we have expected field types set, make sure they match.
    if (!expected_submitted_field_types_.empty()) {
      ASSERT_EQ(expected_submitted_field_types_.size(),
                submitted_form->field_count());
      for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
        SCOPED_TRACE(base::StringPrintf(
            "Field %d with value %s", static_cast<int>(i),
            base::UTF16ToUTF8(submitted_form->field(i)->value).c_str()));
        const ServerFieldTypeSet& possible_types =
            submitted_form->field(i)->possible_types();
        EXPECT_EQ(expected_submitted_field_types_[i].size(),
                  possible_types.size());
        for (ServerFieldTypeSet::const_iterator it =
                 expected_submitted_field_types_[i].begin();
             it != expected_submitted_field_types_[i].end(); ++it) {
          EXPECT_TRUE(possible_types.count(*it))
              << "Expected type: " << AutofillType(*it).ToString();
        }
      }
    }

    AutofillManager::UploadFormDataAsyncCallback(
        submitted_form, load_time, interaction_time, submission_time,
        observed_submission);
  }

  // Resets the run loop so that it can wait for an asynchronous form
  // submission to complete.
  void ResetRunLoop() { run_loop_.reset(new base::RunLoop()); }

  // Wait for the asynchronous calls within StartUploadProcess() to complete.
  void WaitForAsyncUploadProcess() { run_loop_->Run(); }

  void UploadFormData(const FormStructure& submitted_form,
                      bool observed_submission) override {
    submitted_form_signature_ = submitted_form.FormSignatureAsStr();

    if (call_parent_upload_form_data_)
      AutofillManager::UploadFormData(submitted_form, observed_submission);
  }

  const std::string GetSubmittedFormSignature() {
    return submitted_form_signature_;
  }

  AutofillProfile* GetProfileWithGUID(const char* guid) {
    return personal_data_->GetProfileWithGUID(guid);
  }

  CreditCard* GetCreditCardWithGUID(const char* guid) {
    return personal_data_->GetCreditCardWithGUID(guid);
  }

  std::vector<CreditCard*> GetLocalCreditCards() const {
    return personal_data_->GetLocalCreditCards();
  }

  std::vector<CreditCard*> GetCreditCards() const {
    return personal_data_->GetCreditCards();
  }

  void AddProfile(std::unique_ptr<AutofillProfile> profile) {
    personal_data_->AddProfile(*profile);
  }

  void AddCreditCard(const CreditCard& credit_card) {
    personal_data_->AddCreditCard(credit_card);
  }

  int GetPackedCreditCardID(int credit_card_id) {
    std::string credit_card_guid =
        base::StringPrintf("00000000-0000-0000-0000-%012d", credit_card_id);

    return MakeFrontendID(credit_card_guid, std::string());
  }

  void AddSeenForm(std::unique_ptr<FormStructure> form) {
    form->set_form_parsed_timestamp(base::TimeTicks::Now());
    form_structures()->push_back(std::move(form));
  }

  void ClearFormStructures() { form_structures()->clear(); }

 private:
  TestPersonalDataManager* personal_data_;        // Weak reference.
  net::URLRequestContextGetter* context_getter_;  // Weak reference.
  TestFormDataImporter* test_form_data_importer_;
  bool autofill_enabled_;
  bool credit_card_enabled_;
  bool expected_observed_submission_;
  bool call_parent_upload_form_data_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::string submitted_form_signature_;
  std::vector<ServerFieldTypeSet> expected_submitted_field_types_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

// Get Ukm sources from the Ukm service.
std::vector<const ukm::UkmSource*> GetUkmSources(
    ukm::TestUkmRecorder* service) {
  std::vector<const ukm::UkmSource*> sources;
  for (const auto& kv : service->GetSources())
    sources.push_back(kv.second.get());

  return sources;
}

}  // anonymous namespace

class TestCreditCardSaveManager : public CreditCardSaveManager {
 public:
  TestCreditCardSaveManager(AutofillDriver* driver,
                            AutofillClient* client,
                            TestPaymentsClient* payments_client,
                            PersonalDataManager* personal_data_manager)
      : CreditCardSaveManager(client,
                              payments_client,
                              "en-US",
                              personal_data_manager),
        test_payments_client_(payments_client),
        credit_card_upload_enabled_(false),
        credit_card_was_uploaded_(false) {
    if (test_payments_client_) {
      test_payments_client_->SetSaveDelegate(this);
    }
  }
  ~TestCreditCardSaveManager() override {}

  bool IsCreditCardUploadEnabled() override {
    return credit_card_upload_enabled_;
  }

  void set_credit_card_upload_enabled(bool credit_card_upload_enabled) {
    credit_card_upload_enabled_ = credit_card_upload_enabled;
  }

  bool credit_card_was_uploaded() { return credit_card_was_uploaded_; }

  const std::vector<const char*>& GetActiveExperiments() const {
    return test_payments_client_->active_experiments_;
  }

  void SetPaymentsClientServerIdForCardUpload(const char* server_id) {
    test_payments_client_->server_id_ = server_id;
  }

 private:
  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) override {
    credit_card_was_uploaded_ = true;
    CreditCardSaveManager::OnDidUploadCard(result, server_id);
  };

  TestPaymentsClient* test_payments_client_;  // Weak reference.
  bool credit_card_upload_enabled_;
  bool credit_card_was_uploaded_;

  DISALLOW_COPY_AND_ASSIGN(TestCreditCardSaveManager);
};

class CreditCardSaveManagerTest : public testing::Test {
 public:
  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.set_database(autofill_client_.GetDatabase());
    personal_data_.SetPrefService(autofill_client_.GetPrefs());
    autofill_driver_.reset(new TestAutofillDriver());
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    autofill_driver_->SetURLRequestContext(request_context_.get());
    payments_client_ = new TestPaymentsClient(
        autofill_driver_->GetURLRequestContext(), autofill_client_.GetPrefs(),
        autofill_client_.GetIdentityProvider(),
        /*unmask_delegate=*/nullptr,
        // Will be set by CreditCardSaveManager's ctor
        /*save_delegate=*/nullptr);
    credit_card_save_manager_ =
        new TestCreditCardSaveManager(autofill_driver_.get(), &autofill_client_,
                                      payments_client_, &personal_data_);
    autofill_manager_.reset(new TestAutofillManager(
        autofill_driver_.get(), &autofill_client_, credit_card_save_manager_,
        payments_client_, &personal_data_));
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    autofill_driver_.reset();

    // Remove the AutofillWebDataService so TestPersonalDataManager does not
    // need to care about removing self as an observer in destruction.
    personal_data_.set_database(scoped_refptr<AutofillWebDataService>(nullptr));
    personal_data_.SetPrefService(nullptr);
    personal_data_.ClearCreditCards();

    request_context_ = nullptr;
  }

  void EnableAutofillOfferLocalSaveIfServerCardManuallyEnteredExperiment() {
    scoped_feature_list_.InitAndEnableFeature(
        kAutofillOfferLocalSaveIfServerCardManuallyEntered);
  }

  void EnableAutofillUpstreamRequestCvcIfMissingExperiment() {
    scoped_feature_list_.InitAndEnableFeature(
        kAutofillUpstreamRequestCvcIfMissing);
  }

  void EnableAutofillUpstreamShowGoogleLogoExperiment() {
    scoped_feature_list_.InitAndEnableFeature(kAutofillUpstreamShowGoogleLogo);
  }

  void EnableAutofillUpstreamShowNewUiExperiment() {
    scoped_feature_list_.InitAndEnableFeature(kAutofillUpstreamShowNewUi);
  }

  void EnableAutofillUpstreamUseAutofillProfileComparator() {
    scoped_feature_list_.InitAndEnableFeature(
        kAutofillUpstreamUseAutofillProfileComparator);
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager_->OnFormsSeen(forms, base::TimeTicks());
  }

  void FormSubmitted(const FormData& form) {
    autofill_manager_->ResetRunLoop();
    if (autofill_manager_->OnWillSubmitForm(form, base::TimeTicks::Now()) &&
        (!personal_data_.GetProfiles().empty() ||
         !personal_data_.GetCreditCards().empty()))
      autofill_manager_->WaitForAsyncUploadProcess();
    autofill_manager_->OnFormSubmitted(form);
  }

  // Populates |form| with data corresponding to a simple credit card form.
  // Note that this actually appends fields to the form data, which can be
  // useful for building up more complex test forms.
  void CreateTestCreditCardFormData(FormData* form,
                                    bool is_https,
                                    bool use_month_type) {
    form->name = ASCIIToUTF16("MyForm");
    if (is_https) {
      form->origin = GURL("https://myform.com/form.html");
      form->action = GURL("https://myform.com/submit.html");
    } else {
      form->origin = GURL("http://myform.com/form.html");
      form->action = GURL("http://myform.com/submit.html");
    }

    FormFieldData field;
    test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
    form->fields.push_back(field);
    if (use_month_type) {
      test::CreateTestFormField("Expiration Date", "ccmonth", "", "month",
                                &field);
      form->fields.push_back(field);
    } else {
      test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                                &field);
      form->fields.push_back(field);
      test::CreateTestFormField("", "ccyear", "", "text", &field);
      form->fields.push_back(field);
    }
    test::CreateTestFormField("CVC", "cvc", "", "text", &field);
    form->fields.push_back(field);
  }

  // Fills the fields in |form| with test data.
  void ManuallyFillAddressForm(const char* first_name,
                               const char* last_name,
                               const char* zip_code,
                               const char* country,
                               FormData* form) {
    for (FormFieldData& field : form->fields) {
      if (base::EqualsASCII(field.name, "firstname"))
        field.value = ASCIIToUTF16(first_name);
      else if (base::EqualsASCII(field.name, "lastname"))
        field.value = ASCIIToUTF16(last_name);
      else if (base::EqualsASCII(field.name, "addr1"))
        field.value = ASCIIToUTF16("123 Maple");
      else if (base::EqualsASCII(field.name, "city"))
        field.value = ASCIIToUTF16("Dallas");
      else if (base::EqualsASCII(field.name, "state"))
        field.value = ASCIIToUTF16("Texas");
      else if (base::EqualsASCII(field.name, "zipcode"))
        field.value = ASCIIToUTF16(zip_code);
      else if (base::EqualsASCII(field.name, "country"))
        field.value = ASCIIToUTF16(country);
    }
  }

  // Tests if credit card data gets saved.
  void TestSaveCreditCards(bool is_https) {
    // Set up our form data.
    FormData form;
    CreateTestCreditCardFormData(&form, is_https, false);
    std::vector<FormData> forms(1, form);
    FormsSeen(forms);

    // Edit the data, and submit.
    form.fields[1].value = ASCIIToUTF16("4111111111111111");
    form.fields[2].value = ASCIIToUTF16("11");
    form.fields[3].value = ASCIIToUTF16("2017");
    EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _));
    FormSubmitted(form);
  }

  void ExpectUniqueFillableFormParsedUkm() {
    // Check that one source is logged.
    ASSERT_EQ(1U, test_ukm_recorder_.sources_count());
    const ukm::UkmSource* source = GetUkmSources(&test_ukm_recorder_)[0];

    // Check that one entry is logged.
    EXPECT_EQ(1U, test_ukm_recorder_.entries_count());
    const ukm::mojom::UkmEntry* entry = test_ukm_recorder_.GetEntry(0);
    EXPECT_EQ(source->id(), entry->source_id);

    EXPECT_EQ(source->id(), entry->source_id);

    // Check if there is an entry for developer engagement decision.
    EXPECT_EQ(base::HashMetricName(UkmDeveloperEngagementType::kEntryName),
              entry->event_hash);
    EXPECT_EQ(1U, entry->metrics.size());

    // Check that the expected developer engagement metric is logged.
    const ukm::mojom::UkmMetric* metric = ukm::TestUkmRecorder::FindMetric(
        entry, UkmDeveloperEngagementType::kDeveloperEngagementName);
    ASSERT_NE(nullptr, metric);
    EXPECT_EQ(1 << AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS,
              metric->value);
  }

  void ExpectUniqueCardUploadDecision(
      const base::HistogramTester& histogram_tester,
      AutofillMetrics::CardUploadDecisionMetric metric) {
    histogram_tester.ExpectUniqueSample("Autofill.CardUploadDecisionMetric",
                                        ToHistogramSample(metric), 1);
  }

  void ExpectCardUploadDecision(
      const base::HistogramTester& histogram_tester,
      AutofillMetrics::CardUploadDecisionMetric metric) {
    histogram_tester.ExpectBucketCount("Autofill.CardUploadDecisionMetric",
                                       ToHistogramSample(metric), 1);
  }

  void ExpectCardUploadDecisionUkm(
      AutofillMetrics::CardUploadDecisionMetric upload_decision) {
    int expected_metric_value = upload_decision;
    ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
                 UkmCardUploadDecisionType::kEntryName, expected_metric_value,
                 1 /* expected_num_matching_entries */);
  }

  void ExpectFillableFormParsedUkm(int num_fillable_forms_parsed) {
    ExpectMetric(UkmDeveloperEngagementType::kDeveloperEngagementName,
                 UkmDeveloperEngagementType::kEntryName,
                 1 << AutofillMetrics::FILLABLE_FORM_PARSED_WITHOUT_TYPE_HINTS,
                 num_fillable_forms_parsed);
  }

  void ExpectMetric(const char* metric_name,
                    const char* entry_name,
                    int expected_metric_value,
                    int expected_num_matching_entries) {
    int num_matching_entries = 0;
    for (size_t i = 0; i < test_ukm_recorder_.entries_count(); ++i) {
      const ukm::mojom::UkmEntry* entry = test_ukm_recorder_.GetEntry(i);
      // Check if there is an entry for |entry_name|.
      if (entry->event_hash == base::HashMetricName(entry_name)) {
        EXPECT_EQ(1UL, entry->metrics.size());

        // Check that the expected |metric_value| is logged.
        const ukm::mojom::UkmMetric* metric =
            ukm::TestUkmRecorder::FindMetric(entry, metric_name);
        ASSERT_NE(nullptr, metric);
        EXPECT_EQ(expected_metric_value, metric->value);
        ++num_matching_entries;
      }
    }
    EXPECT_EQ(expected_num_matching_entries, num_matching_entries);
  }

 protected:
  base::test::ScopedTaskEnvironment scoped_task_environment_;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder_;
  MockAutofillClient autofill_client_;
  std::unique_ptr<TestAutofillDriver> autofill_driver_;
  std::unique_ptr<TestAutofillManager> autofill_manager_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestPersonalDataManager personal_data_;
  base::test::ScopedFeatureList scoped_feature_list_;
  // Ends up getting owned (and destroyed) by TestFormDataImporter:
  TestCreditCardSaveManager* credit_card_save_manager_;
  // Ends up getting owned (and destroyed) by TestAutofillManager:
  TestPaymentsClient* payments_client_;

 private:
  int ToHistogramSample(AutofillMetrics::CardUploadDecisionMetric metric) {
    for (int sample = 0; sample < metric + 1; ++sample)
      if (metric & (1 << sample))
        return sample;

    NOTREACHED();
    return 0;
  }
};

// Tests that credit card data are saved for forms on https
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_ImportFormDataCreditCardHTTPS \
  DISABLED_ImportFormDataCreditCardHTTPS
#else
#define MAYBE_ImportFormDataCreditCardHTTPS ImportFormDataCreditCardHTTPS
#endif
TEST_F(CreditCardSaveManagerTest, MAYBE_ImportFormDataCreditCardHTTPS) {
  TestSaveCreditCards(true);
}

// Tests that credit card data are saved for forms on http
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_ImportFormDataCreditCardHTTP DISABLED_ImportFormDataCreditCardHTTP
#else
#define MAYBE_ImportFormDataCreditCardHTTP ImportFormDataCreditCardHTTP
#endif
TEST_F(CreditCardSaveManagerTest, MAYBE_ImportFormDataCreditCardHTTP) {
  TestSaveCreditCards(false);
}

// Tests that credit card data are saved when autocomplete=off for CC field.
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_CreditCardSavedWhenAutocompleteOff \
  DISABLED_CreditCardSavedWhenAutocompleteOff
#else
#define MAYBE_CreditCardSavedWhenAutocompleteOff \
  CreditCardSavedWhenAutocompleteOff
#endif
TEST_F(CreditCardSaveManagerTest, MAYBE_CreditCardSavedWhenAutocompleteOff) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);

  // Set "autocomplete=off" for cardnumber field.
  form.fields[1].should_autocomplete = false;

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Edit the data, and submit.
  form.fields[1].value = ASCIIToUTF16("4111111111111111");
  form.fields[2].value = ASCIIToUTF16("11");
  form.fields[3].value = ASCIIToUTF16("2017");
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _));
  FormSubmitted(form);
}

// Tests that credit card data are not saved when CC number does not pass the
// Luhn test.
TEST_F(CreditCardSaveManagerTest, InvalidCreditCardNumberIsNotSaved) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Edit the data, and submit.
  std::string card("4408041234567890");
  ASSERT_FALSE(autofill::IsValidCreditCardNumber(ASCIIToUTF16(card)));
  form.fields[1].value = ASCIIToUTF16(card);
  form.fields[2].value = ASCIIToUTF16("11");
  form.fields[3].value = ASCIIToUTF16("2017");
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(form);
}

TEST_F(CreditCardSaveManagerTest, CreditCardDisabledDoesNotSave) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_enabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The credit card should neither be saved locally or uploaded.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard) {
  personal_data_.ClearCreditCards();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());

  // Server did not send a server_id, expect copy of card is not stored.
  EXPECT_TRUE(autofill_manager_->GetCreditCards().empty());
  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", true, 1);
  // Verify that UMA for "DaysSincePreviousUse" was not logged because we
  // modified the profile.
  histogram_tester.ExpectTotalCount(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile", 0);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_RequestCVCEnabled_DoesNotTrigger) {
  EnableAutofillUpstreamRequestCvcIfMissingExperiment();

  personal_data_.ClearCreditCards();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  // Submitted form included CVC, so user did not need to enter CVC.
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCardAndSaveCopy) {
  personal_data_.ClearCreditCards();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  const char* const server_id = "InstrumentData:1234";
  credit_card_save_manager_->SetPaymentsClientServerIdForCardUpload(server_id);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  const char* const card_number = "4111111111111111";
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16(card_number);
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  FormSubmitted(credit_card_form);

  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(autofill_manager_->GetLocalCreditCards().empty());
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // See |OfferStoreUnmaskedCards|
  EXPECT_TRUE(autofill_manager_->GetCreditCards().empty());
#else
  ASSERT_EQ(1U, autofill_manager_->GetCreditCards().size());
  const CreditCard* const saved_card = autofill_manager_->GetCreditCards()[0];
  EXPECT_EQ(CreditCard::OK, saved_card->GetServerStatus());
  EXPECT_EQ(base::ASCIIToUTF16("1111"), saved_card->LastFourDigits());
  EXPECT_EQ(kVisaCard, saved_card->network());
  EXPECT_EQ(11, saved_card->expiration_month());
  EXPECT_EQ(2017, saved_card->expiration_year());
  EXPECT_EQ(server_id, saved_card->server_id());
  EXPECT_EQ(CreditCard::FULL_SERVER_CARD, saved_card->record_type());
  EXPECT_EQ(base::ASCIIToUTF16(card_number), saved_card->number());
#endif
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_FeatureNotEnabled) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(false);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The save prompt should be shown instead of doing an upload.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _));
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CvcUnavailable) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ExpectUniqueFillableFormParsedUkm();

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // CVC MISSING

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_VALUE_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CvcInvalidLength) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("1234");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::INVALID_CVC_VALUE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::INVALID_CVC_VALUE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_MultipleCvcFields) {
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("CVC (hidden)", "cvc1", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc2", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // CVC MISSING
  credit_card_form.fields[5].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // A CVC value appeared in one of the two CVC fields, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoCvcFieldOnForm) {
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.  Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");

  base::HistogramTester histogram_tester;

  // Upload should not happen because user did not provide CVC.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_InvalidCvcInNonCvcField) {
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Random Field", "random", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen({credit_card_form});

  // Enter an invalid cvc in "Random Field" and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("1234");

  base::HistogramTester histogram_tester;

  // Upload should not happen because user did not provide CVC.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInNonCvcField) {
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Random Field", "random", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen({credit_card_form});

  // Enter an invalid cvc in "Random Field" and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Upload should not happen because user did not provide CVC.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInAddressField) {
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data. Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen({credit_card_form});

  // Enter an invalid cvc in "Random Field" and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Upload should not happen because user did not provide CVC.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_UploadCreditCard_NoCvcFieldOnForm_UserEntersCvc \
  DISABLED_UploadCreditCard_NoCvcFieldOnForm_UserEntersCvc
#else
#define MAYBE_UploadCreditCard_NoCvcFieldOnForm_UserEntersCvc \
  UploadCreditCard_NoCvcFieldOnForm_UserEntersCvc
#endif
TEST_F(CreditCardSaveManagerTest,
       MAYBE_UploadCreditCard_NoCvcFieldOnForm_UserEntersCvc) {
  EnableAutofillUpstreamRequestCvcIfMissingExperiment();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.  Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");

  base::HistogramTester histogram_tester;

  // Upload should still happen as long as the user provides CVC in the bubble.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(credit_card_save_manager_->GetActiveExperiments(),
              UnorderedElementsAre(kAutofillUpstreamRequestCvcIfMissing.name));

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester, AutofillMetrics::UPLOAD_OFFERED);
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectMetric(
      UkmCardUploadDecisionType::kUploadDecisionName,
      UkmCardUploadDecisionType::kEntryName,
      AutofillMetrics::UPLOAD_OFFERED | AutofillMetrics::CVC_FIELD_NOT_FOUND,
      1 /* expected_num_matching_entries */);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoCvcFieldOnFormExperimentOff) {
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Remove the profiles that were created in the TestPersonalDataManager
  // constructor because they would result in conflicting names that would
  // prevent the upload.
  personal_data_.ClearAutofillProfiles();

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.  Note that CVC field is missing.
  FormData credit_card_form;
  credit_card_form.name = ASCIIToUTF16("MyForm");
  credit_card_form.origin = GURL("https://myform.com/form.html");
  credit_card_form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  credit_card_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  credit_card_form.fields.push_back(field);

  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

// kAutofillUpstreamShowNewUi and kAutofillUpstreamShowGoogleLogo flags are
// currently not available on Android.
#if !defined(OS_ANDROID)
TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_AddNewUiFlagStateToRequestIfExperimentOn) {
  EnableAutofillUpstreamShowNewUiExperiment();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Confirm upload happened and the new UI flag was sent in the request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(credit_card_save_manager_->GetActiveExperiments(),
              UnorderedElementsAre(kAutofillUpstreamShowNewUi.name));
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_DoNotAddNewUiFlagStateToRequestIfExperimentOff) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Confirm upload happened and the new UI flag was not sent in the request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_AddShowGoogleLogoFlagStateToRequestIfExperimentOn) {
  EnableAutofillUpstreamShowGoogleLogoExperiment();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Confirm upload happened and the show Google logo flag was sent in the
  // request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(credit_card_save_manager_->GetActiveExperiments(),
              UnorderedElementsAre(kAutofillUpstreamShowGoogleLogo.name));
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_DoNotAddShowGoogleLogoFlagStateToRequestIfExpOff) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // Confirm upload happened and the show Google logo flag was not sent in the
  // request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());
}
#endif

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoProfileAvailable) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Bob Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entries are logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoRecentlyUsedProfile) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a profile.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set the current time to another value.
  test_clock.SetNow(kMuchLaterTime);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_RECENTLY_USED_ADDRESS);
  // Verify the histogram entry for recent profile modification.
  histogram_tester.ExpectUniqueSample(
      "Autofill.HasModifiedProfile.CreditCardFormSubmission", false, 1);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CvcUnavailableAndNoProfileAvailable) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Don't fill or submit an address form.

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("");  // CVC MISSING

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  // Note that AutofillManager should *check* for both no CVC and no address
  // profile, but the no CVC case should have priority over being reported.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entries were logged.
  ExpectCardUploadDecision(histogram_tester,
                           AutofillMetrics::CVC_VALUE_NOT_FOUND);
  ExpectCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectMetric(UkmCardUploadDecisionType::kUploadDecisionName,
               UkmCardUploadDecisionType::kEntryName,
               AutofillMetrics::CVC_VALUE_NOT_FOUND |
                   AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE,
               1 /* expected_num_matching_entries */);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoNameAvailable) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // But omit the name:
  ManuallyFillAddressForm("", "", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ZipCodesConflict) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);
  ExpectFillableFormParsedUkm(2 /* num_fillable_forms_parsed */);

  ManuallyFillAddressForm("Flo", "Master", "77401-8294", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Flo", "Master", "77401-1234", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));
  ExpectFillableFormParsedUkm(3 /* num_fillable_forms_parsed */);

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ZipCodesDiscardWhitespace) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  FormsSeen({address_form1, address_form2});

  ManuallyFillAddressForm("Flo", "Master", "H3B2Y5", "CA", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Flo", "Master", "h3b 2y5", "CA", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_ZipCodesDiscardWhitespace_ComparatorEnabled) {
  EnableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  FormsSeen({address_form1, address_form2});

  ManuallyFillAddressForm("Flo", "Master", "H3B2Y5", "CA", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Flo", "Master", "h3b 2y5", "CA", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Zips match because we discard whitespace before comparison.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      credit_card_save_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_ZipCodesHavePrefixMatch) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different zip codes.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Flo", "Master", "77401-8294", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but don't include a name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // One zip is a prefix of the other, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoZipCodeAvailable) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  // Autofill's validation requirements for Venezuala ("VE", see
  // src/components/autofill/core/browser/country_data.cc) do not require zip
  // codes. We use Venezuala here because to use the US (or one of many other
  // countries which autofill requires a zip code for) would result in no
  // address being imported at all, and then we never reach the check for
  // missing zip code in the upload code.
  ManuallyFillAddressForm("Flo", "Master", "" /* zip_code */, "Venezuela",
                          &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Neither a local save nor an upload should happen in this case.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormHasMiddleInitial) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can be different case.
  ManuallyFillAddressForm("flo", "master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // And they can have a middle initial even if the other names don't.
  ManuallyFillAddressForm("Flo W", "Master", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but use the name with a middle initial *and* period, and
  // submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo W. Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CCFormHasMiddleInitial_ComparatorEnabled) {
  EnableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can be different case.
  ManuallyFillAddressForm("flo", "master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // And they can have a middle initial even if the other names don't.
  ManuallyFillAddressForm("Flo W", "Master", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but use the name with a middle initial *and* period, and
  // submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo W. Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      credit_card_save_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NoMiddleInitialInCCForm) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can have different variations of middle initials.
  ManuallyFillAddressForm("flo w.", "master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);
  ManuallyFillAddressForm("Flo W", "Master", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but do not use middle initial.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_NoMiddleInitialInCCForm_ComparatorEnabled) {
  EnableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  // Names can have different variations of middle initials.
  ManuallyFillAddressForm("flo w.", "master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);
  ManuallyFillAddressForm("Flo W", "Master", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the data, but do not use middle initial.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormHasMiddleName) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit address form without middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the name by adding a middle name.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Quincy Adams");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely but we have disabled comparator. Upload should not
  // happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CCFormHasMiddleName_ComparatorEnabled) {
  EnableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit address form without middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the name by adding a middle name.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Quincy Adams");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      credit_card_save_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_CCFormRemovesMiddleName) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit address form with middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John Quincy", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the name by removing middle name.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Adams");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // With the comparator disabled, names do not match; upload should not happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(CreditCardSaveManagerTest,
       UploadCreditCard_CCFormRemovesMiddleName_ComparatorEnabled) {
  EnableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit address form with middle name.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("John Quincy", "Adams", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the name by removing middle name.
  credit_card_form.fields[0].value = ASCIIToUTF16("John Adams");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names match loosely, upload should happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      credit_card_save_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_NamesHaveToMatch) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);

  std::vector<FormData> address_forms;
  address_forms.push_back(address_form1);
  address_forms.push_back(address_form2);
  FormsSeen(address_forms);

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  ManuallyFillAddressForm("Master", "Blaster", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Bob Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names are required to match, upload should not happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(credit_card_save_manager_->GetActiveExperiments().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_IgnoreOldProfiles) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // Advance the current time. Since |address_form1| will not be a recently
  // used address profile, we will not include it in the candidate profiles.
  test_clock.SetNow(kMuchLaterTime);

  ManuallyFillAddressForm("Master", "Blaster", "77401", "US", &address_form2);
  FormSubmitted(address_form2);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Master Blaster");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Name matches recently used profile, should offer upload.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_LogPreviousUseDate) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);
  const std::vector<AutofillProfile*>& profiles =
      personal_data_.GetProfilesToSuggest();
  ASSERT_EQ(1U, profiles.size());

  // Advance the current time and simulate use of the address profile.
  test_clock.SetNow(kMuchLaterTime);
  profiles[0]->RecordAndLogUse();

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen({credit_card_form});

  // Edit the credit card form and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that UMA for "DaysSincePreviousUse" is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile",
      (kMuchLaterTime - kArbitraryTime).InDays(),
      /*expected_count=*/1);
}

TEST_F(CreditCardSaveManagerTest, UploadCreditCard_UploadDetailsFails) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);

  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  credit_card_save_manager_->SetAppLocale("pt-BR");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // The save prompt should be shown instead of doing an upload.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _));
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
}

TEST_F(CreditCardSaveManagerTest, DuplicateMaskedCreditCard) {
  EnableAutofillOfferLocalSaveIfServerCardManuallyEnteredExperiment();

  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);
  credit_card_save_manager_->SetAppLocale("en-US");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Add a masked credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11", "2017",
                          "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // The local save prompt should be shown.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _));
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());
}

TEST_F(CreditCardSaveManagerTest, DuplicateMaskedCreditCard_ExperimentOff) {
  personal_data_.ClearAutofillProfiles();
  credit_card_save_manager_->set_credit_card_upload_enabled(true);
  credit_card_save_manager_->SetAppLocale("en-US");

  // Create, fill and submit an address form in order to establish a recent
  // profile which can be selected for the upload request.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen(std::vector<FormData>(1, address_form));
  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Add a masked credit card whose |TypeAndLastFourDigits| matches what we will
  // enter below.
  CreditCard credit_card(CreditCard::MASKED_SERVER_CARD, "a123");
  test::SetCreditCardInfo(&credit_card, "Flo Master", "1111", "11", "2017",
                          "1");
  credit_card.SetNetworkForMaskedCard(kVisaCard);
  personal_data_.AddServerCreditCard(credit_card);

  // Set up our credit card form data.
  FormData credit_card_form;
  CreateTestCreditCardFormData(&credit_card_form, true, false);
  FormsSeen(std::vector<FormData>(1, credit_card_form));

  // Edit the data, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  // The local save prompt should not be shown because the experiment is off.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(credit_card_save_manager_->credit_card_was_uploaded());
}

}  // namespace autofill
