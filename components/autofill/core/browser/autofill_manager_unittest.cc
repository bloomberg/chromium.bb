// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_manager.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/metrics_hashes.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_download_manager.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/metrics/proto/ukm/entry.pb.h"
#include "components/prefs/pref_service.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/ukm/ukm_source.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_params_manager.h"
#include "net/base/url_util.h"
#include "net/url_request/url_request_context_getter.h"
#include "net/url_request/url_request_test_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/rect.h"
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

const int kDefaultPageID = 137;

const char kUTF8MidlineEllipsis[] =
    "  "
    "\xE2\x80\xA2\xE2\x80\x86"
    "\xE2\x80\xA2\xE2\x80\x86"
    "\xE2\x80\xA2\xE2\x80\x86"
    "\xE2\x80\xA2\xE2\x80\x86";

const base::Time kArbitraryTime = base::Time::FromDoubleT(25);
const base::Time kMuchLaterTime = base::Time::FromDoubleT(5000);

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {}

  ~MockAutofillClient() override {}

  MOCK_METHOD2(ConfirmSaveCreditCardLocally,
               void(const CreditCard& card, const base::Closure& callback));

  MOCK_METHOD0(ShouldShowSigninPromo, bool());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

class TestPaymentsClient : public payments::PaymentsClient {
 public:
  TestPaymentsClient(net::URLRequestContextGetter* context_getter,
                     payments::PaymentsClientDelegate* delegate)
      : PaymentsClient(context_getter, delegate), delegate_(delegate) {}

  ~TestPaymentsClient() override {}

  void GetUploadDetails(const std::vector<AutofillProfile>& addresses,
                        const std::vector<const char*>& active_experiments,
                        const std::string& app_locale) override {
    active_experiments_ = active_experiments;
    delegate_->OnDidGetUploadDetails(
        app_locale == "en-US" ? AutofillClient::SUCCESS
                              : AutofillClient::PERMANENT_FAILURE,
        ASCIIToUTF16("this is a context token"),
        std::unique_ptr<base::DictionaryValue>(nullptr));
  }

  void UploadCard(const payments::PaymentsClient::UploadRequestDetails&
                      request_details) override {
    active_experiments_ = request_details.active_experiments;
    delegate_->OnDidUploadCard(AutofillClient::SUCCESS, server_id_);
  }

  std::string server_id_;
  std::vector<const char*> active_experiments_;

 private:
  payments::PaymentsClientDelegate* const delegate_;

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
    return NULL;
  }

  CreditCard* GetCreditCardWithGUID(const char* guid) {
    for (CreditCard* card : GetCreditCards()) {
      if (!card->guid().compare(guid))
        return card;
    }
    return NULL;
  }

  void AddProfile(const AutofillProfile& profile) override {
    std::unique_ptr<AutofillProfile> profile_ptr =
        base::MakeUnique<AutofillProfile>(profile);
    profile_ptr->set_modification_date(AutofillClock::Now());
    web_profiles_.push_back(std::move(profile_ptr));
  }

  void AddCreditCard(const CreditCard& credit_card) override {
    std::unique_ptr<CreditCard> local_credit_card =
        base::MakeUnique<CreditCard>(credit_card);
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
        base::MakeUnique<CreditCard>(credit_card);
    server_credit_card->set_modification_date(AutofillClock::Now());
    server_credit_cards_.push_back(std::move(server_credit_card));
  }

  // Create Elvis card with whitespace in the credit card number.
  void CreateTestCreditCardWithWhitespace() {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Elvis Presley",
                            "4234 5678 9012 3456",  // Visa
                            "04", "2999", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000008");
    local_credit_cards_.push_back(std::move(credit_card));
  }

  // Create Elvis card with separator characters in the credit card number.
  void CreateTestCreditCardWithSeparators() {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Elvis Presley",
                            "4234-5678-9012-3456",  // Visa
                            "04", "2999", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000009");
    local_credit_cards_.push_back(std::move(credit_card));
  }

  void CreateTestCreditCardsYearAndMonth(const char* year, const char* month) {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Miku Hatsune",
                            "4234567890654321",  // Visa
                            month, year, "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000007");
    local_credit_cards_.push_back(std::move(credit_card));
  }

  void CreateTestExpiredCreditCard() {
    ClearCreditCards();
    std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>();
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
                         "Lubbock", "Texas", "79401", "US", "23456789012");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(std::move(profile));
    profile = base::MakeUnique<AutofillProfile>();
    test::SetProfileInfo(profile.get(), "", "", "", "", "", "", "", "", "", "",
                         "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(std::move(profile));
  }

  void CreateTestCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* credit_cards) {
    std::unique_ptr<CreditCard> credit_card = base::MakeUnique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Elvis Presley",
                            "4234567890123456",  // Visa
                            "04", "2999", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000004");
    credit_card->set_use_count(10);
    credit_card->set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(5));
    credit_cards->push_back(std::move(credit_card));

    credit_card = base::MakeUnique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2998", "1");
    credit_card->set_guid("00000000-0000-0000-0000-000000000005");
    credit_card->set_use_count(5);
    credit_card->set_use_date(AutofillClock::Now() -
                              base::TimeDelta::FromDays(4));
    credit_cards->push_back(std::move(credit_card));

    credit_card = base::MakeUnique<CreditCard>();
    test::SetCreditCardInfo(credit_card.get(), "", "", "", "", "");
    credit_card->set_guid("00000000-0000-0000-0000-000000000006");
    credit_cards->push_back(std::move(credit_card));
  }

  size_t num_times_save_imported_profile_called_;

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestAutofillDownloadManager : public AutofillDownloadManager {
 public:
  TestAutofillDownloadManager(AutofillDriver* driver,
                              AutofillDownloadManager::Observer* observer)
      : AutofillDownloadManager(driver, observer) {}

  bool StartQueryRequest(const std::vector<FormStructure*>& forms) override {
    last_queried_forms_ = forms;
    return true;
  }

  // Verify that the last queried forms equal |expected_forms|.
  void VerifyLastQueriedForms(const std::vector<FormData>& expected_forms) {
    ASSERT_EQ(expected_forms.size(), last_queried_forms_.size());
    for (size_t i = 0; i < expected_forms.size(); ++i) {
      EXPECT_EQ(*last_queried_forms_[i], expected_forms[i]);
    }
  }

  MOCK_METHOD5(StartUploadRequest,
               bool(const FormStructure&,
                    bool,
                    const ServerFieldTypeSet&,
                    const std::string&,
                    bool));

 private:
  std::vector<FormStructure*> last_queried_forms_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDownloadManager);
};

void ExpectFilledField(const char* expected_label,
                       const char* expected_name,
                       const char* expected_value,
                       const char* expected_form_control_type,
                       const FormFieldData& field) {
  SCOPED_TRACE(expected_label);
  EXPECT_EQ(UTF8ToUTF16(expected_label), field.label);
  EXPECT_EQ(UTF8ToUTF16(expected_name), field.name);
  EXPECT_EQ(UTF8ToUTF16(expected_value), field.value);
  EXPECT_EQ(expected_form_control_type, field.form_control_type);
}

// Verifies that the |filled_form| has been filled with the given data.
// Verifies address fields if |has_address_fields| is true, and verifies
// credit card fields if |has_credit_card_fields| is true. Verifies both if both
// are true. |use_month_type| is used for credit card input month type.
void ExpectFilledForm(int page_id,
                      const FormData& filled_form,
                      int expected_page_id,
                      const char* first,
                      const char* middle,
                      const char* last,
                      const char* address1,
                      const char* address2,
                      const char* city,
                      const char* state,
                      const char* postal_code,
                      const char* country,
                      const char* phone,
                      const char* email,
                      const char* name_on_card,
                      const char* card_number,
                      const char* expiration_month,
                      const char* expiration_year,
                      bool has_address_fields,
                      bool has_credit_card_fields,
                      bool use_month_type) {
  // The number of fields in the address and credit card forms created above.
  const size_t kAddressFormSize = 11;
  const size_t kCreditCardFormSize = use_month_type ? 4 : 5;

  EXPECT_EQ(expected_page_id, page_id);
  EXPECT_EQ(ASCIIToUTF16("MyForm"), filled_form.name);
  if (has_credit_card_fields) {
    EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action);
  } else {
    EXPECT_EQ(GURL("http://myform.com/form.html"), filled_form.origin);
    EXPECT_EQ(GURL("http://myform.com/submit.html"), filled_form.action);
  }

  size_t form_size = 0;
  if (has_address_fields)
    form_size += kAddressFormSize;
  if (has_credit_card_fields)
    form_size += kCreditCardFormSize;
  ASSERT_EQ(form_size, filled_form.fields.size());

  if (has_address_fields) {
    ExpectFilledField("First Name", "firstname", first, "text",
                      filled_form.fields[0]);
    ExpectFilledField("Middle Name", "middlename", middle, "text",
                      filled_form.fields[1]);
    ExpectFilledField("Last Name", "lastname", last, "text",
                      filled_form.fields[2]);
    ExpectFilledField("Address Line 1", "addr1", address1, "text",
                      filled_form.fields[3]);
    ExpectFilledField("Address Line 2", "addr2", address2, "text",
                      filled_form.fields[4]);
    ExpectFilledField("City", "city", city, "text", filled_form.fields[5]);
    ExpectFilledField("State", "state", state, "text", filled_form.fields[6]);
    ExpectFilledField("Postal Code", "zipcode", postal_code, "text",
                      filled_form.fields[7]);
    ExpectFilledField("Country", "country", country, "text",
                      filled_form.fields[8]);
    ExpectFilledField("Phone Number", "phonenumber", phone, "tel",
                      filled_form.fields[9]);
    ExpectFilledField("Email", "email", email, "email", filled_form.fields[10]);
  }

  if (has_credit_card_fields) {
    size_t offset = has_address_fields ? kAddressFormSize : 0;
    ExpectFilledField("Name on Card", "nameoncard", name_on_card, "text",
                      filled_form.fields[offset + 0]);
    ExpectFilledField("Card Number", "cardnumber", card_number, "text",
                      filled_form.fields[offset + 1]);
    if (use_month_type) {
      std::string exp_year = expiration_year;
      std::string exp_month = expiration_month;
      std::string date;
      if (!exp_year.empty() && !exp_month.empty())
        date = exp_year + "-" + exp_month;

      ExpectFilledField("Expiration Date", "ccmonth", date.c_str(), "month",
                        filled_form.fields[offset + 2]);
    } else {
      ExpectFilledField("Expiration Date", "ccmonth", expiration_month, "text",
                        filled_form.fields[offset + 2]);
      ExpectFilledField("", "ccyear", expiration_year, "text",
                        filled_form.fields[offset + 3]);
    }
  }
}

void ExpectFilledAddressFormElvis(int page_id,
                                  const FormData& filled_form,
                                  int expected_page_id,
                                  bool has_credit_card_fields) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "Elvis", "Aaron",
                   "Presley", "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "12345678901",
                   "theking@gmail.com", "", "", "", "", true,
                   has_credit_card_fields, false);
}

void ExpectFilledCreditCardFormElvis(int page_id,
                                     const FormData& filled_form,
                                     int expected_page_id,
                                     bool has_address_fields) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "", "", "", "", "",
                   "", "", "", "", "", "", "Elvis Presley", "4234567890123456",
                   "04", "2999", has_address_fields, true, false);
}

void ExpectFilledCreditCardYearMonthWithYearMonth(int page_id,
                                                  const FormData& filled_form,
                                                  int expected_page_id,
                                                  bool has_address_fields,
                                                  const char* year,
                                                  const char* month) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "", "", "", "", "",
                   "", "", "", "", "", "", "Miku Hatsune", "4234567890654321",
                   month, year, has_address_fields, true, true);
}

class MockAutocompleteHistoryManager : public AutocompleteHistoryManager {
 public:
  MockAutocompleteHistoryManager(AutofillDriver* driver, AutofillClient* client)
      : AutocompleteHistoryManager(driver, client) {}

  MOCK_METHOD4(OnGetAutocompleteSuggestions,
               void(int query_id,
                    const base::string16& name,
                    const base::string16& prefix,
                    const std::string& form_control_type));
  MOCK_METHOD1(OnWillSubmitForm, void(const FormData& form));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutocompleteHistoryManager);
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver()
      : is_incognito_(false), did_interact_with_credit_card_form_(false) {}

  // Mock methods to enable testability.
  MOCK_METHOD3(SendFormDataToRenderer,
               void(int query_id,
                    RendererFormDataAction action,
                    const FormData& data));

  MOCK_METHOD1(SendAutofillTypePredictionsToRenderer,
               void(const std::vector<FormStructure*>& forms));

  void SetIsIncognito(bool is_incognito) { is_incognito_ = is_incognito; }

  bool IsIncognito() const override { return is_incognito_; }

  void DidInteractWithCreditCardForm() override {
    did_interact_with_credit_card_form_ = true;
  }

  void ClearDidInteractWithCreditCardForm() {
    did_interact_with_credit_card_form_ = false;
  }

  bool did_interact_with_credit_card_form() const {
    return did_interact_with_credit_card_form_;
  }

 private:
  bool is_incognito_;
  bool did_interact_with_credit_card_form_;
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver,
                      AutofillClient* client,
                      TestPersonalDataManager* personal_data)
      : AutofillManager(driver, client, personal_data),
        personal_data_(personal_data),
        context_getter_(driver->GetURLRequestContext()),
        test_payments_client_(new TestPaymentsClient(context_getter_, this)),
        autofill_enabled_(true),
        credit_card_upload_enabled_(false),
        credit_card_was_uploaded_(false),
        expected_observed_submission_(true),
        call_parent_upload_form_data_(false) {
    set_payments_client(test_payments_client_);
  }
  ~TestAutofillManager() override {}

  bool IsAutofillEnabled() const override { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  bool IsCreditCardUploadEnabled() override {
    return credit_card_upload_enabled_;
  }

  void set_credit_card_upload_enabled(bool credit_card_upload_enabled) {
    credit_card_upload_enabled_ = credit_card_upload_enabled;
  }

  bool credit_card_was_uploaded() { return credit_card_was_uploaded_; }

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

  const std::vector<CreditCard*>& GetCreditCards() const {
    return personal_data_->GetCreditCards();
  }

  void AddProfile(std::unique_ptr<AutofillProfile> profile) {
    personal_data_->AddProfile(*profile);
  }

  void AddCreditCard(const CreditCard& credit_card) {
    personal_data_->AddCreditCard(credit_card);
  }

  const std::vector<const char*>& GetActiveExperiments() const {
    return test_payments_client_->active_experiments_;
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

  void ResetPaymentsClientForCardUpload(const char* server_id) {
    TestPaymentsClient* payments_client =
        new TestPaymentsClient(context_getter_, this);
    payments_client->server_id_ = server_id;
    set_payments_client(payments_client);
  }

 private:
  void OnDidUploadCard(AutofillClient::PaymentsRpcResult result,
                       const std::string& server_id) override {
    credit_card_was_uploaded_ = true;
    AutofillManager::OnDidUploadCard(result, server_id);
  };

  TestPersonalDataManager* personal_data_;        // Weak reference.
  net::URLRequestContextGetter* context_getter_;  // Weak reference.
  TestPaymentsClient* test_payments_client_;      // Weak reference.
  bool autofill_enabled_;
  bool credit_card_upload_enabled_;
  bool credit_card_was_uploaded_;
  bool expected_observed_submission_;
  bool call_parent_upload_form_data_;

  std::unique_ptr<base::RunLoop> run_loop_;

  std::string submitted_form_signature_;
  std::vector<ServerFieldTypeSet> expected_submitted_field_types_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  explicit TestAutofillExternalDelegate(AutofillManager* autofill_manager,
                                        AutofillDriver* autofill_driver)
      : AutofillExternalDelegate(autofill_manager, autofill_driver),
        on_query_seen_(false),
        on_suggestions_returned_seen_(false) {}
  ~TestAutofillExternalDelegate() override {}

  void OnQuery(int query_id,
               const FormData& form,
               const FormFieldData& field,
               const gfx::RectF& bounds) override {
    on_query_seen_ = true;
    on_suggestions_returned_seen_ = false;
  }

  void OnSuggestionsReturned(
      int query_id,
      const std::vector<Suggestion>& suggestions) override {
    on_suggestions_returned_seen_ = true;
    query_id_ = query_id;
    suggestions_ = suggestions;
  }

  void CheckSuggestions(int expected_page_id,
                        size_t expected_num_suggestions,
                        const Suggestion expected_suggestions[]) {
    // Ensure that these results are from the most recent query.
    EXPECT_TRUE(on_suggestions_returned_seen_);

    EXPECT_EQ(expected_page_id, query_id_);
    ASSERT_LE(expected_num_suggestions, suggestions_.size());
    for (size_t i = 0; i < expected_num_suggestions; ++i) {
      SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
      EXPECT_EQ(expected_suggestions[i].value, suggestions_[i].value);
      EXPECT_EQ(expected_suggestions[i].label, suggestions_[i].label);
      EXPECT_EQ(expected_suggestions[i].icon, suggestions_[i].icon);
      EXPECT_EQ(expected_suggestions[i].frontend_id,
                suggestions_[i].frontend_id);
    }
    ASSERT_EQ(expected_num_suggestions, suggestions_.size());
  }

  // Wrappers around the above GetSuggestions call that take a hardcoded number
  // of expected results so callsites are cleaner.
  void CheckSuggestions(int expected_page_id, const Suggestion& suggestion0) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    CheckSuggestions(expected_page_id, 1, &suggestion_vector[0]);
  }
  void CheckSuggestions(int expected_page_id,
                        const Suggestion& suggestion0,
                        const Suggestion& suggestion1) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    suggestion_vector.push_back(suggestion1);
    CheckSuggestions(expected_page_id, 2, &suggestion_vector[0]);
  }
  void CheckSuggestions(int expected_page_id,
                        const Suggestion& suggestion0,
                        const Suggestion& suggestion1,
                        const Suggestion& suggestion2) {
    std::vector<Suggestion> suggestion_vector;
    suggestion_vector.push_back(suggestion0);
    suggestion_vector.push_back(suggestion1);
    suggestion_vector.push_back(suggestion2);
    CheckSuggestions(expected_page_id, 3, &suggestion_vector[0]);
  }
  // Check that the autofill suggestions were sent, and that they match a page
  // but contain no results.
  void CheckNoSuggestions(int expected_page_id) {
    CheckSuggestions(expected_page_id, 0, nullptr);
  }
  // Check that the autofill suggestions were sent, and that they match a page
  // and contain a specific number of suggestions.
  void CheckSuggestionCount(int expected_page_id,
                            size_t expected_num_suggestions) {
    // Ensure that these results are from the most recent query.
    EXPECT_TRUE(on_suggestions_returned_seen_);

    EXPECT_EQ(expected_page_id, query_id_);
    ASSERT_EQ(expected_num_suggestions, suggestions_.size());
  }

  bool on_query_seen() const { return on_query_seen_; }

  bool on_suggestions_returned_seen() const {
    return on_suggestions_returned_seen_;
  }

 private:
  // Records if OnQuery has been called yet.
  bool on_query_seen_;

  // Records if OnSuggestionsReturned has been called after the most recent
  // call to OnQuery.
  bool on_suggestions_returned_seen_;

  // The query id of the most recent Autofill query.
  int query_id_;

  // The results returned by the most recent Autofill query.
  std::vector<Suggestion> suggestions_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillExternalDelegate);
};

// Get Ukm sources from the Ukm service.
std::vector<const ukm::UkmSource*> GetUkmSources(
    ukm::TestUkmRecorder* service) {
  std::vector<const ukm::UkmSource*> sources;
  for (const auto& kv : service->GetSources())
    sources.push_back(kv.second.get());

  return sources;
}

}  // namespace

class AutofillManagerTest : public testing::Test {
 public:
  AutofillManagerTest() {}

  void SetUp() override {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.set_database(autofill_client_.GetDatabase());
    personal_data_.SetPrefService(autofill_client_.GetPrefs());
    autofill_driver_.reset(new testing::NiceMock<MockAutofillDriver>());
    request_context_ = new net::TestURLRequestContextGetter(
        base::ThreadTaskRunnerHandle::Get());
    autofill_driver_->SetURLRequestContext(request_context_.get());
    autofill_manager_.reset(new TestAutofillManager(
        autofill_driver_.get(), &autofill_client_, &personal_data_));
    download_manager_ = new TestAutofillDownloadManager(
        autofill_driver_.get(), autofill_manager_.get());
    // AutofillManager takes ownership of |download_manager_|.
    autofill_manager_->set_download_manager(download_manager_);
    external_delegate_.reset(new TestAutofillExternalDelegate(
        autofill_manager_.get(), autofill_driver_.get()));
    autofill_manager_->SetExternalDelegate(external_delegate_.get());

    variation_params_.ClearAllVariationParams();
  }

  void TearDown() override {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    autofill_driver_.reset();

    // Remove the AutofillWebDataService so TestPersonalDataManager does not
    // need to care about removing self as an observer in destruction.
    personal_data_.set_database(scoped_refptr<AutofillWebDataService>(NULL));
    personal_data_.SetPrefService(NULL);
    personal_data_.ClearCreditCards();

    request_context_ = nullptr;
  }

  void GetAutofillSuggestions(int query_id,
                              const FormData& form,
                              const FormFieldData& field) {
    autofill_manager_->OnQueryFormFieldAutofill(query_id, form, field,
                                                gfx::RectF());
  }

  void GetAutofillSuggestions(const FormData& form,
                              const FormFieldData& field) {
    GetAutofillSuggestions(kDefaultPageID, form, field);
  }

  void AutocompleteSuggestionsReturned(
      const std::vector<base::string16>& result) {
    autofill_manager_->autocomplete_history_manager_->SendSuggestions(&result);
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

  void FillAutofillFormData(int query_id,
                            const FormData& form,
                            const FormFieldData& field,
                            int unique_id) {
    autofill_manager_->FillOrPreviewForm(AutofillDriver::FORM_DATA_ACTION_FILL,
                                         query_id, form, field, unique_id);
  }

  // Calls |autofill_manager_->OnFillAutofillFormData()| with the specified
  // input parameters after setting up the expectation that the mock driver's
  // |SendFormDataToRenderer()| method will be called and saving the parameters
  // of that call into the |response_query_id| and |response_data| output
  // parameters.
  void FillAutofillFormDataAndSaveResults(int input_query_id,
                                          const FormData& input_form,
                                          const FormFieldData& input_field,
                                          int unique_id,
                                          int* response_query_id,
                                          FormData* response_data) {
    EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _))
        .WillOnce((DoAll(testing::SaveArg<0>(response_query_id),
                         testing::SaveArg<2>(response_data))));
    FillAutofillFormData(input_query_id, input_form, input_field, unique_id);
  }

  int MakeFrontendID(const std::string& cc_sid,
                     const std::string& profile_sid) const {
    return autofill_manager_->MakeFrontendID(cc_sid, profile_sid);
  }

  bool WillFillCreditCardNumber(const FormData& form,
                                const FormFieldData& field) {
    return autofill_manager_->WillFillCreditCardNumber(form, field);
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

  void PrepareForRealPanResponse(FormData* form, CreditCard* card) {
    // This line silences the warning from PaymentsClient about matching sync
    // and Payments server types.
    base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
        "sync-url", "https://google.com");

    CreateTestCreditCardFormData(form, true, false);
    FormsSeen(std::vector<FormData>(1, *form));
    *card = CreditCard(CreditCard::MASKED_SERVER_CARD, "a123");
    test::SetCreditCardInfo(card, "John Dillinger", "1881" /* Visa */, "01",
                            "2017", "1");
    card->SetNetworkForMaskedCard(kVisaCard);

    EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _))
        .Times(AtLeast(1));
    autofill_manager_->FillOrPreviewCreditCardForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, kDefaultPageID, *form,
        form->fields[0], *card);
  }

  // Convenience method for using and retrieving a mock autocomplete history
  // manager.
  MockAutocompleteHistoryManager* RecreateMockAutocompleteHistoryManager() {
    MockAutocompleteHistoryManager* manager =
        new MockAutocompleteHistoryManager(autofill_driver_.get(),
                                           autofill_manager_->client());
    autofill_manager_->autocomplete_history_manager_.reset(manager);
    return manager;
  }

  // Convenience method to cast the FullCardRequest into a CardUnmaskDelegate.
  CardUnmaskDelegate* full_card_unmask_delegate() {
    DCHECK(autofill_manager_->full_card_request_);
    return static_cast<CardUnmaskDelegate*>(
        autofill_manager_->full_card_request_.get());
  }

  void SetHttpWarningEnabled() {
    scoped_feature_list_.InitAndEnableFeature(
        security_state::kHttpFormWarningFeature);
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

  void DisableAutofillUpstreamUseAutofillProfileComparator() {
    scoped_feature_list_.InitAndDisableFeature(
        kAutofillUpstreamUseAutofillProfileComparator);
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
  std::unique_ptr<MockAutofillDriver> autofill_driver_;
  std::unique_ptr<TestAutofillManager> autofill_manager_;
  std::unique_ptr<TestAutofillExternalDelegate> external_delegate_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  TestAutofillDownloadManager* download_manager_;
  TestPersonalDataManager personal_data_;
  base::test::ScopedFeatureList scoped_feature_list_;
  variations::testing::VariationParamsManager variation_params_;

 private:
  int ToHistogramSample(AutofillMetrics::CardUploadDecisionMetric metric) {
    for (int sample = 0; sample < metric + 1; ++sample)
      if (metric & (1 << sample))
        return sample;

    NOTREACHED();
    return 0;
  }
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
      form_field->set_server_type(server_types[i]);
    }

    UpdateAutofillCount();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFormStructure);
};

// Test that calling OnFormsSeen with an empty set of forms (such as when
// reloading a page or when the renderer processes a set of forms but detects
// no changes) does not load the forms again.
TEST_F(AutofillManagerTest, OnFormsSeen_Empty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);

  base::HistogramTester histogram_tester;
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);

  // No more forms, metric is not logged.
  forms.clear();
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);
}

// Test that calling OnFormsSeen consecutively with a different set of forms
// will query for each separately.
TEST_F(AutofillManagerTest, OnFormsSeen_DifferentFormStructures) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);

  base::HistogramTester histogram_tester;
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 1);
  download_manager_->VerifyLastQueriedForms(forms);

  // Different form structure.
  FormData form2;
  form2.name = ASCIIToUTF16("MyForm");
  form2.origin = GURL("https://myform.com/form.html");
  form2.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form2.fields.push_back(field);

  forms.clear();
  forms.push_back(form2);
  FormsSeen(forms);
  histogram_tester.ExpectUniqueSample("Autofill.UserHappiness",
                                      0 /* FORMS_LOADED */, 2);
  download_manager_->VerifyLastQueriedForms(forms);
}

// Test that when forms are seen, the renderer is updated with the predicted
// field types
TEST_F(AutofillManagerTest, OnFormsSeen_SendAutofillTypePredictionsToRenderer) {
  // Set up a queryable form.
  FormData form1;
  test::CreateTestAddressFormData(&form1);

  // Set up a non-queryable form.
  FormData form2;
  FormFieldData field;
  test::CreateTestFormField("Querty", "qwerty", "", "text", &field);
  form2.name = ASCIIToUTF16("NonQueryable");
  form2.origin = form1.origin;
  form2.action = GURL("https://myform.com/submit.html");
  form2.fields.push_back(field);

  // Package the forms for observation.
  std::vector<FormData> forms{form1, form2};

  // Setup expectations.
  EXPECT_CALL(*autofill_driver_, SendAutofillTypePredictionsToRenderer(_))
      .Times(2);
  FormsSeen(forms);
}

// Test that no autofill suggestions are returned for a field with an
// unrecognized autocomplete attribute.
TEST_F(AutofillManagerTest, GetProfileSuggestions_UnrecognizedAttribute) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  // Set no autocomplete attribute for the middle name.
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Ensure that autocomplete manager is not called for suggestions either.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  // Suggestions should be returned for the first two fields.
  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckSuggestionCount(kDefaultPageID, 2);
  GetAutofillSuggestions(form, form.fields[1]);
  external_delegate_->CheckSuggestionCount(kDefaultPageID, 2);

  // No suggestions should not be provided for the third field because of its
  // unrecognized autocomplete attribute.
  GetAutofillSuggestions(form, form.fields[2]);
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

// Test that no suggestions are returned when there are less than three fields
// and none of them have an autocomplete attribute.
TEST_F(AutofillManagerTest, GetProfileSuggestions_SmallFormNoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Ensure that autocomplete manager is called for both fields.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(2);

  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());

  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that for form with two fields with one that has an autocomplete
// attribute, suggestions are only made for the one that has the attribute.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_SmallFormWithOneAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Check that suggestions are made for the field that has the autocomplete
  // attribute.
  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckSuggestions(kDefaultPageID,
                                       Suggestion("Charles", "", "", 1),
                                       Suggestion("Elvis", "", "", 2));

  // Check that there are no suggestions for the field without the autocomplete
  // attribute.
  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that for a form with two fields with autocomplete attributes,
// suggestions are made for both fields.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_SmallFormWithTwoAutocomplete) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, form.fields[0]);
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "Charles Hardin Holley", "", 1),
      Suggestion("Elvis", "Elvis Aaron Presley", "", 2));

  GetAutofillSuggestions(form, form.fields[1]);
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Holley", "Charles Hardin Holley", "", 1),
      Suggestion("Presley", "Elvis Aaron Presley", "", 2));
}

// Test that we return all address profile suggestions when all form fields are
// empty.
TEST_F(AutofillManagerTest, GetProfileSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate. Inferred
  // labels include full first relevant field, which in this case is the
  // address line 1.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2));
}

// Test that the HttpWarning does not appear on non-payment forms.
TEST_F(AutofillManagerTest, GetProfileSuggestions_EmptyValueNotSecure) {
  SetHttpWarningEnabled();
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2));
}

// Test that we return only matching address profile suggestions when the
// selected form field has been partially filled out.
TEST_F(AutofillManagerTest, GetProfileSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text", &field);
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 1));
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that we merge identical values.
TEST_F(AutofillManagerTest,
       GetProfileSuggestions_AlreadyAutofilledMergeValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  form.fields[0].is_autofilled = true;

  // Two profiles have the same last name, and the third shares the same first
  // letter for last name.
  std::unique_ptr<AutofillProfile> profile1 =
      base::MakeUnique<AutofillProfile>();
  profile1->set_guid("00000000-0000-0000-0000-000000000103");
  profile1->SetInfo(NAME_FIRST, ASCIIToUTF16("Robin"), "en-US");
  profile1->SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile1->SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                    "en-US");
  autofill_manager_->AddProfile(std::move(profile1));

  std::unique_ptr<AutofillProfile> profile2 =
      base::MakeUnique<AutofillProfile>();
  profile2->set_guid("00000000-0000-0000-0000-000000000124");
  profile2->SetInfo(NAME_FIRST, ASCIIToUTF16("Carl"), "en-US");
  profile2->SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile2->SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                    "en-US");
  autofill_manager_->AddProfile(std::move(profile2));

  std::unique_ptr<AutofillProfile> profile3 =
      base::MakeUnique<AutofillProfile>();
  profile3->set_guid("00000000-0000-0000-0000-000000000126");
  profile3->SetInfo(NAME_FIRST, ASCIIToUTF16("Aaron"), "en-US");
  profile3->SetInfo(NAME_LAST, ASCIIToUTF16("Googler"), "en-US");
  profile3->SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1600 Amphitheater pkwy"),
                    "en-US");
  autofill_manager_->AddProfile(std::move(profile3));

  FormFieldData field;
  test::CreateTestFormField("Last Name", "lastname", "G", "text", &field);
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate. No labels,
  // with duplicate values "Grimes" merged.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Googler", "" /* no label */, "", 1),
      Suggestion("Grimes", "" /* no label */, "", 2));
}

// Tests that we return address profile suggestions values when the section
// is already autofilled, and that they have no label.
TEST_F(AutofillManagerTest, GetProfileSuggestions_AlreadyAutofilledNoLabels) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First name is already autofilled which will make the section appear as
  // "already autofilled".
  form.fields[0].is_autofilled = true;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text", &field);
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate. No labels.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Elvis", "" /* no label */, "", 1));
}

// Test that we return no suggestions when the form has no relevant fields.
TEST_F(AutofillManagerTest, GetProfileSuggestions_UnknownFields) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Username", "username", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Password", "password", "", "password", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Quest", "quest", "", "quest", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Color", "color", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we cull duplicate profile suggestions.
TEST_F(AutofillManagerTest, GetProfileSuggestions_WithDuplicates) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Add a duplicate profile.
  std::unique_ptr<AutofillProfile> duplicate_profile =
      base::MakeUnique<AutofillProfile>(*(autofill_manager_->GetProfileWithGUID(
          "00000000-0000-0000-0000-000000000001")));
  autofill_manager_->AddProfile(std::move(duplicate_profile));

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2));
}

// Test that we return no suggestions when autofill is disabled.
TEST_F(AutofillManagerTest, GetProfileSuggestions_AutofillDisabledByUser) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Disable Autofill.
  autofill_manager_->set_autofill_enabled(false);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return all credit card profile suggestions when all form fields
// are empty.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_EmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has whitespace in it.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_Whitespace) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = ASCIIToUTF16("       ");
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it, which should be removed.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_StopCharsOnly) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  field.value = ASCIIToUTF16("____-____-____-____");
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card profile suggestions when the triggering
// field has stop characters in it and some input.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_StopCharsWithInput) {
  // Add a credit card with particular numbers that we will attempt to recall.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Smith",
                          "5255667890123123",  // Mastercard
                          "08", "2017", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  autofill_manager_->AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];

  field.value = ASCIIToUTF16("5255-66__-____-____");
  GetAutofillSuggestions(form, field);

  // Test that we sent the right value to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "3123",
                 "08/17", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that we return only matching credit card profile suggestions when the
// selected form field has been partially filled out.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_MatchCharacter) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "78", "text", &field);
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that we return credit card profile suggestions when the selected form
// field is not the credit card number field.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_NonCCNumber) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID)
  static const std::string kVisaSuggestion =
      std::string("Visa") + kUTF8MidlineEllipsis + "3456";
  static const std::string kMcSuggestion =
      std::string("Mastercard") + kUTF8MidlineEllipsis + "8765";
#else
  static const std::string kVisaSuggestion = "*3456";
  static const std::string kMcSuggestion = "*8765";
#endif

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion("Elvis Presley", kVisaSuggestion, kVisaCard,
                 autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion("Buddy Holly", kMcSuggestion, kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return a warning explaining that credit card profile suggestions
// are unavailable when the page and the form target URL are not secure.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_NonSecureContext) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, /* is_https */ false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion(l10n_util::GetStringUTF8(
                                     IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
                                 "", "", -1));

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  // Autocomplete suggestions are queried, but not Autofill.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return an extra "Payment not secure" warning when the page and
// the form target URL are not secure.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_NonSecureContextWithHttpBadSwitchOn) {
  SetHttpWarningEnabled();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, /* is_https */ false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(l10n_util::GetStringUTF8(
                     IDS_AUTOFILL_CREDIT_CARD_HTTP_WARNING_MESSAGE),
                 l10n_util::GetStringUTF8(IDS_AUTOFILL_HTTP_WARNING_LEARN_MORE),
                 "httpWarning", POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE),
#if !defined(OS_ANDROID)
      Suggestion("", "", "", POPUP_ITEM_ID_SEPARATOR),
#endif
      Suggestion(
          l10n_util::GetStringUTF8(IDS_AUTOFILL_WARNING_PAYMENT_DISABLED), "",
          "", POPUP_ITEM_ID_INSECURE_CONTEXT_PAYMENT_DISABLED_MESSAGE));

  // Clear the test credit cards and try again -- we should still show the
  // warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(l10n_util::GetStringUTF8(
                     IDS_AUTOFILL_CREDIT_CARD_HTTP_WARNING_MESSAGE),
                 l10n_util::GetStringUTF8(IDS_AUTOFILL_HTTP_WARNING_LEARN_MORE),
                 "httpWarning", POPUP_ITEM_ID_HTTP_NOT_SECURE_WARNING_MESSAGE));
}

// Test that we don't show the extra "Payment not secure" warning when the page
// and the form target URL are secure, even when the switch is on.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContextWithHttpBadSwitchOn) {
  SetHttpWarningEnabled();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, /* is_https */ true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we will eventually return the credit card signin promo when there
// are no credit card suggestions and the promo is active. See the tests in
// AutofillExternalDelegateTest that test whether the promo is added.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_OnlySigninPromo) {
  personal_data_.ClearCreditCards();

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[1];

  ON_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillByDefault(Return(true));
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo()).Times(2);
  EXPECT_TRUE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Autocomplete suggestions are not queried.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  GetAutofillSuggestions(form, field);

  // Test that we sent no values to the external delegate. It will add the promo
  // before passing along the results.
  external_delegate_->CheckNoSuggestions(kDefaultPageID);

  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return a warning explaining that credit card profile suggestions
// are unavailable when the page is secure, but the form action URL is valid but
// not secure.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_FormActionNotHTTPS) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, /* is_https= */ true, false);
  // However we set the action (target URL) to be HTTP after all.
  form.action = GURL("http://myform.com/submit.html");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion(l10n_util::GetStringUTF8(
                                     IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
                                 "", "", -1));

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  // Autocomplete suggestions are queried, but not Autofill.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return credit card suggestions for secure pages that have an
// empty form action target URL.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_EmptyFormAction) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Clear the form action.
  form.action = GURL();
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return credit card suggestions for secure pages that have a
// form action set to "javascript:something".
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_SecureContext_JavascriptFormAction) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  // Have the form action be a javascript function (which is a valid URL).
  form.action = GURL("javascript:alert('Hello');");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that we return all credit card suggestions in the case that two cards
// have the same obfuscated number.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_RepeatedObfuscatedNumber) {
  // Add a credit card with the same obfuscated number as Elvis's.
  // |credit_card| will be owned by the mock PersonalDataManager.
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley",
                          "5231567890123456",  // Mastercard
                          "05", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000007");
  credit_card.set_use_date(AutofillClock::Now() -
                           base::TimeDelta::FromDays(15));
  autofill_manager_->AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "3456",
                 "05/99", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(7)));
}

// Test that we return profile and credit card suggestions for combined forms.
TEST_F(AutofillManagerTest, GetAddressAndCreditCardSuggestions) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right address suggestions to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2));

  const int kPageID2 = 2;
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  GetAutofillSuggestions(kPageID2, form, field);

  // Test that we sent the credit card suggestions to the external delegate.
  external_delegate_->CheckSuggestions(
      kPageID2,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)),
      Suggestion(std::string("Mastercard") + kUTF8MidlineEllipsis + "8765",
                 "10/98", kMasterCard,
                 autofill_manager_->GetPackedCreditCardID(5)));
}

// Test that for non-https forms with both address and credit card fields, we
// only return address suggestions. Instead of credit card suggestions, we
// should return a warning explaining that credit card profile suggestions are
// unavailable when the form is not https.
TEST_F(AutofillManagerTest, GetAddressAndCreditCardSuggestionsNonHttps) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right suggestions to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2));

  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(
      kPageID2, Suggestion(l10n_util::GetStringUTF8(
                               IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
                           "", "", -1));

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  external_delegate_->CheckNoSuggestions(kDefaultPageID);
}

// Test that we return autocomplete-like suggestions when trying to autofill
// already filled forms.
TEST_F(AutofillManagerTest, GetFieldSuggestionsWhenFormIsAutofilled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Mark one of the fields as filled.
  form.fields[2].is_autofilled = true;
  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(kDefaultPageID,
                                       Suggestion("Charles", "", "", 1),
                                       Suggestion("Elvis", "", "", 2));
}

// Test that nothing breaks when there are autocomplete suggestions but no
// autofill suggestions.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForAutocompleteOnly) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field;
  test::CreateTestFormField("Some Field", "somefield", "", "text", &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<base::string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("one"));
  suggestions.push_back(ASCIIToUTF16("two"));
  AutocompleteSuggestionsReturned(suggestions);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(kDefaultPageID,
                                       Suggestion("one", "", "", 0),
                                       Suggestion("two", "", "", 0));
}

// Test that we do not return duplicate values drawn from multiple profiles when
// filling an already filled field.
TEST_F(AutofillManagerTest, GetFieldSuggestionsWithDuplicateValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  std::unique_ptr<AutofillProfile> profile =
      base::MakeUnique<AutofillProfile>();
  test::SetProfileInfo(profile.get(), "Elvis", "", "", "", "", "", "", "", "",
                       "", "", "");
  profile->set_guid("00000000-0000-0000-0000-000000000101");
  autofill_manager_->AddProfile(std::move(profile));

  FormFieldData& field = form.fields[0];
  field.is_autofilled = true;
  field.value = ASCIIToUTF16("Elvis");
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate.
  external_delegate_->CheckSuggestions(kDefaultPageID,
                                       Suggestion("Elvis", "", "", 1));
}

TEST_F(AutofillManagerTest, GetProfileSuggestions_FancyPhone) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::unique_ptr<AutofillProfile> profile =
      base::MakeUnique<AutofillProfile>();
  profile->set_guid("00000000-0000-0000-0000-000000000103");
  profile->SetInfo(NAME_FULL, ASCIIToUTF16("Natty Bumppo"), "en-US");
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1800PRAIRIE"));
  autofill_manager_->AddProfile(std::move(profile));

  const FormFieldData& field = form.fields[9];
  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate. Inferred
  // labels include the most private field of those that would be filled.
  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion("18007724743", "Natty Bumppo", "", 1),  // 1800PRAIRIE
      Suggestion("23456789012", "123 Apple St.", "", 2),
      Suggestion("12345678901", "3734 Elvis Presley Blvd.", "", 3));
}

TEST_F(AutofillManagerTest, GetProfileSuggestions_ForPhonePrefixOrSuffix) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  struct {
    const char* const label;
    const char* const name;
    size_t max_length;
    const char* const autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"},
                     {"Phone Extension", "ext", 5, "tel-extension"}};

  FormFieldData field;
  for (const auto& test_field : test_fields) {
    test::CreateTestFormField(test_field.label, test_field.name, "", "text",
                              &field);
    field.max_length = test_field.max_length;
    field.autocomplete_attribute = std::string();
    form.fields.push_back(field);
  }

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::unique_ptr<AutofillProfile> profile =
      base::MakeUnique<AutofillProfile>();
  profile->set_guid("00000000-0000-0000-0000-000000000104");
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER, ASCIIToUTF16("1800FLOWERS"));
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->AddProfile(std::move(profile));

  const FormFieldData& phone_prefix = form.fields[2];
  GetAutofillSuggestions(form, phone_prefix);

  // Test that we sent the right prefix values to the external delegate.
  external_delegate_->CheckSuggestions(kDefaultPageID,
                                       Suggestion("356", "1800FLOWERS", "", 1));

  const FormFieldData& phone_suffix = form.fields[3];
  GetAutofillSuggestions(form, phone_suffix);

  // Test that we sent the right suffix values to the external delegate.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("9377", "1800FLOWERS", "", 1));
}

// Test that we correctly fill an address form.
TEST_F(AutofillManagerTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  AutofillProfile* profile = autofill_manager_->GetProfileWithGUID(guid);
  ASSERT_TRUE(profile);
  EXPECT_EQ(1U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());

  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  EXPECT_EQ(2U, profile->use_count());
  EXPECT_NE(base::Time(), profile->use_date());
}

TEST_F(AutofillManagerTest, WillFillCreditCardNumber) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData* number_field = nullptr;
  FormFieldData* name_field = nullptr;
  FormFieldData* month_field = nullptr;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    if (form.fields[i].name == ASCIIToUTF16("cardnumber"))
      number_field = &form.fields[i];
    else if (form.fields[i].name == ASCIIToUTF16("nameoncard"))
      name_field = &form.fields[i];
    else if (form.fields[i].name == ASCIIToUTF16("ccmonth"))
      month_field = &form.fields[i];
  }

  // Empty form - whole form is Autofilled.
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));

  // If the user has entered a value, it won't be overridden.
  number_field->value = ASCIIToUTF16("gibberish");
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));

  // But if that value is removed, it will be Autofilled.
  number_field->value.clear();
  EXPECT_TRUE(WillFillCreditCardNumber(form, *name_field));

  // When part of the section is Autofilled, only fill the initiating field.
  month_field->is_autofilled = true;
  EXPECT_FALSE(WillFillCreditCardNumber(form, *name_field));
  EXPECT_TRUE(WillFillCreditCardNumber(form, *number_field));
}

// Test that we correctly fill a credit card form.
TEST_F(AutofillManagerTest, FillCreditCardForm_Simple) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that whitespace is stripped from the credit card number.
TEST_F(AutofillManagerTest, FillCreditCardForm_StripCardNumberWhitespace) {
  // Same as the SetUp(), but generate Elvis card with whitespace in credit
  // card number.
  personal_data_.CreateTestCreditCardWithWhitespace();
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000008";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that separator characters are stripped from the credit card number.
TEST_F(AutofillManagerTest, FillCreditCardForm_StripCardNumberSeparators) {
  // Same as the SetUp(), but generate Elvis card with separator characters in
  // credit card number.
  personal_data_.CreateTestCreditCardWithSeparators();
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that we correctly fill a credit card form with month input type.
// 1. year empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardForm_NoYearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("", "");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
                                               kDefaultPageID, false, "", "");
}

// Test that we correctly fill a credit card form with month input type.
// 2. year empty, month non-empty
TEST_F(AutofillManagerTest, FillCreditCardForm_NoYearMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("", "04");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
                                               kDefaultPageID, false, "", "04");
}

// Test that we correctly fill a credit card form with month input type.
// 3. year non-empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardForm_YearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("2999", "");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(
      response_page_id, response_data, kDefaultPageID, false, "2999", "");
}

// Test that we correctly fill a credit card form with month input type.
// 4. year non-empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardForm_YearMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("2999", "04");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000007";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(
      response_page_id, response_data, kDefaultPageID, false, "2999", "04");
}

// Test that we correctly fill a credit card form with first and last cardholder
// name.
TEST_F(AutofillManagerTest, FillCreditCardForm_SplitName) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Card Name", "cardname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last Name", "cardlastname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("CVC", "cvc", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);
  ExpectFilledField("Card Name", "cardname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Last Name", "cardlastname", "Presley", "text",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    response_data.fields[2]);
}

// Test that we correctly fill a combined address and credit card form.
TEST_F(AutofillManagerTest, FillAddressAndCreditCardForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  {
    SCOPED_TRACE("Address");
    FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                       MakeFrontendID(std::string(), guid),
                                       &response_page_id, &response_data);
    ExpectFilledAddressFormElvis(response_page_id, response_data,
                                 kDefaultPageID, true);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  {
    FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
                                       MakeFrontendID(guid2, std::string()),
                                       &response_page_id, &response_data);
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }
}

// Test that a field with an unrecognized autocomplete attribute is not filled.
TEST_F(AutofillManagerTest, FillAddressForm_UnrecognizedAttribute) {
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.origin = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set a valid autocomplete attribute for the first name.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  address_form.fields.push_back(field);
  // Set no autocomplete attribute for the middle name.
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.autocomplete_attribute = "";
  address_form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute for the last name.
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // The fist and middle names should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);

  // The last name should not be filled.
  ExpectFilledField("Last name", "lastname", "", "text",
                    response_data.fields[2]);
}

// Test that non credit card related fields with the autocomplete attribute set
// to off are not filled on desktop.
TEST_F(AutofillManagerTest, FillAddressForm_AutocompleteOff) {
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.origin = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.should_autocomplete = false;
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  address_form.fields.push_back(field);
  test::CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  field.should_autocomplete = false;
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // The fist name should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);

  // The middle name should not be filled on desktop.
  if (IsDesktopPlatform()) {
    ExpectFilledField("Middle name", "middle", "", "text",
                      response_data.fields[1]);
  } else {
    ExpectFilledField("Middle name", "middle", "Aaron", "text",
                      response_data.fields[1]);
  }

  // The last name should be filled.
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);

  // The address line 1 should not be filled on desktop.
  if (IsDesktopPlatform()) {
    ExpectFilledField("Address Line 1", "addr1", "", "text",
                      response_data.fields[3]);
  } else {
    ExpectFilledField("Address Line 1", "addr1", "3734 Elvis Presley Blvd.",
                      "text", response_data.fields[3]);
  }
}

// Test that a field with a value equal to it's placeholder attribute is filled.
TEST_F(AutofillManagerTest, FillAddressForm_PlaceholderEqualsValue) {
  FormData address_form;
  address_form.name = ASCIIToUTF16("MyForm");
  address_form.origin = GURL("https://myform.com/form.html");
  address_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  // Set the same placeholder and value for each field.
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.placeholder = ASCIIToUTF16("First Name");
  field.value = ASCIIToUTF16("First Name");
  address_form.fields.push_back(field);
  test::CreateTestFormField("Middle name", "middle", "", "text", &field);
  field.placeholder = ASCIIToUTF16("Middle Name");
  field.value = ASCIIToUTF16("Middle Name");
  address_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.placeholder = ASCIIToUTF16("Last Name");
  field.value = ASCIIToUTF16("Last Name");
  address_form.fields.push_back(field);
  std::vector<FormData> address_forms(1, address_form);
  FormsSeen(address_forms);

  // Fill the address form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(
      kDefaultPageID, address_form, address_form.fields[0],
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data);

  // All the fields should be filled.
  ExpectFilledField("First name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("Middle name", "middle", "Aaron", "text",
                    response_data.fields[1]);
  ExpectFilledField("Last name", "lastname", "Presley", "text",
                    response_data.fields[2]);
}

// Test that a credit card field with an unrecognized autocomplete attribute
// gets filled.
TEST_F(AutofillManagerTest, FillCreditCardForm_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  // Set a valid autocomplete attribute on the card name.
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.autocomplete_attribute = "cc-name";
  form.fields.push_back(field);
  // Set no autocomplete attribute on the card number.
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute on the expiration month.
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);

  // The credit card name and number should be filled.
  ExpectFilledField("Name on Card", "nameoncard", "Elvis Presley", "text",
                    response_data.fields[0]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890123456", "text",
                    response_data.fields[1]);

  // The expiration month should be filled.
  ExpectFilledField("Expiration Date", "ccmonth", "04/2999", "text",
                    response_data.fields[2]);
}

// Test that credit card fields are filled even if they have the autocomplete
// attribute set to off.
TEST_F(AutofillManagerTest, FillCreditCardForm_AutocompleteOff) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);

  // Set the autocomplete=off on all fields.
  for (FormFieldData field : form.fields)
    field.should_autocomplete = false;

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000004";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);

  // All fields should be filled.
  ExpectFilledCreditCardFormElvis(response_page_id, response_data,
                                  kDefaultPageID, false);
}

// Test that selecting an expired credit card fills everything except the
// expiration date.
TEST_F(AutofillManagerTest, FillCreditCardForm_ExpiredCard) {
  personal_data_.CreateTestExpiredCreditCard();

  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  // Create a credit card form.
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.autocomplete_attribute = "cc-name";
  form.fields.push_back(field);
  std::vector<const char*> kCreditCardTypes = {"Visa", "Mastercard", "AmEx",
                                               "discover"};
  test::CreateTestSelectField("Card Type", "cardtype", "", kCreditCardTypes,
                              kCreditCardTypes, 4, &field);
  field.autocomplete_attribute = "cc-type";
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.autocomplete_attribute = "cc-number";
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccmonth", "", "text", &field);
  field.autocomplete_attribute = "cc-exp-month";
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Year", "ccyear", "", "text", &field);
  field.autocomplete_attribute = "cc-exp-year";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const char guid[] = "00000000-0000-0000-0000-000000000009";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(guid, std::string()),
                                     &response_page_id, &response_data);

  // The credit card name, type and number should be filled.
  ExpectFilledField("Name on Card", "nameoncard", "Homer Simpson", "text",
                    response_data.fields[0]);
  ExpectFilledField("Card Type", "cardtype", "Visa", "select-one",
                    response_data.fields[1]);
  ExpectFilledField("Card Number", "cardnumber", "4234567890654321", "text",
                    response_data.fields[2]);

  // The expiration month and year should not be filled.
  ExpectFilledField("Expiration Month", "ccmonth", "", "text",
                    response_data.fields[3]);
  ExpectFilledField("Expiration Year", "ccyear", "", "text",
                    response_data.fields[4]);
}

// Test that non-focusable field is ignored while inferring boundaries between
// sections: http://crbug.com/231160
TEST_F(AutofillManagerTest, FillFormWithNonFocusableFields) {
  // Create a form with both focusable and non-focusable fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "lastname", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email", "", "text", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("Phone Number", "phonenumber", "", "tel", &field);
  form.fields.push_back(field);

  test::CreateTestFormField("", "email_", "", "text", &field);
  field.is_focusable = false;
  form.fields.push_back(field);

  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  // The whole form should be filled as all the fields belong to the same
  // logical section.
  ASSERT_EQ(6U, response_data.fields.size());
  ExpectFilledField("First Name", "firstname", "Elvis", "text",
                    response_data.fields[0]);
  ExpectFilledField("", "lastname", "Presley", "text", response_data.fields[1]);
  ExpectFilledField("", "email", "theking@gmail.com", "text",
                    response_data.fields[2]);
  ExpectFilledField("Phone Number", "phonenumber", "12345678901", "tel",
                    response_data.fields[3]);
  ExpectFilledField("", "email_", "theking@gmail.com", "text",
                    response_data.fields[4]);
  ExpectFilledField("Country", "country", "United States", "text",
                    response_data.fields[5]);
}

// Test that we correctly fill a form that has multiple logical sections, e.g.
// both a billing and a shipping address.
TEST_F(AutofillManagerTest, FillFormWithMultipleSections) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  const size_t kAddressFormSize = form.fields.size();
  test::CreateTestAddressFormData(&form);
  for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
    // Make sure the fields have distinct names.
    form.fields[i].name = form.fields[i].name + ASCIIToUTF16("_");
  }
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the first section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 1");
    // The second address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(base::string16(), response_data.fields[i].value);
    }

    // The first address section should be filled with Elvis's data.
    response_data.fields.resize(kAddressFormSize);
    ExpectFilledAddressFormElvis(response_page_id, response_data,
                                 kDefaultPageID, false);
  }

  // Fill the second section, with the initiating field somewhere in the middle
  // of the section.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  ASSERT_LT(9U, kAddressFormSize);
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID2, form, form.fields[kAddressFormSize + 9],
      MakeFrontendID(std::string(), guid2), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 2");
    ASSERT_EQ(response_data.fields.size(), form.fields.size());

    // The first address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2 * kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      EXPECT_EQ(base::string16(), response_data.fields[i].value);
    }

    // The second address section should be filled with Elvis's data.
    FormData secondSection = response_data;
    secondSection.fields.erase(secondSection.fields.begin(),
                               secondSection.fields.begin() + kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      // Restore the expected field names.
      base::string16 name = secondSection.fields[i].name;
      base::string16 original_name = name.substr(0, name.size() - 1);
      secondSection.fields[i].name = original_name;
    }
    ExpectFilledAddressFormElvis(response_page_id, secondSection, kPageID2,
                                 false);
  }
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_F(AutofillManagerTest, FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;

  test::CreateTestFormField("", "country", "", "text", &field);
  field.autocomplete_attribute = "section-billing country";
  form.fields.push_back(field);

  test::CreateTestFormField("", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  test::CreateTestFormField("", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  test::CreateTestFormField("", "address", "", "text", &field);
  field.autocomplete_attribute = "section-billing address-line1";
  form.fields.push_back(field);

  test::CreateTestFormField("", "city", "", "text", &field);
  field.autocomplete_attribute = "section-billing locality";
  form.fields.push_back(field);

  test::CreateTestFormField("", "state", "", "text", &field);
  field.autocomplete_attribute = "section-billing region";
  form.fields.push_back(field);

  test::CreateTestFormField("", "zip", "", "text", &field);
  field.autocomplete_attribute = "section-billing postal-code";
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccname", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-name";
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccnumber", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-number";
  form.fields.push_back(field);

  test::CreateTestFormField("", "ccexp", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-exp";
  form.fields.push_back(field);

  test::CreateTestFormField("", "email", "", "text", &field);
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the unnamed section.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[1],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(kDefaultPageID, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "Elvis", "text",
                      response_data.fields[1]);
    ExpectFilledField("", "lastname", "Presley", "text",
                      response_data.fields[2]);
    ExpectFilledField("", "address", "", "text", response_data.fields[3]);
    ExpectFilledField("", "city", "", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "", "text", response_data.fields[5]);
    ExpectFilledField("", "zip", "", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "", "text", response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "theking@gmail.com", "text",
                      response_data.fields[10]);
  }

  // Fill the address portion of the billing section.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000001";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid2),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(kPageID2, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "US", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "", "text", response_data.fields[1]);
    ExpectFilledField("", "lastname", "", "text", response_data.fields[2]);
    ExpectFilledField("", "address", "3734 Elvis Presley Blvd.", "text",
                      response_data.fields[3]);
    ExpectFilledField("", "city", "Memphis", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "Tennessee", "text",
                      response_data.fields[5]);
    ExpectFilledField("", "zip", "38116", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "", "text", response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", response_data.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "", "text", response_data.fields[10]);
  }

  // Fill the credit card portion of the billing section.
  const int kPageID3 = 3;
  const char guid3[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID3, form, form.fields[form.fields.size() - 2],
      MakeFrontendID(guid3, std::string()), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(kPageID3, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "", "text", response_data.fields[0]);
    ExpectFilledField("", "firstname", "", "text", response_data.fields[1]);
    ExpectFilledField("", "lastname", "", "text", response_data.fields[2]);
    ExpectFilledField("", "address", "", "text", response_data.fields[3]);
    ExpectFilledField("", "city", "", "text", response_data.fields[4]);
    ExpectFilledField("", "state", "", "text", response_data.fields[5]);
    ExpectFilledField("", "zip", "", "text", response_data.fields[6]);
    ExpectFilledField("", "ccname", "Elvis Presley", "text",
                      response_data.fields[7]);
    ExpectFilledField("", "ccnumber", "4234567890123456", "text",
                      response_data.fields[8]);
    ExpectFilledField("", "ccexp", "04/2999", "text", response_data.fields[9]);
    ExpectFilledField("", "email", "", "text", response_data.fields[10]);
  }
}

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(AutofillManagerTest, FillFormWithMultipleEmails) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormFieldData field;
  test::CreateTestFormField("Confirm email", "email2", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  // The second email address should be filled.
  EXPECT_EQ(ASCIIToUTF16("theking@gmail.com"),
            response_data.fields.back().value);

  // The remainder of the form should be filled as usual.
  response_data.fields.pop_back();
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_F(AutofillManagerTest, FillAutofilledForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  // Mark one of the address fields as autofilled.
  form.fields[4].is_autofilled = true;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_page_id, response_data, kDefaultPageID, "Elvis",
                     "", "", "", "", "", "", "", "", "", "", "", "", "", "",
                     true, true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  const char guid2[] = "00000000-0000-0000-0000-000000000004";
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
                                     MakeFrontendID(guid2, std::string()),
                                     &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(response_page_id, response_data, kPageID2,
                                    true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (std::vector<FormFieldData>::iterator iter = form.fields.begin();
       iter != form.fields.end(); ++iter) {
    iter->is_autofilled = true;
  }

  const int kPageID3 = 3;
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID3, form, form.fields[form.fields.size() - 2],
      MakeFrontendID(guid2, std::string()), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(response_page_id, response_data, kPageID3, "", "", "", "",
                     "", "", "", "", "", "", "", "", "", "", "2999", true, true,
                     false);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(AutofillManagerTest, FillPhoneNumber) {
  // In one form, rely on the maxlength attribute to imply US phone number
  // parts. In the other form, rely on the autocompletetype attribute.
  FormData form_with_us_number_max_length;
  form_with_us_number_max_length.name = ASCIIToUTF16("MyMaxlengthPhoneForm");
  form_with_us_number_max_length.origin =
      GURL("http://myform.com/phone_form.html");
  form_with_us_number_max_length.action =
      GURL("http://myform.com/phone_submit.html");
  FormData form_with_autocompletetype = form_with_us_number_max_length;
  form_with_autocompletetype.name = ASCIIToUTF16("MyAutocompletetypePhoneForm");

  struct {
    const char* label;
    const char* name;
    size_t max_length;
    const char* autocomplete_attribute;
  } test_fields[] = {{"country code", "country_code", 1, "tel-country-code"},
                     {"area code", "area_code", 3, "tel-area-code"},
                     {"phone", "phone_prefix", 3, "tel-local-prefix"},
                     {"-", "phone_suffix", 4, "tel-local-suffix"},
                     {"Phone Extension", "ext", 3, "tel-extension"}};

  FormFieldData field;
  const size_t default_max_length = field.max_length;
  for (const auto& test_field : test_fields) {
    test::CreateTestFormField(test_field.label, test_field.name, "", "text",
                              &field);
    field.max_length = test_field.max_length;
    field.autocomplete_attribute = std::string();
    form_with_us_number_max_length.fields.push_back(field);

    field.max_length = default_max_length;
    field.autocomplete_attribute = test_field.autocomplete_attribute;
    form_with_autocompletetype.fields.push_back(field);
  }

  std::vector<FormData> forms;
  forms.push_back(form_with_us_number_max_length);
  forms.push_back(form_with_autocompletetype);
  FormsSeen(forms);

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile* work_profile = autofill_manager_->GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != NULL);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  std::string guid(work_profile->guid());
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data1;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data1);
  EXPECT_EQ(1, response_page_id);

  ASSERT_EQ(5U, response_data1.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), response_data1.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data1.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), response_data1.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), response_data1.fields[3].value);
  EXPECT_EQ(base::string16(), response_data1.fields[4].value);

  page_id = 2;
  response_page_id = 0;
  FormData response_data2;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
                                     *form_with_autocompletetype.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data2);
  EXPECT_EQ(2, response_page_id);

  ASSERT_EQ(5U, response_data2.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), response_data2.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data2.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), response_data2.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), response_data2.fields[3].value);
  EXPECT_EQ(base::string16(), response_data2.fields[4].value);

  // We should not be able to fill international numbers correctly in a form
  // containing fields with US max_length. However, the field should fill with
  // the number of digits equal to the max length specified, starting from the
  // right.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("GB"));
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("447700954321"));
  page_id = 3;
  response_page_id = 0;
  FormData response_data3;
  FillAutofillFormDataAndSaveResults(
      page_id, form_with_us_number_max_length,
      *form_with_us_number_max_length.fields.begin(),
      MakeFrontendID(std::string(), guid), &response_page_id, &response_data3);
  EXPECT_EQ(3, response_page_id);

  ASSERT_EQ(5U, response_data3.fields.size());
  EXPECT_EQ(ASCIIToUTF16("4"), response_data3.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("700"), response_data3.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("321"), response_data3.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4321"), response_data3.fields[3].value);
  EXPECT_EQ(base::string16(), response_data3.fields[4].value);

  page_id = 4;
  response_page_id = 0;
  FormData response_data4;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
                                     *form_with_autocompletetype.fields.begin(),
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data4);
  EXPECT_EQ(4, response_page_id);

  ASSERT_EQ(5U, response_data4.fields.size());
  EXPECT_EQ(ASCIIToUTF16("44"), response_data4.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("7700"), response_data4.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data4.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data4.fields[3].value);
  EXPECT_EQ(base::string16(), response_data4.fields[4].value);
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(AutofillManagerTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Add a field -- we'll remove it again later.
  FormFieldData field;
  test::CreateTestFormField("Some", "field", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(AutofillManagerTest, FormChangesAddField) {
  // The offset of the phone field in the address form.
  const int kPhoneFieldOffset = 9;

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Remove the phone field -- we'll add it back later.
  std::vector<FormFieldData>::iterator pos =
      form.fields.begin() + kPhoneFieldOffset;
  FormFieldData field = *pos;
  pos = form.fields.erase(pos);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we restore the field before filling.
  form.fields.insert(pos, field);

  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);
}

// Test that we are able to save form data when forms are submitted.
TEST_F(AutofillManagerTest, FormSubmitted) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

// Test that we are not saving form data when only the WillSubmitForm event is
// sent.
TEST_F(AutofillManagerTest, FormWillSubmitDoesNotSaveData) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate OnWillSubmitForm(). We should *not* be calling into the PDM at
  // this point (since the form was not submitted). Does not call
  // OnFormSubmitted.
  autofill_manager_->ResetRunLoop();
  autofill_manager_->OnWillSubmitForm(response_data, base::TimeTicks::Now());
  autofill_manager_->WaitForAsyncUploadProcess();
  EXPECT_EQ(0, personal_data_.num_times_save_imported_profile_called());
}

// Test that when Autocomplete is enabled and Autofill is disabled, form
// submissions are still received by AutocompleteHistoryManager.
TEST_F(AutofillManagerTest, FormSubmittedAutocompleteEnabled) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnWillSubmitForm(_));
  FormSubmitted(form);
}

// Test that when Autofill is disabled, Autocomplete suggestions are still
// queried.
TEST_F(AutofillManagerTest, AutocompleteSuggestions_SomeWhenAutofillDisabled) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];

  // Expect Autocomplete manager to be called for suggestions.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _));

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field should not autocomplete,
// autocomplete is not queried for suggestions.
TEST_F(AutofillManagerTest,
       AutocompleteSuggestions_AutofillDisabledAndFieldShouldNotAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData field = form.fields[0];
  field.should_autocomplete = false;

  // Autocomplete manager is not called for suggestions.

  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  GetAutofillSuggestions(form, field);
}

// Test that we do not query for Autocomplete suggestions when there are
// Autofill suggestions available.
TEST_F(AutofillManagerTest, AutocompleteSuggestions_NoneWhenAutofillPresent) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];

  // Autocomplete manager is not called for suggestions.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  GetAutofillSuggestions(form, field);

  // Test that we sent the right values to the external delegate. Inferred
  // labels include full first relevant field, which in this case is the
  // address line 1.
  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Charles", "123 Apple St.", "", 1),
      Suggestion("Elvis", "3734 Elvis Presley Blvd.", "", 2));
}

// Test that we query for Autocomplete suggestions when there are no Autofill
// suggestions available.
TEST_F(AutofillManagerTest, AutocompleteSuggestions_SomeWhenAutofillEmpty) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // No suggestions matching "donkey".
  FormFieldData field;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Autocomplete manager is called for suggestions because Autofill is empty.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _));

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card name
// field,
// autocomplete is queried for suggestions.
TEST_F(AutofillManagerTest,
       AutocompleteSuggestions_CreditCardNameFieldShouldAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  // The first field is "Name on card", which should autocomplete.
  FormFieldData field = form.fields[0];
  field.should_autocomplete = true;

  // Autocomplete manager is not called for suggestions.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _));

  GetAutofillSuggestions(form, field);
}

// Test that when Autofill is disabled and the field is a credit card number
// field, autocomplete is not queried for suggestions.
TEST_F(AutofillManagerTest,
       AutocompleteSuggestions_CreditCardNumberShouldNotAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  // The second field is "Card Number", which should not autocomplete.
  FormFieldData field = form.fields[1];
  field.should_autocomplete = true;

  // Autocomplete manager is not called for suggestions.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  GetAutofillSuggestions(form, field);
}

// Test that we do not query for Autocomplete suggestions when there are no
// Autofill suggestions available, and that the field should not autocomplete.
TEST_F(
    AutofillManagerTest,
    AutocompleteSuggestions_NoneWhenAutofillEmptyFieldShouldNotAutocomplete) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // No suggestions matching "donkey".
  FormFieldData field;
  field.should_autocomplete = false;
  test::CreateTestFormField("Email", "email", "donkey", "email", &field);

  // Autocomplete manager is not called for suggestions.
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  GetAutofillSuggestions(form, field);
}

TEST_F(AutofillManagerTest, AutocompleteOffRespectedForAutocomplete) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->SetExternalDelegate(external_delegate_.get());

  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnGetAutocompleteSuggestions(_, _, _, _)).Times(0);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormFieldData* field = &form.fields[0];
  field->should_autocomplete = false;
  GetAutofillSuggestions(form, *field);
}

// Test that OnLoadedServerPredictions can obtain the FormStructure with the
// signature of the queried form and apply type predictions.
TEST_F(AutofillManagerTest, OnLoadedServerPredictions) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes(nullptr /* ukm_recorder */);
  autofill_manager_->AddSeenForm(base::WrapUnique(form_structure));

  // Similarly, a second form.
  FormData form2;
  form2.name = ASCIIToUTF16("MyForm");
  form2.origin = GURL("http://myform.com/form.html");
  form2.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form2.fields.push_back(field);

  test::CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form2.fields.push_back(field);

  test::CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form2.fields.push_back(field);

  TestFormStructure* form_structure2 = new TestFormStructure(form2);
  form_structure2->DetermineHeuristicTypes(nullptr /* ukm_recorder */);
  autofill_manager_->AddSeenForm(base::WrapUnique(form_structure2));

  AutofillQueryResponseContents response;
  response.add_field()->set_autofill_type(3);
  for (int i = 0; i < 7; ++i) {
    response.add_field()->set_autofill_type(0);
  }
  response.add_field()->set_autofill_type(3);
  response.add_field()->set_autofill_type(2);
  response.add_field()->set_autofill_type(61);
  response.add_field()->set_autofill_type(5);
  response.add_field()->set_autofill_type(4);
  response.add_field()->set_autofill_type(35);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::vector<std::string> signatures;
  signatures.push_back(form_structure->FormSignatureAsStr());
  signatures.push_back(form_structure2->FormSignatureAsStr());

  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(response_string, signatures);
  // Verify that FormStructure::ParseQueryResponse was called (here and below).
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_RECEIVED,
                                     1);
  histogram_tester.ExpectBucketCount("Autofill.ServerQueryResponse",
                                     AutofillMetrics::QUERY_RESPONSE_PARSED, 1);
  // We expect the server type to have been applied to the first field of the
  // first form.
  EXPECT_EQ(NAME_FIRST, form_structure->field(0)->Type().GetStorableType());

  // We expect the server types to have been applied to the second form.
  EXPECT_EQ(NAME_LAST, form_structure2->field(0)->Type().GetStorableType());
  EXPECT_EQ(NAME_MIDDLE, form_structure2->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_ZIP,
            form_structure2->field(2)->Type().GetStorableType());
}

// Test that OnLoadedServerPredictions does not call ParseQueryResponse if the
// AutofillManager has been reset between the time the query was sent and the
// response received.
TEST_F(AutofillManagerTest, OnLoadedServerPredictions_ResetManager) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes(nullptr /* ukm_recorder */);
  autofill_manager_->AddSeenForm(base::WrapUnique(form_structure));

  AutofillQueryResponseContents response;
  response.add_field()->set_autofill_type(3);
  for (int i = 0; i < 7; ++i) {
    response.add_field()->set_autofill_type(0);
  }
  response.add_field()->set_autofill_type(3);
  response.add_field()->set_autofill_type(2);
  response.add_field()->set_autofill_type(61);

  std::string response_string;
  ASSERT_TRUE(response.SerializeToString(&response_string));

  std::vector<std::string> signatures;
  signatures.push_back(form_structure->FormSignatureAsStr());

  // Reset the manager (such as during a navigation).
  autofill_manager_->Reset();

  base::HistogramTester histogram_tester;
  autofill_manager_->OnLoadedServerPredictions(response_string, signatures);

  // Verify that FormStructure::ParseQueryResponse was NOT called.
  histogram_tester.ExpectTotalCount("Autofill.ServerQueryResponse", 0);
}

// Test that we are able to save form data when forms are submitted and we only
// have server data for the field types.
TEST_F(AutofillManagerTest, FormSubmittedServerTypes) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  FormsSeen(std::vector<FormData>(1, form));

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  form_structure->DetermineHeuristicTypes(nullptr /* ukm_recorder */);

  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure->field(i)->heuristic_type());
  }
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenForm(base::WrapUnique(form_structure));

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

// Test that we are able to save form data after the possible types have been
// determined. We do two submissions and verify that only at the second
// submission are the possible types able to be inferred.
TEST_F(AutofillManagerTest, FormSubmittedPossibleTypesTwoSubmissions) {
  // Set up our form data.
  FormData form;
  std::vector<ServerFieldTypeSet> expected_types;
  test::CreateTestAddressFormData(&form, &expected_types);
  FormsSeen(std::vector<FormData>(1, form));

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(response_page_id, response_data, kDefaultPageID,
                               false);

  personal_data_.ClearAutofillProfiles();
  // The default credit card is a Elvis card. It must be removed because name
  // fields would be detected. However at least one profile or card is needed to
  // start the upload process, which is why this other card is created.
  personal_data_.ClearCreditCards();
  personal_data_.CreateTestCreditCardsYearAndMonth("2999", "04");
  ASSERT_EQ(0u, personal_data_.GetProfiles().size());

  // Simulate form submission. The first submission should not count the data
  // towards possible types. Therefore we expect all UNKNOWN_TYPE entries.
  ServerFieldTypeSet type_set;
  type_set.insert(UNKNOWN_TYPE);
  std::vector<ServerFieldTypeSet> unknown_types(expected_types.size(),
                                                type_set);
  autofill_manager_->set_expected_submitted_field_types(unknown_types);
  FormSubmitted(response_data);
  ASSERT_EQ(1u, personal_data_.GetProfiles().size());

  // The second submission should now have data by which to infer types.
  autofill_manager_->set_expected_submitted_field_types(expected_types);
  FormSubmitted(response_data);
  ASSERT_EQ(2u, personal_data_.GetProfiles().size());
}

// Test that the form signature for an uploaded form always matches the form
// signature from the query.
TEST_F(AutofillManagerTest, FormSubmittedWithDifferentFields) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Cache the expected form signature.
  std::string signature = FormStructure(form).FormSignatureAsStr();

  // Change the structure of the form prior to submission.
  // Websites would typically invoke JavaScript either on page load or on form
  // submit to achieve this.
  form.fields.pop_back();
  FormFieldData field = form.fields[3];
  form.fields[3] = form.fields[7];
  form.fields[7] = field;

  // Simulate form submission.
  FormSubmitted(form);
  EXPECT_EQ(signature, autofill_manager_->GetSubmittedFormSignature());
}

// Test that we do not save form data when submitted fields contain default
// values.
TEST_F(AutofillManagerTest, FormSubmittedWithDefaultValues) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  form.fields[3].value = ASCIIToUTF16("Enter your address");

  // Convert the state field to a <select> popup, to make sure that we only
  // reject default values for text fields.
  ASSERT_TRUE(form.fields[6].name == ASCIIToUTF16("state"));
  form.fields[6].form_control_type = "select-one";
  form.fields[6].value = ASCIIToUTF16("Tennessee");

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  const char guid[] = "00000000-0000-0000-0000-000000000001";
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[3],
                                     MakeFrontendID(std::string(), guid),
                                     &response_page_id, &response_data);

  // Simulate form submission.  We should call into the PDM to try to save the
  // filled data.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());

  // Set the address field's value back to the default value.
  response_data.fields[3].value = ASCIIToUTF16("Enter your address");

  // Simulate form submission.  We should not call into the PDM to try to save
  // the filled data, since the filled form is effectively missing an address.
  FormSubmitted(response_data);
  EXPECT_EQ(1, personal_data_.num_times_save_imported_profile_called());
}

// Tests that credit card data are saved for forms on https
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_ImportFormDataCreditCardHTTPS \
  DISABLED_ImportFormDataCreditCardHTTPS
#else
#define MAYBE_ImportFormDataCreditCardHTTPS ImportFormDataCreditCardHTTPS
#endif
TEST_F(AutofillManagerTest, MAYBE_ImportFormDataCreditCardHTTPS) {
  TestSaveCreditCards(true);
}

// Tests that credit card data are saved for forms on http
// TODO(crbug.com/666704): Flaky on android_n5x_swarming_rel bot.
#if defined(OS_ANDROID)
#define MAYBE_ImportFormDataCreditCardHTTP DISABLED_ImportFormDataCreditCardHTTP
#else
#define MAYBE_ImportFormDataCreditCardHTTP ImportFormDataCreditCardHTTP
#endif
TEST_F(AutofillManagerTest, MAYBE_ImportFormDataCreditCardHTTP) {
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
TEST_F(AutofillManagerTest, MAYBE_CreditCardSavedWhenAutocompleteOff) {
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
TEST_F(AutofillManagerTest, InvalidCreditCardNumberIsNotSaved) {
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

// Tests that DeterminePossibleFieldTypesForUpload makes accurate matches.
TEST_F(AutofillManagerTest, DeterminePossibleFieldTypesForUpload) {
  // Set up the test profiles.
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "Apt. 10", "Memphis", "Tennessee", "38116", "US",
                       "+1 (234) 567-8901");
  profile.set_guid("00000000-0000-0000-0000-000000000001");
  profiles.push_back(profile);
  test::SetProfileInfo(&profile, "Charles", "", "Holley", "buddy@gmail.com",
                       "Decca", "123 Apple St.", "unit 6", "Lubbock", "TX",
                       "79401", "US", "5142821292");
  profile.set_guid("00000000-0000-0000-0000-000000000002");
  profiles.push_back(profile);
  test::SetProfileInfo(&profile, "Charles", "", "Baudelaire",
                       "lesfleursdumal@gmail.com", "", "108 Rue Saint-Lazare",
                       "Apt. 10", "Paris", "Île de France", "75008", "FR",
                       "+33 2 49 19 70 70");
  profile.set_guid("00000000-0000-0000-0000-000000000001");
  profiles.push_back(profile);

  // Set up the test credit cards.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "John Doe", "4234-5678-9012-3456", "04",
                          "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  typedef struct {
    std::string input_value;     // The value to input in the field.
    ServerFieldType field_type;  // The expected field type to be determined.
  } TestCase;

  TestCase test_cases[] = {
      // Profile fields matches.
      {"Elvis", NAME_FIRST},
      {"Aaron", NAME_MIDDLE},
      {"A", NAME_MIDDLE_INITIAL},
      {"Presley", NAME_LAST},
      {"Elvis Aaron Presley", NAME_FULL},
      {"theking@gmail.com", EMAIL_ADDRESS},
      {"RCA", COMPANY_NAME},
      {"3734 Elvis Presley Blvd.", ADDRESS_HOME_LINE1},
      {"Apt. 10", ADDRESS_HOME_LINE2},
      {"Memphis", ADDRESS_HOME_CITY},
      {"Tennessee", ADDRESS_HOME_STATE},
      {"38116", ADDRESS_HOME_ZIP},
      {"USA", ADDRESS_HOME_COUNTRY},
      {"United States", ADDRESS_HOME_COUNTRY},
      {"12345678901", PHONE_HOME_WHOLE_NUMBER},
      {"+1 (234) 567-8901", PHONE_HOME_WHOLE_NUMBER},
      {"(234)567-8901", PHONE_HOME_CITY_AND_NUMBER},
      {"2345678901", PHONE_HOME_CITY_AND_NUMBER},
      {"1", PHONE_HOME_COUNTRY_CODE},
      {"234", PHONE_HOME_CITY_CODE},
      {"5678901", PHONE_HOME_NUMBER},
      {"567", PHONE_HOME_NUMBER},
      {"8901", PHONE_HOME_NUMBER},

      // Test a European profile.
      {"Paris", ADDRESS_HOME_CITY},
      {"Île de France", ADDRESS_HOME_STATE},    // Exact match
      {"Ile de France", ADDRESS_HOME_STATE},    // Missing accent.
      {"-Ile-de-France-", ADDRESS_HOME_STATE},  // Extra punctuation.
      {"île dÉ FrÃÑÇË", ADDRESS_HOME_STATE},  // Other accents & case mismatch.
      {"75008", ADDRESS_HOME_ZIP},
      {"FR", ADDRESS_HOME_COUNTRY},
      {"France", ADDRESS_HOME_COUNTRY},
      {"33249197070", PHONE_HOME_WHOLE_NUMBER},
      {"+33 2 49 19 70 70", PHONE_HOME_WHOLE_NUMBER},
      {"2 49 19 70 70", PHONE_HOME_CITY_AND_NUMBER},
      {"249197070", PHONE_HOME_CITY_AND_NUMBER},
      {"33", PHONE_HOME_COUNTRY_CODE},
      {"2", PHONE_HOME_CITY_CODE},

      // Credit card fields matches.
      {"John Doe", CREDIT_CARD_NAME_FULL},
      {"John", CREDIT_CARD_NAME_FIRST},
      {"Doe", CREDIT_CARD_NAME_LAST},
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER},
      {"04", CREDIT_CARD_EXP_MONTH},
      {"April", CREDIT_CARD_EXP_MONTH},
      {"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR},
      {"99", CREDIT_CARD_EXP_2_DIGIT_YEAR},
      {"04/2999", CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR},

      // Make sure whitespace and invalid characters are handled properly.
      {"", EMPTY_TYPE},
      {" ", EMPTY_TYPE},
      {"***", UNKNOWN_TYPE},
      {" Elvis", NAME_FIRST},
      {"Elvis ", NAME_FIRST},

      // Make sure fields that differ by case match.
      {"elvis ", NAME_FIRST},
      {"UnItEd StAtEs", ADDRESS_HOME_COUNTRY},

      // Make sure fields that differ by punctuation match.
      {"3734 Elvis Presley Blvd", ADDRESS_HOME_LINE1},
      {"3734, Elvis    Presley Blvd.", ADDRESS_HOME_LINE1},

      // Make sure that a state's full name and abbreviation match.
      {"TN", ADDRESS_HOME_STATE},     // Saved as "Tennessee" in profile.
      {"Texas", ADDRESS_HOME_STATE},  // Saved as "TX" in profile.

      // Special phone number case. A profile with no country code should only
      // match PHONE_HOME_CITY_AND_NUMBER.
      {"5142821292", PHONE_HOME_CITY_AND_NUMBER},

      // Make sure unsupported variants do not match.
      {"Elvis Aaron", UNKNOWN_TYPE},
      {"Mr. Presley", UNKNOWN_TYPE},
      {"3734 Elvis Presley", UNKNOWN_TYPE},
      {"38116-1023", UNKNOWN_TYPE},
      {"5", UNKNOWN_TYPE},
      {"56", UNKNOWN_TYPE},
      {"901", UNKNOWN_TYPE}};

  for (TestCase test_case : test_cases) {
    FormData form;
    form.name = ASCIIToUTF16("MyForm");
    form.origin = GURL("http://myform.com/form.html");
    form.action = GURL("http://myform.com/submit.html");

    FormFieldData field;
    test::CreateTestFormField("", "1", "", "text", &field);
    field.value = UTF8ToUTF16(test_case.input_value);
    form.fields.push_back(field);

    FormStructure form_structure(form);

    AutofillManager::DeterminePossibleFieldTypesForUpload(
        profiles, credit_cards, "en-us", &form_structure);

    ASSERT_EQ(1U, form_structure.field_count());
    ServerFieldTypeSet possible_types =
        form_structure.field(0)->possible_types();
    EXPECT_EQ(1U, possible_types.size());

    EXPECT_NE(possible_types.end(), possible_types.find(test_case.field_type))
        << "Failed to determine type for: \"" << test_case.input_value << "\"";
  }
}

// Tests that DeterminePossibleFieldTypesForUpload is called when a form is
// submitted.
TEST_F(AutofillManagerTest, DeterminePossibleFieldTypesForUpload_IsTriggered) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  std::vector<base::string16> expected_values;

  // These fields should all match.
  FormFieldData field;
  ServerFieldTypeSet types;

  test::CreateTestFormField("", "1", "", "text", &field);
  expected_values.push_back(ASCIIToUTF16("Elvis"));
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "2", "", "text", &field);
  expected_values.push_back(ASCIIToUTF16("Aaron"));
  types.clear();
  types.insert(NAME_MIDDLE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "3", "", "text", &field);
  expected_values.push_back(ASCIIToUTF16("A"));
  types.clear();
  types.insert(NAME_MIDDLE_INITIAL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // Make sure the form is in the cache so that it is processed for Autofill
  // upload.
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Once the form is cached, fill the values.
  EXPECT_EQ(form.fields.size(), expected_values.size());
  for (size_t i = 0; i < expected_values.size(); i++) {
    form.fields[i].value = expected_values[i];
  }

  autofill_manager_->set_expected_submitted_field_types(expected_types);
  FormSubmitted(form);
}

// Tests that DisambiguateUploadTypes makes the correct choices.
TEST_F(AutofillManagerTest, DisambiguateUploadTypes) {
  // Set up the test profile.
  std::vector<AutofillProfile> profiles;
  AutofillProfile profile;
  test::SetProfileInfo(&profile, "Elvis", "Aaron", "Presley",
                       "theking@gmail.com", "RCA", "3734 Elvis Presley Blvd.",
                       "", "Memphis", "Tennessee", "38116", "US",
                       "(234) 567-8901");
  profile.set_guid("00000000-0000-0000-0000-000000000001");
  profiles.push_back(profile);

  // Set up the test credit card.
  std::vector<CreditCard> credit_cards;
  CreditCard credit_card;
  test::SetCreditCardInfo(&credit_card, "Elvis Presley", "4234-5678-9012-3456",
                          "04", "2999", "1");
  credit_card.set_guid("00000000-0000-0000-0000-000000000003");
  credit_cards.push_back(credit_card);

  typedef struct {
    std::string input_value;
    ServerFieldType predicted_type;
    bool expect_disambiguation;
    ServerFieldType expected_upload_type;
  } TestFieldData;

  std::vector<TestFieldData> test_cases[13];

  // Address disambiguation.
  // An ambiguous address line followed by a field predicted as a line 2 and
  // that is empty should be disambiguated as an ADDRESS_HOME_LINE1.
  test_cases[0].push_back({"3734 Elvis Presley Blvd.", ADDRESS_HOME_LINE1, true,
                           ADDRESS_HOME_LINE1});
  test_cases[0].push_back({"", ADDRESS_HOME_LINE2, true, EMPTY_TYPE});

  // An ambiguous address line followed by a field predicted as a line 2 but
  // filled with another know profile value should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases[1].push_back({"3734 Elvis Presley Blvd.",
                           ADDRESS_HOME_STREET_ADDRESS, true,
                           ADDRESS_HOME_STREET_ADDRESS});
  test_cases[1].push_back(
      {"38116", ADDRESS_HOME_LINE2, true, ADDRESS_HOME_ZIP});

  // An ambiguous address line followed by an empty field predicted as
  // something other than a line 2 should be disambiguated as an
  // ADDRESS_HOME_STREET_ADDRESS.
  test_cases[2].push_back({"3734 Elvis Presley Blvd.",
                           ADDRESS_HOME_STREET_ADDRESS, true,
                           ADDRESS_HOME_STREET_ADDRESS});
  test_cases[2].push_back({"", ADDRESS_HOME_ZIP, true, EMPTY_TYPE});

  // An ambiguous address line followed by no other field should be
  // disambiguated as an ADDRESS_HOME_STREET_ADDRESS.
  test_cases[3].push_back({"3734 Elvis Presley Blvd.",
                           ADDRESS_HOME_STREET_ADDRESS, true,
                           ADDRESS_HOME_STREET_ADDRESS});

  // Phone number disambiguation.
  // A field with possible types PHONE_HOME_CITY_AND_NUMBER and
  // PHONE_HOME_WHOLE_NUMBER should be disambiguated as
  // PHONE_HOME_CITY_AND_NUMBER
  test_cases[4].push_back({"2345678901", PHONE_HOME_WHOLE_NUMBER, true,
                           PHONE_HOME_CITY_AND_NUMBER});

  // Name disambiguation.
  // An ambiguous name field that has no next field and that is preceded by
  // a non credit card field should be disambiguated as a non credit card
  // name.
  test_cases[5].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});
  test_cases[5].push_back({"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST});
  test_cases[5].push_back({"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST});

  // An ambiguous name field that has no next field and that is preceded by
  // a credit card field should be disambiguated as a credit card name.
  test_cases[6].push_back(
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER});
  test_cases[6].push_back({"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST});
  test_cases[6].push_back({"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST});

  // An ambiguous name field that has no previous field and that is
  // followed by a non credit card field should be disambiguated as a non
  // credit card name.
  test_cases[7].push_back({"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST});
  test_cases[7].push_back({"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST});
  test_cases[7].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});

  // An ambiguous name field that has no previous field and that is followed
  // by a credit card field should be disambiguated as a credit card name.
  test_cases[8].push_back({"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST});
  test_cases[8].push_back({"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST});
  test_cases[8].push_back(
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER});

  // An ambiguous name field that is preceded and followed by non credit
  // card fields should be disambiguated as a non credit card name.
  test_cases[9].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});
  test_cases[9].push_back({"Elvis", CREDIT_CARD_NAME_FIRST, true, NAME_FIRST});
  test_cases[9].push_back({"Presley", CREDIT_CARD_NAME_LAST, true, NAME_LAST});
  test_cases[9].push_back(
      {"Tennessee", ADDRESS_HOME_STATE, true, ADDRESS_HOME_STATE});

  // An ambiguous name field that is preceded and followed by credit card
  // fields should be disambiguated as a credit card name.
  test_cases[10].push_back(
      {"4234-5678-9012-3456", CREDIT_CARD_NUMBER, true, CREDIT_CARD_NUMBER});
  test_cases[10].push_back({"Elvis", NAME_FIRST, true, CREDIT_CARD_NAME_FIRST});
  test_cases[10].push_back({"Presley", NAME_LAST, true, CREDIT_CARD_NAME_LAST});
  test_cases[10].push_back({"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
                            CREDIT_CARD_EXP_4_DIGIT_YEAR});

  // An ambiguous name field that is preceded by a non credit card field and
  // followed by a credit card field should not be disambiguated.
  test_cases[11].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});
  test_cases[11].push_back(
      {"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST});
  test_cases[11].push_back(
      {"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST});
  test_cases[11].push_back({"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
                            CREDIT_CARD_EXP_4_DIGIT_YEAR});

  // An ambiguous name field that is preceded by a credit card field and
  // followed by a non credit card field should not be disambiguated.
  test_cases[12].push_back({"2999", CREDIT_CARD_EXP_4_DIGIT_YEAR, true,
                            CREDIT_CARD_EXP_4_DIGIT_YEAR});
  test_cases[12].push_back(
      {"Elvis", NAME_FIRST, false, CREDIT_CARD_NAME_FIRST});
  test_cases[12].push_back(
      {"Presley", NAME_LAST, false, CREDIT_CARD_NAME_LAST});
  test_cases[12].push_back(
      {"Memphis", ADDRESS_HOME_CITY, true, ADDRESS_HOME_CITY});

  for (const std::vector<TestFieldData>& test_fields : test_cases) {
    FormData form;
    form.name = ASCIIToUTF16("MyForm");
    form.origin = GURL("http://myform.com/form.html");
    form.action = GURL("http://myform.com/submit.html");

    // Create the form fields specified in the test case.
    FormFieldData field;
    for (const TestFieldData& test_field : test_fields) {
      test::CreateTestFormField("", "1", "", "text", &field);
      field.value = ASCIIToUTF16(test_field.input_value);
      form.fields.push_back(field);
    }

    // Assign the specified predicted type for each field in the test case.
    FormStructure form_structure(form);
    for (size_t i = 0; i < test_fields.size(); ++i) {
      form_structure.field(i)->set_server_type(test_fields[i].predicted_type);
    }

    AutofillManager::DeterminePossibleFieldTypesForUpload(
        profiles, credit_cards, "en-us", &form_structure);
    ASSERT_EQ(test_fields.size(), form_structure.field_count());

    // Make sure the disambiguation method selects the expected upload type.
    ServerFieldTypeSet possible_types;
    for (size_t i = 0; i < test_fields.size(); ++i) {
      possible_types = form_structure.field(i)->possible_types();
      if (test_fields[i].expect_disambiguation) {
        EXPECT_EQ(1U, possible_types.size());
        EXPECT_NE(possible_types.end(),
                  possible_types.find(test_fields[i].expected_upload_type));
      } else {
        EXPECT_EQ(2U, possible_types.size());
      }
    }
  }
}

TEST_F(AutofillManagerTest, RemoveProfile) {
  // Add and remove an Autofill profile.
  std::unique_ptr<AutofillProfile> profile =
      base::MakeUnique<AutofillProfile>();
  const char guid[] = "00000000-0000-0000-0000-000000000102";
  profile->set_guid(guid);
  autofill_manager_->AddProfile(std::move(profile));

  int id = MakeFrontendID(std::string(), guid);

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(autofill_manager_->GetProfileWithGUID(guid));
}

TEST_F(AutofillManagerTest, RemoveCreditCard) {
  // Add and remove an Autofill credit card.
  CreditCard credit_card;
  const char guid[] = "00000000-0000-0000-0000-000000100007";
  credit_card.set_guid(guid);
  autofill_manager_->AddCreditCard(credit_card);

  int id = MakeFrontendID(guid, std::string());

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(autofill_manager_->GetCreditCardWithGUID(guid));
}

// Test our external delegate is called at the right time.
TEST_F(AutofillManagerTest, TestExternalDelegate) {
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);  // should call the delegate's OnQuery()

  EXPECT_TRUE(external_delegate_->on_query_seen());
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(AutofillManagerTest, OnTextFieldDidChangeAndUnfocus_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->set_expected_submitted_field_types(expected_types);
  autofill_manager_->set_expected_observed_submission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = ASCIIToUTF16("Elvis");
  form.fields[1].value = ASCIIToUTF16("Presley");
  form.fields[2].value = ASCIIToUTF16("theking@gmail.com");
  // Simulate editing a field.
  autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                          gfx::RectF(), base::TimeTicks::Now());

  autofill_manager_->ResetRunLoop();
  // Simulate lost of focus on the form.
  autofill_manager_->OnFocusNoLongerOnForm();
  // Wait for upload to complete (will check expected types as well).
  autofill_manager_->WaitForAsyncUploadProcess();
}

// Test that navigating with a filled form sends an upload with types matching
// the fields.
TEST_F(AutofillManagerTest, OnTextFieldDidChangeAndNavigation_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->set_expected_submitted_field_types(expected_types);
  autofill_manager_->set_expected_observed_submission(false);

  // The fields are edited after calling FormsSeen on them. This is because
  // default values are not used for upload comparisons.
  form.fields[0].value = ASCIIToUTF16("Elvis");
  form.fields[1].value = ASCIIToUTF16("Presley");
  form.fields[2].value = ASCIIToUTF16("theking@gmail.com");
  // Simulate editing a field.
  autofill_manager_->OnTextFieldDidChange(form, form.fields.front(),
                                          gfx::RectF(), base::TimeTicks::Now());

  autofill_manager_->ResetRunLoop();
  // Simulate a navigation so that the pending form is uploaded.
  autofill_manager_->Reset();
  // Wait for upload to complete (will check expected types as well).
  autofill_manager_->WaitForAsyncUploadProcess();
}

// Test that unfocusing a filled form sends an upload with types matching the
// fields.
TEST_F(AutofillManagerTest, OnDidFillAutofillFormDataAndUnfocus_Upload) {
  // Set up our form data (empty).
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;

  // These fields should all match.
  ServerFieldTypeSet types;
  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  types.insert(NAME_FIRST);
  expected_types.push_back(types);

  test::CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(NAME_LAST);
  expected_types.push_back(types);

  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  FormsSeen(std::vector<FormData>(1, form));

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->set_expected_submitted_field_types(expected_types);
  autofill_manager_->set_expected_observed_submission(false);

  // Form was autofilled with user data.
  form.fields[0].value = ASCIIToUTF16("Elvis");
  form.fields[1].value = ASCIIToUTF16("Presley");
  form.fields[2].value = ASCIIToUTF16("theking@gmail.com");
  autofill_manager_->OnDidFillAutofillFormData(form, base::TimeTicks::Now());

  autofill_manager_->ResetRunLoop();
  // Simulate lost of focus on the form.
  autofill_manager_->OnFocusNoLongerOnForm();
  // Wait for upload to complete.
  autofill_manager_->WaitForAsyncUploadProcess();
}

// Test that suggestions are returned for credit card fields with an
// unrecognized
// autocomplete attribute.
TEST_F(AutofillManagerTest, GetCreditCardSuggestions_UnrecognizedAttribute) {
  // Set up the form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData field;
  // Set a valid autocomplete attribute on the card name.
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.autocomplete_attribute = "cc-name";
  form.fields.push_back(field);
  // Set no autocomplete attribute on the card number.
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.autocomplete_attribute = "";
  form.fields.push_back(field);
  // Set an unrecognized autocomplete attribute on the expiration month.
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  field.autocomplete_attribute = "unrecognized";
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Suggestions should be returned for the first two fields
  GetAutofillSuggestions(form, form.fields[0]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  GetAutofillSuggestions(form, form.fields[1]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());

  // Suggestions should still be returned for the third field because it is a
  // credit card field.
  GetAutofillSuggestions(form, form.fields[2]);
  EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
}

// Test to verify suggestions appears for forms having credit card number split
// across fields.
TEST_F(AutofillManagerTest,
       GetCreditCardSuggestions_ForNumberSpitAcrossFields) {
  // Set up our form data with credit card number split across fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");

  FormFieldData name_field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text",
                            &name_field);
  form.fields.push_back(name_field);

  // Add new 4 |card_number_field|s to the |form|.
  FormFieldData card_number_field;
  card_number_field.max_length = 4;
  test::CreateTestFormField("Card Number", "cardnumber_1", "", "text",
                            &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_2", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_3", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  test::CreateTestFormField("", "cardnumber_4", "", "text", &card_number_field);
  form.fields.push_back(card_number_field);

  FormFieldData exp_field;
  test::CreateTestFormField("Expiration Date", "ccmonth", "", "text",
                            &exp_field);
  form.fields.push_back(exp_field);

  test::CreateTestFormField("", "ccyear", "", "text", &exp_field);
  form.fields.push_back(exp_field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Verify whether suggestions are populated correctly for one of the middle
  // credit card number fields when filled partially.
  FormFieldData number_field = form.fields[3];
  number_field.value = ASCIIToUTF16("901");

  // Get the suggestions for already filled credit card |number_field|.
  GetAutofillSuggestions(form, number_field);

  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion(std::string("Visa") + kUTF8MidlineEllipsis + "3456", "04/99",
                 kVisaCard, autofill_manager_->GetPackedCreditCardID(4)));
}

// Test that inputs detected to be CVC inputs are forced to
// !should_autocomplete for AutocompleteHistoryManager::OnWillSubmitForm.
TEST_F(AutofillManagerTest, DontSaveCvcInAutocompleteHistory) {
  FormData form_seen_by_ahm;
  MockAutocompleteHistoryManager* m = RecreateMockAutocompleteHistoryManager();
  EXPECT_CALL(*m, OnWillSubmitForm(_)).WillOnce(SaveArg<0>(&form_seen_by_ahm));

  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  struct {
    const char* label;
    const char* name;
    const char* value;
    ServerFieldType expected_field_type;
  } test_fields[] = {
      {"Card number", "1", "4234-5678-9012-3456", CREDIT_CARD_NUMBER},
      {"Card verification code", "2", "123", CREDIT_CARD_VERIFICATION_CODE},
      {"expiration date", "3", "04/2020", CREDIT_CARD_EXP_4_DIGIT_YEAR},
  };

  for (const auto& test_field : test_fields) {
    FormFieldData field;
    test::CreateTestFormField(test_field.label, test_field.name,
                              test_field.value, "text", &field);
    form.fields.push_back(field);
  }

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  FormSubmitted(form);

  EXPECT_EQ(form.fields.size(), form_seen_by_ahm.fields.size());
  ASSERT_EQ(arraysize(test_fields), form_seen_by_ahm.fields.size());
  for (size_t i = 0; i < arraysize(test_fields); ++i) {
    EXPECT_EQ(
        form_seen_by_ahm.fields[i].should_autocomplete,
        test_fields[i].expected_field_type != CREDIT_CARD_VERIFICATION_CODE);
  }
}

TEST_F(AutofillManagerTest, DontOfferToSavePaymentsCard) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  // Manually fill out |form| so we can use it in OnFormSubmitted.
  for (size_t i = 0; i < form.fields.size(); ++i) {
    if (form.fields[i].name == ASCIIToUTF16("cardnumber"))
      form.fields[i].value = ASCIIToUTF16("4012888888881881");
    else if (form.fields[i].name == ASCIIToUTF16("nameoncard"))
      form.fields[i].value = ASCIIToUTF16("John H Dillinger");
    else if (form.fields[i].name == ASCIIToUTF16("ccmonth"))
      form.fields[i].value = ASCIIToUTF16("01");
    else if (form.fields[i].name == ASCIIToUTF16("ccyear"))
      form.fields[i].value = ASCIIToUTF16("2017");
  }

  CardUnmaskDelegate::UnmaskResponse response;
  response.should_store_pan = false;
  response.cvc = ASCIIToUTF16("123");
  full_card_unmask_delegate()->OnUnmaskResponse(response);
  autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                     "4012888888881881");
  autofill_manager_->OnFormSubmitted(form);
}

TEST_F(AutofillManagerTest, FillInUpdatedExpirationDate) {
  FormData form;
  CreditCard card;
  PrepareForRealPanResponse(&form, &card);

  CardUnmaskDelegate::UnmaskResponse response;
  response.should_store_pan = false;
  response.cvc = ASCIIToUTF16("123");
  response.exp_month = ASCIIToUTF16("02");
  response.exp_year = ASCIIToUTF16("2018");
  full_card_unmask_delegate()->OnUnmaskResponse(response);
  autofill_manager_->OnDidGetRealPan(AutofillClient::SUCCESS,
                                     "4012888888881881");
}

TEST_F(AutofillManagerTest, UploadCreditCard) {
  personal_data_.ClearCreditCards();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));

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

TEST_F(AutofillManagerTest, UploadCreditCard_RequestCVCEnabled_DoesNotTrigger) {
  EnableAutofillUpstreamRequestCvcIfMissingExperiment();

  personal_data_.ClearCreditCards();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  // Submitted form included CVC, so user did not need to enter CVC.
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));
}

TEST_F(AutofillManagerTest, UploadCreditCardAndSaveCopy) {
  personal_data_.ClearCreditCards();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

  const char* const server_id = "InstrumentData:1234";
  autofill_manager_->ResetPaymentsClientForCardUpload(server_id);

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

  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
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

TEST_F(AutofillManagerTest, UploadCreditCard_FeatureNotEnabled) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(false);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that no histogram entry was logged.
  histogram_tester.ExpectTotalCount("Autofill.CardUploadDecisionMetric", 0);
}

TEST_F(AutofillManagerTest, UploadCreditCard_CvcUnavailable) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_VALUE_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_VALUE_NOT_FOUND);
}

TEST_F(AutofillManagerTest, UploadCreditCard_CvcInvalidLength) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::INVALID_CVC_VALUE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::INVALID_CVC_VALUE);
}

TEST_F(AutofillManagerTest, UploadCreditCard_MultipleCvcFields) {
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest, UploadCreditCard_NoCvcFieldOnForm) {
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_InvalidCvcInNonCvcField) {
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInNonCvcField) {
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::FOUND_POSSIBLE_CVC_VALUE_IN_NON_CVC_FIELD);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_NoCvcFieldOnForm_CvcInAddressField) {
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

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
TEST_F(AutofillManagerTest,
       MAYBE_UploadCreditCard_NoCvcFieldOnForm_UserEntersCvc) {
  EnableAutofillUpstreamRequestCvcIfMissingExperiment();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name,
                           kAutofillUpstreamRequestCvcIfMissing.name));

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

TEST_F(AutofillManagerTest, UploadCreditCard_NoCvcFieldOnFormExperimentOff) {
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::CVC_FIELD_NOT_FOUND);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::CVC_FIELD_NOT_FOUND);
}

// kAutofillUpstreamShowNewUi and kAutofillUpstreamShowGoogleLogo flags are
// currently not available on Android.
#if !defined(OS_ANDROID)
TEST_F(AutofillManagerTest,
       UploadCreditCard_AddNewUiFlagStateToRequestIfExperimentOn) {
  EnableAutofillUpstreamShowNewUiExperiment();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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

  // Confirm upload happened and the new UI flag was sent in the request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name,
                           kAutofillUpstreamShowNewUi.name));
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_DoNotAddNewUiFlagStateToRequestIfExperimentOff) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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

  // Confirm upload happened and the new UI flag was sent in the request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_AddShowGoogleLogoFlagStateToRequestIfExperimentOn) {
  EnableAutofillUpstreamShowGoogleLogoExperiment();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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

  // Confirm upload happened and the show Google logo flag was sent in the
  // request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name,
                           kAutofillUpstreamShowGoogleLogo.name));
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_DoNotAddShowGoogleLogoFlagStateToRequestIfExpOff) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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

  // Confirm upload happened and the show Google logo flag was sent in the
  // request.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));
}
#endif

TEST_F(AutofillManagerTest, UploadCreditCard_NoProfileAvailable) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entries are logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ADDRESS_PROFILE);
}

TEST_F(AutofillManagerTest, UploadCreditCard_NoRecentlyUsedProfile) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

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

TEST_F(AutofillManagerTest,
       UploadCreditCard_NoRecentlyUsedProfile_CanUseOldProfile) {
  variation_params_.SetVariationParamsWithFeatureAssociations(
      kAutofillUpstreamUseNotRecentlyUsedAutofillProfile.name,
      {{kAutofillUpstreamMaxMinutesSinceAutofillProfileUseKey,
        base::IntToString((kMuchLaterTime - kArbitraryTime).InMinutes() + 1)}},
      {kAutofillUpstreamUseNotRecentlyUsedAutofillProfile.name});

  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit an address form in order to establish a profile.
  FormData address_form;
  test::CreateTestAddressFormData(&address_form);
  FormsSeen({address_form});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form);
  FormSubmitted(address_form);

  // Advance the current time. Although |address_form| is not a recently used
  // address profile, we will include it in the candidate profiles because
  // there are no recently used address profiles and the feature to use older
  // profiles is enabled.
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

  // Upload should be offered.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_THAT(autofill_manager_->GetActiveExperiments(),
              UnorderedElementsAre(
                  kAutofillUpstreamUseAutofillProfileComparator.name,
                  kAutofillUpstreamUseNotRecentlyUsedAutofillProfile.name));

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_CvcUnavailableAndNoProfileAvailable) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

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

TEST_F(AutofillManagerTest, UploadCreditCard_NoNameAvailable) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_NOT_OFFERED_NO_NAME);
}

TEST_F(AutofillManagerTest, UploadCreditCard_ZipCodesConflict) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}

TEST_F(AutofillManagerTest, UploadCreditCard_ZipCodesDiscardWhitespace) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_ZipCodesDiscardWhitespace_DisableComparator) {
  DisableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_ZIPS);
}

TEST_F(AutofillManagerTest, UploadCreditCard_ZipCodesHavePrefixMatch) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest, UploadCreditCard_NoZipCodeAvailable) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_NOT_OFFERED_NO_ZIP_CODE);
}

TEST_F(AutofillManagerTest, UploadCreditCard_CCFormHasMiddleInitial) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_CCFormHasMiddleInitial_DisableComparator) {
  DisableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  EXPECT_TRUE(autofill_manager_->GetActiveExperiments().empty());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest, UploadCreditCard_NoMiddleInitialInCCForm) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_NoMiddleInitialInCCForm_DisableComparator) {
  DisableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest, UploadCreditCard_CCFormHasMiddleName) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_CCFormHasMiddleName_DisableComparator) {
  DisableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(AutofillManagerTest, UploadCreditCard_CCFormRemovesMiddleName) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest, UploadCreditCard_NamesHaveToMatch) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(AutofillManagerTest, UploadCreditCard_IgnoreOldProfiles) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_IgnoreOldProfiles_CanUseOldProfiles) {
  variation_params_.SetVariationParamsWithFeatureAssociations(
      kAutofillUpstreamUseNotRecentlyUsedAutofillProfile.name,
      {{kAutofillUpstreamMaxMinutesSinceAutofillProfileUseKey,
        base::IntToString((kMuchLaterTime - kArbitraryTime).InMinutes() + 1)}},
      {kAutofillUpstreamUseNotRecentlyUsedAutofillProfile.name});

  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

  // Create, fill and submit two address forms with different names.
  FormData address_form1, address_form2;
  test::CreateTestAddressFormData(&address_form1);
  test::CreateTestAddressFormData(&address_form2);
  FormsSeen({address_form1, address_form2});

  ManuallyFillAddressForm("Flo", "Master", "77401", "US", &address_form1);
  FormSubmitted(address_form1);

  // Advance the current time. Since |address_form1| will not be a recently
  // used address profile, we will not include it in the candidate profiles
  // because we have a recently used address.
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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());
  // Recently used profile was available, so did not need to use old profile.
  EXPECT_THAT(
      autofill_manager_->GetActiveExperiments(),
      UnorderedElementsAre(kAutofillUpstreamUseAutofillProfileComparator.name));

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(histogram_tester,
                                 AutofillMetrics::UPLOAD_OFFERED);
}

TEST_F(AutofillManagerTest,
       UploadCreditCard_NamesHaveToMatch_DisableComparator) {
  DisableAutofillUpstreamUseAutofillProfileComparator();
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  FormsSeen({credit_card_form});

  // Edit the data, but use yet another name, and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Bob Master");
  FormsSeen({credit_card_form});

  // Edit the credit card form and submit.
  credit_card_form.fields[0].value = ASCIIToUTF16("Flo Master");
  credit_card_form.fields[1].value = ASCIIToUTF16("4111111111111111");
  credit_card_form.fields[2].value = ASCIIToUTF16("11");
  credit_card_form.fields[3].value = ASCIIToUTF16("2017");
  credit_card_form.fields[4].value = ASCIIToUTF16("123");

  base::HistogramTester histogram_tester;

  // Names are required to match, upload should not happen.
  EXPECT_CALL(autofill_client_, ConfirmSaveCreditCardLocally(_, _)).Times(0);
  FormSubmitted(credit_card_form);
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester, AutofillMetrics::UPLOAD_NOT_OFFERED_CONFLICTING_NAMES);
}

TEST_F(AutofillManagerTest, UploadCreditCard_LogPreviousUseDate) {
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(kArbitraryTime);

  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

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
  EXPECT_TRUE(autofill_manager_->credit_card_was_uploaded());

  // Verify that UMA for "DaysSincePreviousUse" is logged.
  histogram_tester.ExpectUniqueSample(
      "Autofill.DaysSincePreviousUseAtSubmission.Profile",
      (kMuchLaterTime - kArbitraryTime).InDays(),
      /*expected_count=*/1);
}

TEST_F(AutofillManagerTest, UploadCreditCard_UploadDetailsFails) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);

  // Anything other than "en-US" will cause GetUploadDetails to return a failure
  // response.
  autofill_manager_->set_app_locale("pt-BR");

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());

  // Verify that the correct histogram entry (and only that) was logged.
  ExpectUniqueCardUploadDecision(
      histogram_tester,
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
  // Verify that the correct UKM was logged.
  ExpectCardUploadDecisionUkm(
      AutofillMetrics::UPLOAD_NOT_OFFERED_GET_UPLOAD_DETAILS_FAILED);
}

TEST_F(AutofillManagerTest, DuplicateMaskedCreditCard) {
  EnableAutofillOfferLocalSaveIfServerCardManuallyEnteredExperiment();

  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);
  autofill_manager_->set_app_locale("en-US");

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());
}

TEST_F(AutofillManagerTest, DuplicateMaskedCreditCard_ExperimentOff) {
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->set_credit_card_upload_enabled(true);
  autofill_manager_->set_app_locale("en-US");

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
  EXPECT_FALSE(autofill_manager_->credit_card_was_uploaded());
}

// Verify that typing "gmail" will match "theking@gmail.com" and
// "buddy@gmail.com" when substring matching is enabled.
TEST_F(AutofillManagerTest, DisplaySuggestionsWithMatchingTokens) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "gmail", "email", &field);
  GetAutofillSuggestions(form, field);

  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("buddy@gmail.com", "123 Apple St.", "", 1),
      Suggestion("theking@gmail.com", "3734 Elvis Presley Blvd.", "", 2));
}

// Verify that typing "apple" will match "123 Apple St." when substring matching
// is enabled.
TEST_F(AutofillManagerTest, DisplaySuggestionsWithMatchingTokens_CaseIgnored) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Address Line 2", "addr2", "apple", "text", &field);
  GetAutofillSuggestions(form, field);

  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion("123 Apple St., unit 6", "123 Apple St.", "", 1));
}

// Verify that typing "mail" will not match any of the "@gmail.com" email
// addresses when substring matching is enabled.
TEST_F(AutofillManagerTest, NoSuggestionForNonPrefixTokenMatch) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "mail", "email", &field);
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Verify that typing "pres" will match "Elvis Presley" when substring matching
// is enabled.
TEST_F(AutofillManagerTest, DisplayCreditCardSuggestionsWithMatchingTokens) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "pres", "text",
                            &field);
  GetAutofillSuggestions(form, field);

#if defined(OS_ANDROID)
  static const std::string kVisaSuggestion =
      std::string("Visa") + kUTF8MidlineEllipsis + "3456";
#else
  static const std::string kVisaSuggestion = "*3456";
#endif

  external_delegate_->CheckSuggestions(
      kDefaultPageID, Suggestion("Elvis Presley", kVisaSuggestion, kVisaCard,
                                 autofill_manager_->GetPackedCreditCardID(4)));
}

// Verify that typing "lvis" will not match any of the credit card name when
// substring matching is enabled.
TEST_F(AutofillManagerTest, NoCreditCardSuggestionsForNonPrefixTokenMatch) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "lvis", "text",
                            &field);
  GetAutofillSuggestions(form, field);
  // Autocomplete suggestions are queried, but not Autofill.
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form with an impression limit of three and no impressions yet.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_UnmetLimit) {
  // No impressions yet.
  ASSERT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // The mock implementation of ShouldShowSigninPromo() will return true here,
  // creating an impression, and false below, preventing an impression.
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_TRUE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Expect to now have an impression.
  EXPECT_EQ(1, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // No additional impression.
  EXPECT_EQ(1, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form with an impression limit that has been attained already.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_WithAttainedLimit) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // Set the impression count to the same value as the limit.
  autofill_client_.GetPrefs()->SetInteger(
      prefs::kAutofillCreditCardSigninPromoImpressionCount,
      kCreditCardSigninPromoImpressionLimit);

  // Both calls will now return false, regardless of the mock implementation of
  // ShouldShowSigninPromo().
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions stay the same.
  EXPECT_EQ(kCreditCardSigninPromoImpressionLimit,
            autofill_client_.GetPrefs()->GetInteger(
                prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form on a non-secure page.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_NonSecureContext) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  autofill_client_.set_form_origin(form.origin);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // Both calls will now return false, regardless of the mock implementation of
  // ShouldShowSigninPromo().
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions should remain at zero.
  EXPECT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for a credit
// card form targeting a non-secure page.
TEST_F(AutofillManagerTest,
       ShouldShowCreditCardSigninPromo_CreditCardField_NonSecureAction) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);

  // Both calls will now return false, regardless of the mock implementation of
  // ShouldShowSigninPromo().
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(true));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo())
      .WillOnce(testing::Return(false));
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));

  // Number of impressions should remain at zero.
  EXPECT_EQ(0, autofill_client_.GetPrefs()->GetInteger(
                   prefs::kAutofillCreditCardSigninPromoImpressionCount));
}

// Test that ShouldShowCreditCardSigninPromo behaves as expected for an address
// form.
TEST_F(AutofillManagerTest, ShouldShowCreditCardSigninPromo_AddressField) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "", "text", &field);

  // Call will now return false, because it is initiated from an address field.
  EXPECT_CALL(autofill_client_, ShouldShowSigninPromo()).Times(0);
  EXPECT_FALSE(autofill_manager_->ShouldShowCreditCardSigninPromo(form, field));
}

// Verify that typing "S" into the middle name field will match and order middle
// names "Shawn Smith" followed by "Adam Smith" i.e. prefix matched followed by
// substring matched.
TEST_F(AutofillManagerTest,
       DisplaySuggestionsWithPrefixesPrecedeSubstringMatched) {
  // Token matching is currently behind a flag.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnableSuggestionsWithSubstringMatch);

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::unique_ptr<AutofillProfile> profile1 =
      base::MakeUnique<AutofillProfile>();
  profile1->set_guid("00000000-0000-0000-0000-000000000103");
  profile1->SetInfo(NAME_FIRST, ASCIIToUTF16("Robin"), "en-US");
  profile1->SetInfo(NAME_MIDDLE, ASCIIToUTF16("Adam Smith"), "en-US");
  profile1->SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile1->SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                    "en-US");
  autofill_manager_->AddProfile(std::move(profile1));

  std::unique_ptr<AutofillProfile> profile2 =
      base::MakeUnique<AutofillProfile>();
  profile2->set_guid("00000000-0000-0000-0000-000000000124");
  profile2->SetInfo(NAME_FIRST, ASCIIToUTF16("Carl"), "en-US");
  profile2->SetInfo(NAME_MIDDLE, ASCIIToUTF16("Shawn Smith"), "en-US");
  profile2->SetInfo(NAME_LAST, ASCIIToUTF16("Grimes"), "en-US");
  profile2->SetInfo(ADDRESS_HOME_LINE1, ASCIIToUTF16("1234 Smith Blvd."),
                    "en-US");
  autofill_manager_->AddProfile(std::move(profile2));

  FormFieldData field;
  test::CreateTestFormField("Middle Name", "middlename", "S", "text", &field);
  GetAutofillSuggestions(form, field);

  external_delegate_->CheckSuggestions(
      kDefaultPageID,
      Suggestion("Shawn Smith", "1234 Smith Blvd., Carl Shawn Smith Grimes", "",
                 1),
      Suggestion("Adam Smith", "1234 Smith Blvd., Robin Adam Smith Grimes", "",
                 2));
}

TEST_F(AutofillManagerTest, ShouldUploadForm) {
  FormData form;
  form.name = ASCIIToUTF16("TestForm");
  form.origin = GURL("http://example.com/form.html");
  form.action = GURL("http://example.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Name", "name", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Email", "email", "", "text", &field);
  form.fields.push_back(field);

  FormStructure form_structure(form);

  // Has less than 3 fields.
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(form_structure));

  // Has less than 3 fields but has autocomplete attribute.
  form.fields[0].autocomplete_attribute = "given-name";
  FormStructure form_structure_2(form);
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(form_structure_2));

  // Has more than 3 fields, no autocomplete attribute.
  form.fields[0].autocomplete_attribute = "";
  test::CreateTestFormField("Country", "country", "", "text", &field);
  form.fields.push_back(field);
  FormStructure form_structure_3(form);
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(form_structure_3));

  // Has more than 3 fields and at least one autocomplete attribute.
  form.fields[0].autocomplete_attribute = "given-name";
  FormStructure form_structure_4(form);
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(form_structure_4));

  // Is off the record.
  autofill_driver_->SetIsIncognito(true);
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(form_structure_4));

  // Make sure it's reset for the next test case.
  autofill_driver_->SetIsIncognito(false);
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(form_structure_4));

  // Has one field which is a password field.
  form.fields.clear();
  test::CreateTestFormField("Password", "pw", "", "password", &field);
  form.fields.push_back(field);
  FormStructure form_structure_5(form);
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(form_structure_5));

  // Has two fields which are password fields.
  test::CreateTestFormField("New Password", "new_pw", "", "password", &field);
  form.fields.push_back(field);
  FormStructure form_structure_6(form);
  EXPECT_TRUE(autofill_manager_->ShouldUploadForm(form_structure_6));

  // Autofill disabled.
  autofill_manager_->set_autofill_enabled(false);
  EXPECT_FALSE(autofill_manager_->ShouldUploadForm(form_structure_3));
}

// Verify that no suggestions are shown on desktop for non credit card related
// fields if the initiating field has the "autocomplete" attribute set to off.
TEST_F(AutofillManagerTest, DisplaySuggestions_AutocompleteOff_AddressField) {
  // Set up an address form.
  FormData mixed_form;
  mixed_form.name = ASCIIToUTF16("MyForm");
  mixed_form.origin = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Address", "address", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  std::vector<FormData> mixed_forms(1, mixed_form);
  FormsSeen(mixed_forms);

  // Suggestions should not be displayed on desktop for this field.
  GetAutofillSuggestions(mixed_form, mixed_form.fields[0]);
  if (IsDesktopPlatform()) {
    EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
  } else {
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Suggestions should always be displayed for all the other fields.
  for (size_t i = 1U; i < mixed_form.fields.size(); ++i) {
    GetAutofillSuggestions(mixed_form, mixed_form.fields[i]);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Verify that suggestions are shown on desktop for credit card related fields
// even if the initiating field field has the "autocomplete" attribute set to
// off.
TEST_F(AutofillManagerTest,
       DisplaySuggestions_AutocompleteOff_CreditCardField) {
  // Set up a credit card form.
  FormData mixed_form;
  mixed_form.name = ASCIIToUTF16("MyForm");
  mixed_form.origin = GURL("https://myform.com/form.html");
  mixed_form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.should_autocomplete = true;
  mixed_form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccexpiresmonth", "", "text",
                            &field);
  field.should_autocomplete = false;
  mixed_form.fields.push_back(field);
  mixed_form.fields.push_back(field);
  std::vector<FormData> mixed_forms(1, mixed_form);
  FormsSeen(mixed_forms);

  // Suggestions should always be displayed.
  for (const FormFieldData& field : mixed_form.fields) {
    GetAutofillSuggestions(mixed_form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that querying for credit card field suggestions notifies the
// driver of an interaction with a credit card field.
TEST_F(AutofillManagerTest, NotifyDriverOfCreditCardInteraction) {
  // Set up a credit card form.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  field.should_autocomplete = false;
  form.fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  field.should_autocomplete = true;
  form.fields.push_back(field);
  test::CreateTestFormField("Expiration Month", "ccexpiresmonth", "", "text",
                            &field);
  field.should_autocomplete = false;
  form.fields.push_back(field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  EXPECT_FALSE(autofill_driver_->did_interact_with_credit_card_form());

  // The driver should always be notified.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(autofill_driver_->did_interact_with_credit_card_form());
    autofill_driver_->ClearDidInteractWithCreditCardForm();
  }
}

// Tests that a form with server only types is still autofillable if the form
// gets updated in cache.
TEST_F(AutofillManagerTest, DisplaySuggestionsForUpdatedServerTypedForm) {
  // Create a form with unknown heuristic fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("Field 1", "field1", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 2", "field2", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Field 3", "field3", "", "text", &field);
  form.fields.push_back(field);

  auto form_structure = base::MakeUnique<TestFormStructure>(form);
  form_structure->DetermineHeuristicTypes(nullptr /* ukm_recorder */);
  // Make sure the form can not be autofilled now.
  ASSERT_EQ(0u, form_structure->autofill_count());
  for (size_t idx = 0; idx < form_structure->field_count(); ++idx) {
    ASSERT_EQ(UNKNOWN_TYPE, form_structure->field(idx)->heuristic_type());
  }

  // Prepare and set known server fields.
  const std::vector<ServerFieldType> heuristic_types(form.fields.size(),
                                                     UNKNOWN_TYPE);
  const std::vector<ServerFieldType> server_types{NAME_FIRST, NAME_MIDDLE,
                                                  NAME_LAST};
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenForm(std::move(form_structure));

  // Make sure the form can be autofilled.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    ASSERT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify one of the fields in the original form.
  form.fields[0].css_classes += ASCIIToUTF16("a");

  // Expect the form still can be autofilled.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }

  // Modify form action URL. This can happen on in-page navitaion if the form
  // doesn't have an actual action (attribute is empty).
  form.action = net::AppendQueryParameter(form.action, "arg", "value");

  // Expect the form still can be autofilled.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Tests that a form with <select> field is accepted if <option> value (not
// content) is quite long. Some websites use value to propagate long JSON to
// JS-backed logic.
TEST_F(AutofillManagerTest, FormWithLongOptionValuesIsAcceptable) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  FormFieldData field;
  test::CreateTestFormField("First name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  test::CreateTestFormField("Last name", "lastname", "", "text", &field);
  form.fields.push_back(field);

  // Prepare <select> field with long <option> values.
  const size_t kOptionValueLength = 10240;
  const std::string long_string(kOptionValueLength, 'a');
  const std::vector<const char*> values(3, long_string.c_str());
  const std::vector<const char*> contents{"A", "B", "C"};
  test::CreateTestSelectField("Country", "country", "", values, contents,
                              values.size(), &field);
  form.fields.push_back(field);

  FormsSeen({form});

  // Suggestions should be displayed.
  for (const FormFieldData& field : form.fields) {
    GetAutofillSuggestions(form, field);
    EXPECT_TRUE(external_delegate_->on_suggestions_returned_seen());
  }
}

// Test that a sign-in form submission sends an upload with types matching the
// fields.
TEST_F(AutofillManagerTest, SignInFormSubmission_Upload) {
  // Set up our form data (it's already filled out with user data).
  FormData form;
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");

  std::vector<ServerFieldTypeSet> expected_types;
  ServerFieldTypeSet types;

  FormFieldData field;
  test::CreateTestFormField("Email", "email", "theking@gmail.com", "text",
                            &field);
  form.fields.push_back(field);
  types.insert(EMAIL_ADDRESS);
  expected_types.push_back(types);

  test::CreateTestFormField("Password", "pw", "", "password", &field);
  form.fields.push_back(field);
  FormsSeen(std::vector<FormData>(1, form));
  types.clear();
  types.insert(PASSWORD);
  expected_types.push_back(types);

  FormsSeen({form});

  // We will expect these types in the upload and no observed submission. (the
  // callback initiated by WaitForAsyncUploadProcess checks these expectations.)
  autofill_manager_->set_expected_submitted_field_types(expected_types);
  autofill_manager_->set_expected_observed_submission(true);
  autofill_manager_->set_call_parent_upload_form_data(true);
  autofill_manager_->ResetRunLoop();

  std::unique_ptr<FormStructure> form_structure(new FormStructure(form));
  form_structure->set_is_signin_upload(true);
  form_structure->set_form_parsed_timestamp(base::TimeTicks::Now());
  form_structure->field(1)->set_possible_types({autofill::PASSWORD});

  std::string signature = form_structure->FormSignatureAsStr();

  ServerFieldTypeSet uploaded_available_types;
  EXPECT_CALL(*download_manager_,
              StartUploadRequest(_, false, _, std::string(), true))
      .WillOnce(DoAll(SaveArg<2>(&uploaded_available_types), Return(true)));
  autofill_manager_->StartUploadProcess(std::move(form_structure),
                                        base::TimeTicks::Now(), true);

  // Wait for upload to complete (will check expected types as well).
  autofill_manager_->WaitForAsyncUploadProcess();

  EXPECT_EQ(signature, autofill_manager_->GetSubmittedFormSignature());
  EXPECT_NE(uploaded_available_types.end(),
            uploaded_available_types.find(autofill::PASSWORD));
}

}  // namespace autofill
