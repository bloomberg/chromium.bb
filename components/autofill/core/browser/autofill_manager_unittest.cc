// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/tuple.h"
#include "components/autofill/core/browser/autocomplete_history_manager.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/popup_item_ids.h"
#include "components/autofill/core/browser/test_autofill_client.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_external_delegate.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"
#include "url/gurl.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;
using testing::_;

namespace autofill {

typedef PersonalDataManager::GUIDPair GUIDPair;

namespace {

const int kDefaultPageID = 137;

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() : PersonalDataManager("en-US") {
    CreateTestAutofillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  using PersonalDataManager::set_database;
  using PersonalDataManager::SetPrefService;

  MOCK_METHOD1(SaveImportedProfile, std::string(const AutofillProfile&));

  AutofillProfile* GetProfileWithGUID(const char* guid) {
    for (std::vector<AutofillProfile *>::iterator it = web_profiles_.begin();
         it != web_profiles_.end(); ++it) {
      if (!(*it)->guid().compare(guid))
        return *it;
    }
    return NULL;
  }

  CreditCard* GetCreditCardWithGUID(const char* guid) {
    for (std::vector<CreditCard *>::iterator it = credit_cards_.begin();
         it != credit_cards_.end(); ++it){
      if (!(*it)->guid().compare(guid))
        return *it;
    }
    return NULL;
  }

  void AddProfile(AutofillProfile* profile) {
    web_profiles_.push_back(profile);
  }

  void AddCreditCard(CreditCard* credit_card) {
    credit_cards_.push_back(credit_card);
  }

  virtual void RemoveByGUID(const std::string& guid) OVERRIDE {
    CreditCard* credit_card = GetCreditCardWithGUID(guid.c_str());
    if (credit_card) {
      credit_cards_.erase(
          std::remove(credit_cards_.begin(), credit_cards_.end(), credit_card),
          credit_cards_.end());
    }

    AutofillProfile* profile = GetProfileWithGUID(guid.c_str());
    if (profile) {
      web_profiles_.erase(
          std::remove(web_profiles_.begin(), web_profiles_.end(), profile),
          web_profiles_.end());
    }
  }

  // Do nothing (auxiliary profiles will be created in
  // CreateTestAuxiliaryProfile).
  virtual void LoadAuxiliaryProfiles(bool record_metrics) const OVERRIDE {}

  void ClearAutofillProfiles() {
    web_profiles_.clear();
  }

  void ClearCreditCards() {
    credit_cards_.clear();
  }

  void CreateTestAuxiliaryProfiles() {
    CreateTestAutofillProfiles(&auxiliary_profiles_);
  }

  void CreateTestCreditCardsYearAndMonth(const char* year, const char* month) {
    ClearCreditCards();
    CreditCard* credit_card = new CreditCard;
    test::SetCreditCardInfo(credit_card, "Miku Hatsune",
                            "4234567890654321", // Visa
                            month, year);
    credit_card->set_guid("00000000-0000-0000-0000-000000000007");
    credit_cards_.push_back(credit_card);
  }

 private:
  void CreateTestAutofillProfiles(ScopedVector<AutofillProfile>* profiles) {
    AutofillProfile* profile = new AutofillProfile;
    test::SetProfileInfo(profile, "Elvis", "Aaron",
                         "Presley", "theking@gmail.com", "RCA",
                         "3734 Elvis Presley Blvd.", "Apt. 10",
                         "Memphis", "Tennessee", "38116", "US",
                         "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    test::SetProfileInfo(profile, "Charles", "Hardin",
                         "Holley", "buddy@gmail.com", "Decca",
                         "123 Apple St.", "unit 6", "Lubbock",
                         "Texas", "79401", "US", "23456789012");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    test::SetProfileInfo(
        profile, "", "", "", "", "", "", "", "", "", "", "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    test::SetCreditCardInfo(credit_card, "Elvis Presley",
                            "4234 5678 9012 3456",  // Visa
                            "04", "2012");
    credit_card->set_guid("00000000-0000-0000-0000-000000000004");
    credit_cards->push_back(credit_card);

    credit_card = new CreditCard;
    test::SetCreditCardInfo(credit_card, "Buddy Holly",
                            "5187654321098765",  // Mastercard
                            "10", "2014");
    credit_card->set_guid("00000000-0000-0000-0000-000000000005");
    credit_cards->push_back(credit_card);

    credit_card = new CreditCard;
    test::SetCreditCardInfo(credit_card, "", "", "", "");
    credit_card->set_guid("00000000-0000-0000-0000-000000000006");
    credit_cards->push_back(credit_card);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

// Populates |form| with data corresponding to a simple credit card form.
// Note that this actually appends fields to the form data, which can be useful
// for building up more complex test forms.
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
  form->user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form->fields.push_back(field);
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  if (use_month_type) {
    test::CreateTestFormField(
        "Expiration Date", "ccmonth", "", "month", &field);
    form->fields.push_back(field);
  } else {
    test::CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    test::CreateTestFormField("", "ccyear", "", "text", &field);
    form->fields.push_back(field);
  }
}

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
  const size_t kCreditCardFormSize = use_month_type ? 3 : 4;

  EXPECT_EQ(expected_page_id, page_id);
  EXPECT_EQ(ASCIIToUTF16("MyForm"), filled_form.name);
  if (has_credit_card_fields) {
    EXPECT_EQ(GURL("https://myform.com/form.html"), filled_form.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), filled_form.action);
  } else {
    EXPECT_EQ(GURL("http://myform.com/form.html"), filled_form.origin);
    EXPECT_EQ(GURL("http://myform.com/submit.html"), filled_form.action);
  }
  EXPECT_TRUE(filled_form.user_submitted);

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
    ExpectFilledField("City", "city", city, "text",
                      filled_form.fields[5]);
    ExpectFilledField("State", "state", state, "text",
                      filled_form.fields[6]);
    ExpectFilledField("Postal Code", "zipcode", postal_code, "text",
                      filled_form.fields[7]);
    ExpectFilledField("Country", "country", country, "text",
                      filled_form.fields[8]);
    ExpectFilledField("Phone Number", "phonenumber", phone, "tel",
                      filled_form.fields[9]);
    ExpectFilledField("Email", "email", email, "email",
                      filled_form.fields[10]);
  }

  if (has_credit_card_fields) {
    size_t offset = has_address_fields? kAddressFormSize : 0;
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
  ExpectFilledForm(page_id, filled_form, expected_page_id,
                   "", "", "", "", "", "", "", "", "", "", "",
                   "Elvis Presley", "4234567890123456", "04", "2012",
                   has_address_fields, true, false);
}

void ExpectFilledCreditCardYearMonthWithYearMonth(int page_id,
                                                  const FormData& filled_form,
                                                  int expected_page_id,
                                                  bool has_address_fields,
                                                  const char* year,
                                                  const char* month) {
  ExpectFilledForm(page_id, filled_form, expected_page_id,
                   "", "", "", "", "", "", "", "", "", "", "",
                   "Miku Hatsune", "4234567890654321", month, year,
                   has_address_fields, true, true);
}

class MockAutocompleteHistoryManager : public AutocompleteHistoryManager {
 public:
  MockAutocompleteHistoryManager(AutofillDriver* driver, AutofillClient* client)
      : AutocompleteHistoryManager(driver, client) {}

  MOCK_METHOD1(OnFormSubmitted, void(const FormData& form));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutocompleteHistoryManager);
};

class MockAutofillDriver : public TestAutofillDriver {
 public:
  MockAutofillDriver() {}

  // Mock methods to enable testability.
  MOCK_METHOD3(SendFormDataToRenderer, void(int query_id,
                                            RendererFormDataAction action,
                                            const FormData& data));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillDriver);
};

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(AutofillDriver* driver,
                      autofill::AutofillClient* client,
                      TestPersonalDataManager* personal_data)
      : AutofillManager(driver, client, personal_data),
        personal_data_(personal_data),
        autofill_enabled_(true) {}
  virtual ~TestAutofillManager() {}

  virtual bool IsAutofillEnabled() const OVERRIDE { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  void set_expected_submitted_field_types(
      const std::vector<ServerFieldTypeSet>& expected_types) {
    expected_submitted_field_types_ = expected_types;
  }

  virtual void UploadFormDataAsyncCallback(
      const FormStructure* submitted_form,
      const base::TimeTicks& load_time,
      const base::TimeTicks& interaction_time,
      const base::TimeTicks& submission_time) OVERRIDE {
    run_loop_->Quit();

    // If we have expected field types set, make sure they match.
    if (!expected_submitted_field_types_.empty()) {
      ASSERT_EQ(expected_submitted_field_types_.size(),
                submitted_form->field_count());
      for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
        SCOPED_TRACE(
            base::StringPrintf(
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

    AutofillManager::UploadFormDataAsyncCallback(submitted_form,
                                                 load_time,
                                                 interaction_time,
                                                 submission_time);
  }

  // Resets the run loop so that it can wait for an asynchronous form
  // submission to complete.
  void ResetRunLoop() { run_loop_.reset(new base::RunLoop()); }

  // Wait for the asynchronous OnFormSubmitted() call to complete.
  void WaitForAsyncFormSubmit() { run_loop_->Run(); }

  virtual void UploadFormData(const FormStructure& submitted_form) OVERRIDE {
    submitted_form_signature_ = submitted_form.FormSignature();
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

  void AddProfile(AutofillProfile* profile) {
    personal_data_->AddProfile(profile);
  }

  void AddCreditCard(CreditCard* credit_card) {
    personal_data_->AddCreditCard(credit_card);
  }

  int GetPackedCreditCardID(int credit_card_id) {
    std::string credit_card_guid =
        base::StringPrintf("00000000-0000-0000-0000-%012d", credit_card_id);

    return PackGUIDs(GUIDPair(credit_card_guid, 0), GUIDPair(std::string(), 0));
  }

  void AddSeenForm(FormStructure* form) {
    form_structures()->push_back(form);
  }

  void ClearFormStructures() {
    form_structures()->clear();
  }

 private:
  // Weak reference.
  TestPersonalDataManager* personal_data_;

  bool autofill_enabled_;

  scoped_ptr<base::RunLoop> run_loop_;

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
  virtual ~TestAutofillExternalDelegate() {}

  virtual void OnQuery(int query_id,
                       const FormData& form,
                       const FormFieldData& field,
                       const gfx::RectF& bounds,
                       bool display_warning) OVERRIDE {
    on_query_seen_ = true;
    on_suggestions_returned_seen_ = false;
  }

  virtual void OnSuggestionsReturned(
      int query_id,
      const std::vector<base::string16>& autofill_values,
      const std::vector<base::string16>& autofill_labels,
      const std::vector<base::string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE {
    on_suggestions_returned_seen_ = true;

    query_id_ = query_id;
    autofill_values_ = autofill_values;
    autofill_labels_ = autofill_labels;
    autofill_icons_ = autofill_icons;
    autofill_unique_ids_ = autofill_unique_ids;
  }

  void CheckSuggestions(int expected_page_id,
                        size_t expected_num_suggestions,
                        const base::string16 expected_values[],
                        const base::string16 expected_labels[],
                        const base::string16 expected_icons[],
                        const int expected_unique_ids[]) {
    // Ensure that these results are from the most recent query.
    EXPECT_TRUE(on_suggestions_returned_seen_);

    EXPECT_EQ(expected_page_id, query_id_);
    ASSERT_EQ(expected_num_suggestions, autofill_values_.size());
    ASSERT_EQ(expected_num_suggestions, autofill_labels_.size());
    ASSERT_EQ(expected_num_suggestions, autofill_icons_.size());
    ASSERT_EQ(expected_num_suggestions, autofill_unique_ids_.size());
    for (size_t i = 0; i < expected_num_suggestions; ++i) {
      SCOPED_TRACE(base::StringPrintf("i: %" PRIuS, i));
      EXPECT_EQ(expected_values[i], autofill_values_[i]);
      EXPECT_EQ(expected_labels[i], autofill_labels_[i]);
      EXPECT_EQ(expected_icons[i], autofill_icons_[i]);
      EXPECT_EQ(expected_unique_ids[i], autofill_unique_ids_[i]);
    }
  }

  bool on_query_seen() const {
    return on_query_seen_;
  }

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
  std::vector<base::string16> autofill_values_;
  std::vector<base::string16> autofill_labels_;
  std::vector<base::string16> autofill_icons_;
  std::vector<int> autofill_unique_ids_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillExternalDelegate);
};

}  // namespace

class AutofillManagerTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    autofill_client_.SetPrefs(test::PrefServiceForTesting());
    personal_data_.set_database(autofill_client_.GetDatabase());
    personal_data_.SetPrefService(autofill_client_.GetPrefs());
    autofill_driver_.reset(new MockAutofillDriver());
    autofill_manager_.reset(new TestAutofillManager(
        autofill_driver_.get(), &autofill_client_, &personal_data_));

    external_delegate_.reset(new TestAutofillExternalDelegate(
        autofill_manager_.get(),
        autofill_driver_.get()));
    autofill_manager_->SetExternalDelegate(external_delegate_.get());
  }

  virtual void TearDown() OVERRIDE {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed.
    autofill_manager_.reset();
    autofill_driver_.reset();

    // Remove the AutofillWebDataService so TestPersonalDataManager does not
    // need to care about removing self as an observer in destruction.
    personal_data_.set_database(scoped_refptr<AutofillWebDataService>(NULL));
    personal_data_.SetPrefService(NULL);
  }

  void GetAutofillSuggestions(int query_id,
                              const FormData& form,
                              const FormFieldData& field) {
    autofill_manager_->OnQueryFormFieldAutofill(query_id,
                                                form,
                                                field,
                                                gfx::Rect(),
                                                false);
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
    if (autofill_manager_->OnFormSubmitted(form, base::TimeTicks::Now()))
      autofill_manager_->WaitForAsyncFormSubmit();
  }

  void FillAutofillFormData(int query_id,
                            const FormData& form,
                            const FormFieldData& field,
                            int unique_id) {
    autofill_manager_->FillOrPreviewForm(
        AutofillDriver::FORM_DATA_ACTION_FILL, query_id, form, field,
        unique_id);
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
    EXPECT_CALL(*autofill_driver_, SendFormDataToRenderer(_, _, _)).
        WillOnce((DoAll(testing::SaveArg<0>(response_query_id),
                        testing::SaveArg<2>(response_data))));
    FillAutofillFormData(input_query_id, input_form, input_field, unique_id);
  }

  int PackGUIDs(const GUIDPair& cc_guid, const GUIDPair& profile_guid) const {
    return autofill_manager_->PackGUIDs(cc_guid, profile_guid);
  }

 protected:
  base::MessageLoop message_loop_;
  TestAutofillClient autofill_client_;
  scoped_ptr<MockAutofillDriver> autofill_driver_;
  scoped_ptr<TestAutofillManager> autofill_manager_;
  scoped_ptr<TestAutofillExternalDelegate> external_delegate_;
  TestPersonalDataManager personal_data_;
};

class TestFormStructure : public FormStructure {
 public:
  explicit TestFormStructure(const FormData& form)
      : FormStructure(form) {}
  virtual ~TestFormStructure() {}

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

// Test that we return all address profile suggestions when all form fields are
// empty.
TEST_F(AutofillManagerTest, GetProfileSuggestionsEmptyValue) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  // Inferred labels include full first relevant field, which in this case is
  // the address line 1.
  base::string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  base::string16 expected_icons[] = {base::string16(), base::string16()};
  int expected_unique_ids[] = {1, 2};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return only matching address profile suggestions when the
// selected form field has been partially filled out.
TEST_F(AutofillManagerTest, GetProfileSuggestionsMatchCharacter) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("First Name", "firstname", "E", "text",&field);
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {ASCIIToUTF16("Elvis")};
  base::string16 expected_labels[] = {ASCIIToUTF16("3734 Elvis Presley Blvd.")};
  base::string16 expected_icons[] = {base::string16()};
  int expected_unique_ids[] = {1};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return no suggestions when the form has no relevant fields.
TEST_F(AutofillManagerTest, GetProfileSuggestionsUnknownFields) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  test::CreateTestFormField("Username", "username", "", "text",&field);
  form.fields.push_back(field);
  test::CreateTestFormField("Password", "password", "", "password",&field);
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
TEST_F(AutofillManagerTest, GetProfileSuggestionsWithDuplicates) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Add a duplicate profile.
  AutofillProfile* duplicate_profile =
      new AutofillProfile(
          *(autofill_manager_->GetProfileWithGUID(
              "00000000-0000-0000-0000-000000000001")));
  autofill_manager_->AddProfile(duplicate_profile);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  base::string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  base::string16 expected_icons[] = {base::string16(), base::string16()};
  int expected_unique_ids[] = {1, 2};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return no suggestions when autofill is disabled.
TEST_F(AutofillManagerTest, GetProfileSuggestionsAutofillDisabledByUser) {
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
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsEmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("************3456"),
    ASCIIToUTF16("************8765")
  };
  base::string16 expected_labels[] = { ASCIIToUTF16("04/12"),
                                       ASCIIToUTF16("10/14")};
  base::string16 expected_icons[] = {
    ASCIIToUTF16(kVisaCard),
    ASCIIToUTF16(kMasterCard)
  };
  int expected_unique_ids[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5)
  };
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return only matching credit card profile suggestions when the
// selected form field has been partially filled out.
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsMatchCharacter) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  test::CreateTestFormField("Card Number", "cardnumber", "4", "text", &field);
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {ASCIIToUTF16("************3456")};
  base::string16 expected_labels[] = {ASCIIToUTF16("04/12")};
  base::string16 expected_icons[] = {ASCIIToUTF16(kVisaCard)};
  int expected_unique_ids[] = {autofill_manager_->GetPackedCreditCardID(4)};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return credit card profile suggestions when the selected form
// field is not the credit card number field.
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsNonCCNumber) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis Presley"),
    ASCIIToUTF16("Buddy Holly")
  };
  base::string16 expected_labels[] = { ASCIIToUTF16("*3456"),
                                       ASCIIToUTF16("*8765") };
  base::string16 expected_icons[] = {
    ASCIIToUTF16(kVisaCard),
    ASCIIToUTF16(kMasterCard)
  };
  int expected_unique_ids[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5)
  };
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return a warning explaining that credit card profile suggestions
// are unavailable when the form is not https.
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsNonHTTPS) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION)
  };
  base::string16 expected_labels[] = {base::string16()};
  base::string16 expected_icons[] = {base::string16()};
  int expected_unique_ids[] = {-1};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);

  // Now add some Autocomplete suggestions. We should show the autocomplete
  // suggestions and the warning.
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  std::vector<base::string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  base::string16 expected_values2[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
    ASCIIToUTF16("Jay"),
    ASCIIToUTF16("Jason")
  };
  base::string16 expected_labels2[] = { base::string16(), base::string16(),
                                        base::string16() };
  base::string16 expected_icons2[] = { base::string16(), base::string16(),
                                       base::string16() };
  int expected_unique_ids2[] = {-1, 0, 0};
  external_delegate_->CheckSuggestions(
      kPageID2, arraysize(expected_values2), expected_values2,
      expected_labels2, expected_icons2, expected_unique_ids2);

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we return all credit card suggestions in the case that two cards
// have the same obfuscated number.
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsRepeatedObfuscatedNumber) {
  // Add a credit card with the same obfuscated number as Elvis's.
  // |credit_card| will be owned by the mock PersonalDataManager.
  CreditCard* credit_card = new CreditCard;
  test::SetCreditCardInfo(credit_card, "Elvis Presley",
                          "5231567890123456",  // Mastercard
                          "05", "2012");
  credit_card->set_guid("00000000-0000-0000-0000-000000000007");
  autofill_manager_->AddCreditCard(credit_card);

  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[1];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("************3456"),
    ASCIIToUTF16("************8765"),
    ASCIIToUTF16("************3456")
  };
  base::string16 expected_labels[] = {
    ASCIIToUTF16("04/12"),
    ASCIIToUTF16("10/14"),
    ASCIIToUTF16("05/12"),
  };
  base::string16 expected_icons[] = {
    ASCIIToUTF16(kVisaCard),
    ASCIIToUTF16(kMasterCard),
    ASCIIToUTF16(kMasterCard)
  };
  int expected_unique_ids[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5),
    autofill_manager_->GetPackedCreditCardID(7)
  };
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
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

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right address suggestions to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  base::string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  base::string16 expected_icons[] = {base::string16(), base::string16()};
  int expected_unique_ids[] = {1, 2};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);

  const int kPageID2 = 2;
  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  GetAutofillSuggestions(kPageID2, form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the credit card suggestions to the external delegate.
  base::string16 expected_values2[] = {
    ASCIIToUTF16("************3456"),
    ASCIIToUTF16("************8765")
  };
  base::string16 expected_labels2[] = { ASCIIToUTF16("04/12"),
                                        ASCIIToUTF16("10/14")};
  base::string16 expected_icons2[] = {
    ASCIIToUTF16(kVisaCard),
    ASCIIToUTF16(kMasterCard)
  };
  int expected_unique_ids2[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5)
  };
  external_delegate_->CheckSuggestions(
      kPageID2, arraysize(expected_values2), expected_values2,
      expected_labels2, expected_icons2, expected_unique_ids2);
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

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right suggestions to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  base::string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  base::string16 expected_icons[] = {base::string16(), base::string16()};
  int expected_unique_ids[] = {1, 2};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);

  test::CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values2[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION)
  };
  base::string16 expected_labels2[] = {base::string16()};
  base::string16 expected_icons2[] = {base::string16()};
  int expected_unique_ids2[] = {-1};
  external_delegate_->CheckSuggestions(
      kPageID2, arraysize(expected_values2), expected_values2,
      expected_labels2, expected_icons2, expected_unique_ids2);

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(external_delegate_->on_suggestions_returned_seen());
}

// Test that we correctly combine autofill and autocomplete suggestions.
TEST_F(AutofillManagerTest, GetCombinedAutofillAndAutocompleteSuggestions) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<base::string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  // This suggestion is a duplicate, and should be trimmed.
  suggestions.push_back(ASCIIToUTF16("Elvis"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles"),
    ASCIIToUTF16("Jay"),
    ASCIIToUTF16("Jason")
  };
  base::string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St."),
    base::string16(),
    base::string16()
  };
  base::string16 expected_icons[] = { base::string16(), base::string16(),
                                      base::string16(), base::string16()};
  int expected_unique_ids[] = {1, 2, 0, 0};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
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

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  base::string16 expected_labels[] = {base::string16(), base::string16()};
  base::string16 expected_icons[] = {base::string16(), base::string16()};
  int expected_unique_ids[] = {1, 2};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
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
  base::string16 expected_values[] = {
    ASCIIToUTF16("one"),
    ASCIIToUTF16("two")
  };
  base::string16 expected_labels[] = {base::string16(), base::string16()};
  base::string16 expected_icons[] = {base::string16(), base::string16()};
  int expected_unique_ids[] = {0, 0};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
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
  AutofillProfile* profile = new AutofillProfile;
  test::SetProfileInfo(
      profile, "Elvis", "", "", "", "", "", "", "", "", "", "", "");
  profile->set_guid("00000000-0000-0000-0000-000000000101");
  autofill_manager_->AddProfile(profile);

  FormFieldData& field = form.fields[0];
  field.is_autofilled = true;
  field.value = ASCIIToUTF16("Elvis");
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = { ASCIIToUTF16("Elvis") };
  base::string16 expected_labels[] = { base::string16() };
  base::string16 expected_icons[] = { base::string16() };
  int expected_unique_ids[] = { 1 };
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that a non-default value is suggested for multi-valued profile, on an
// unfilled form.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForMultiValuedProfileUnfilled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile* profile = new AutofillProfile;
  test::SetProfileInfo(profile, "Elvis", "", "Presley", "me@x.com", "",
                       "", "", "", "", "", "", "");
  profile->set_guid("00000000-0000-0000-0000-000000000101");
  std::vector<base::string16> multi_values(2);
  multi_values[0] = ASCIIToUTF16("Elvis");
  multi_values[1] = ASCIIToUTF16("Elena");
  profile->SetRawMultiInfo(NAME_FIRST, multi_values);
  multi_values[0] = ASCIIToUTF16("Presley");
  multi_values[1] = ASCIIToUTF16("Love");
  profile->SetRawMultiInfo(NAME_LAST, multi_values);
  personal_data_.ClearAutofillProfiles();
  autofill_manager_->AddProfile(profile);

  {
    // Get the first name field.
    // Start out with "E", hoping for either "Elvis" or "Elena.
    FormFieldData& field = form.fields[0];
    field.value = ASCIIToUTF16("E");
    field.is_autofilled = false;
    GetAutofillSuggestions(form, field);

    // Trigger the |Send|.
    AutocompleteSuggestionsReturned(std::vector<base::string16>());

    // Test that we sent the right values to the external delegate.
    base::string16 expected_values[] = {
      ASCIIToUTF16("Elvis"),
      ASCIIToUTF16("Elena")
    };
    base::string16 expected_labels[] = {
      ASCIIToUTF16("me@x.com"),
      ASCIIToUTF16("me@x.com")
    };
    base::string16 expected_icons[] = { base::string16(), base::string16() };
    int expected_unique_ids[] = { 1, 2 };
    external_delegate_->CheckSuggestions(
        kDefaultPageID, arraysize(expected_values), expected_values,
        expected_labels, expected_icons, expected_unique_ids);
  }

  {
    // Get the first name field.
    // This time, start out with "Ele", hoping for "Elena".
    FormFieldData& field = form.fields[0];
    field.value = ASCIIToUTF16("Ele");
    field.is_autofilled = false;
    GetAutofillSuggestions(form, field);

    // Trigger the |Send|.
    AutocompleteSuggestionsReturned(std::vector<base::string16>());

    // Test that we sent the right values to the external delegate.
    base::string16 expected_values[] = { ASCIIToUTF16("Elena") };
    base::string16 expected_labels[] = { ASCIIToUTF16("me@x.com") };
    base::string16 expected_icons[] = { base::string16() };
    int expected_unique_ids[] = { 2 };
    external_delegate_->CheckSuggestions(
        kDefaultPageID, arraysize(expected_values), expected_values,
        expected_labels, expected_icons, expected_unique_ids);
  }
}

// Test that all values are suggested for multi-valued profile, on a filled
// form.  This is the per-field "override" case.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForMultiValuedProfileFilled) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile* profile = new AutofillProfile;
  profile->set_guid("00000000-0000-0000-0000-000000000102");
  std::vector<base::string16> multi_values(3);
  multi_values[0] = ASCIIToUTF16("Travis");
  multi_values[1] = ASCIIToUTF16("Cynthia");
  multi_values[2] = ASCIIToUTF16("Zac");
  profile->SetRawMultiInfo(NAME_FIRST, multi_values);
  multi_values[0] = ASCIIToUTF16("Smith");
  multi_values[1] = ASCIIToUTF16("Love");
  multi_values[2] = ASCIIToUTF16("Mango");
  profile->SetRawMultiInfo(NAME_LAST, multi_values);
  autofill_manager_->AddProfile(profile);

  // Get the first name field.  And start out with "Travis", hoping for all the
  // multi-valued variants as suggestions.
  FormFieldData& field = form.fields[0];
  field.value = ASCIIToUTF16("Travis");
  field.is_autofilled = true;
  GetAutofillSuggestions(form, field);

  // Trigger the |Send|.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("Travis"),
    ASCIIToUTF16("Cynthia"),
    ASCIIToUTF16("Zac")
  };
  base::string16 expected_labels[] = { base::string16(), base::string16(),
                                       base::string16() };
  base::string16 expected_icons[] = { base::string16(), base::string16(),
                                      base::string16() };
  int expected_unique_ids[] = { 1, 2, 3 };
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

TEST_F(AutofillManagerTest, GetProfileSuggestionsFancyPhone) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile* profile = new AutofillProfile;
  profile->set_guid("00000000-0000-0000-0000-000000000103");
  profile->SetInfo(AutofillType(NAME_FULL), ASCIIToUTF16("Natty Bumppo"),
                   "en-US");
  profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                      ASCIIToUTF16("1800PRAIRIE"));
  autofill_manager_->AddProfile(profile);

  const FormFieldData& field = form.fields[9];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<base::string16>());

  // Test that we sent the right values to the external delegate.
  base::string16 expected_values[] = {
    ASCIIToUTF16("12345678901"),
    ASCIIToUTF16("23456789012"),
    ASCIIToUTF16("18007724743"),  // 1800PRAIRIE
  };
  // Inferred labels include full first relevant field, which in this case is
  // the address line 1.
  base::string16 expected_labels[] = {
    ASCIIToUTF16("Elvis Aaron Presley"),
    ASCIIToUTF16("Charles Hardin Holley"),
    ASCIIToUTF16("Natty Bumppo"),
  };
  base::string16 expected_icons[] = { base::string16(), base::string16(),
                                      base::string16()};
  int expected_unique_ids[] = {1, 2, 3};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we correctly fill an address form.
TEST_F(AutofillManagerTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);
}

// Test that we correctly fill an address form from an auxiliary profile.
TEST_F(AutofillManagerTest, FillAddressFormFromAuxiliaryProfile) {
  personal_data_.ClearAutofillProfiles();
#if defined(OS_MACOSX) && !defined(OS_IOS)
  autofill_client_.GetPrefs()->SetBoolean(
      ::autofill::prefs::kAutofillUseMacAddressBook, true);
#else
  autofill_client_.GetPrefs()->SetBoolean(
      ::autofill::prefs::kAutofillAuxiliaryProfilesEnabled, true);
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

  personal_data_.CreateTestAuxiliaryProfiles();

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);
}

// Test that we correctly fill a credit card form.
TEST_F(AutofillManagerTest, FillCreditCardForm) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000004", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
      PackGUIDs(guid, empty), &response_page_id, &response_data);
  ExpectFilledCreditCardFormElvis(
      response_page_id, response_data, kDefaultPageID, false);
}

// Test that we correctly fill a credit card form with month input type.
// 1. year empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardFormNoYearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("", "");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000007", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
      PackGUIDs(guid, empty), &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
      kDefaultPageID, false, "", "");
}


// Test that we correctly fill a credit card form with month input type.
// 2. year empty, month non-empty
TEST_F(AutofillManagerTest, FillCreditCardFormNoYearMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("", "04");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000007", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
      PackGUIDs(guid, empty), &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
      kDefaultPageID, false, "", "04");
}

// Test that we correctly fill a credit card form with month input type.
// 3. year non-empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardFormYearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.CreateTestCreditCardsYearAndMonth("2012", "");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000007", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
      PackGUIDs(guid, empty), &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
      kDefaultPageID, false, "2012", "");
}

// Test that we correctly fill a credit card form with month input type.
// 4. year non-empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardFormYearMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  personal_data_.ClearCreditCards();
  personal_data_.CreateTestCreditCardsYearAndMonth("2012", "04");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000007", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
      PackGUIDs(guid, empty), &response_page_id, &response_data);
  ExpectFilledCreditCardYearMonthWithYearMonth(response_page_id, response_data,
      kDefaultPageID, false, "2012", "04");
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
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  {
    SCOPED_TRACE("Address");
    FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
        PackGUIDs(empty, guid), &response_page_id, &response_data);
    ExpectFilledAddressFormElvis(
        response_page_id, response_data, kDefaultPageID, true);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000004", 0);
  response_page_id = 0;
  {
    FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
        PackGUIDs(guid2, empty), &response_page_id, &response_data);
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(
        response_page_id, response_data, kPageID2, true);
  }
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
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 1");
    // The second address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2*kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(base::string16(), response_data.fields[i].value);
    }

    // The first address section should be filled with Elvis's data.
    response_data.fields.resize(kAddressFormSize);
    ExpectFilledAddressFormElvis(
        response_page_id, response_data, kDefaultPageID, false);
  }

  // Fill the second section, with the initiating field somewhere in the middle
  // of the section.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000001", 0);
  ASSERT_LT(9U, kAddressFormSize);
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID2, form, form.fields[kAddressFormSize + 9],
      PackGUIDs(empty, guid2), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address 2");
    ASSERT_EQ(response_data.fields.size(), form.fields.size());

    // The first address section should be empty.
    ASSERT_EQ(response_data.fields.size(), 2*kAddressFormSize);
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
    ExpectFilledAddressFormElvis(
        response_page_id, secondSection, kPageID2, false);
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
  form.user_submitted = true;

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
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[1],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(kDefaultPageID, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    EXPECT_TRUE(response_data.user_submitted);
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
  GUIDPair guid2("00000000-0000-0000-0000-000000000001", 0);
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields[0],
      PackGUIDs(empty, guid2), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(kPageID2, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    EXPECT_TRUE(response_data.user_submitted);
    ASSERT_EQ(11U, response_data.fields.size());

    ExpectFilledField("", "country", "US", "text",
                      response_data.fields[0]);
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
  GUIDPair guid3("00000000-0000-0000-0000-000000000004", 0);
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(
      kPageID3, form, form.fields[form.fields.size() - 2],
      PackGUIDs(guid3, empty), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(kPageID3, response_page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), response_data.name);
    EXPECT_EQ(GURL("https://myform.com/form.html"), response_data.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), response_data.action);
    EXPECT_TRUE(response_data.user_submitted);
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
    ExpectFilledField("", "ccexp", "04/2012", "text", response_data.fields[9]);
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
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);

  // The second email address should be filled.
  EXPECT_EQ(ASCIIToUTF16("theking@gmail.com"),
            response_data.fields.back().value);

  // The remainder of the form should be filled as usual.
  response_data.fields.pop_back();
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);
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
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, *form.fields.begin(),
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(response_page_id, response_data, kDefaultPageID,
                     "Elvis", "", "", "", "", "", "", "", "", "", "", "", "",
                     "", "", true, true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000004", 0);
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields.back(),
      PackGUIDs(guid2, empty), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(
        response_page_id, response_data, kPageID2, true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (std::vector<FormFieldData>::iterator iter = form.fields.begin();
       iter != form.fields.end();
       ++iter) {
    iter->is_autofilled = true;
  }

  const int kPageID3 = 3;
  response_page_id = 0;
  FillAutofillFormDataAndSaveResults(kPageID3, form, *form.fields.rbegin(),
      PackGUIDs(guid2, empty), &response_page_id, &response_data);
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(response_page_id, response_data, kPageID3,
                     "", "", "", "", "", "", "", "", "", "", "", "", "", "",
                     "2012", true, true, false);
  }
}

// Test that we correctly fill an address form with a non-default variant for a
// multi-valued field.
TEST_F(AutofillManagerTest, FillAddressFormWithVariantType) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Add a name variant to the Elvis profile.
  AutofillProfile* profile = autofill_manager_->GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000001");

  std::vector<base::string16> name_variants;
  name_variants.push_back(ASCIIToUTF16("Some"));
  name_variants.push_back(profile->GetRawInfo(NAME_FIRST));
  profile->SetRawMultiInfo(NAME_FIRST, name_variants);

  name_variants.clear();
  name_variants.push_back(ASCIIToUTF16("Other"));
  name_variants.push_back(profile->GetRawInfo(NAME_MIDDLE));
  profile->SetRawMultiInfo(NAME_MIDDLE, name_variants);

  name_variants.clear();
  name_variants.push_back(ASCIIToUTF16("Guy"));
  name_variants.push_back(profile->GetRawInfo(NAME_LAST));
  profile->SetRawMultiInfo(NAME_LAST, name_variants);

  GUIDPair guid(profile->guid(), 1);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data1;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data1);
  {
    SCOPED_TRACE("Valid variant");
    ExpectFilledAddressFormElvis(
        response_page_id, response_data1, kDefaultPageID, false);
  }

  // Try filling with a variant that doesn't exist.  The fields to which this
  // variant would normally apply should not be filled.
  const int kPageID2 = 2;
  GUIDPair guid2(profile->guid(), 2);
  response_page_id = 0;
  FormData response_data2;
  FillAutofillFormDataAndSaveResults(kPageID2, form, form.fields[0],
      PackGUIDs(empty, guid2), &response_page_id, &response_data2);
  {
    SCOPED_TRACE("Invalid variant");
    ExpectFilledForm(response_page_id, response_data2, kPageID2, "", "", "",
                     "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                     "Tennessee", "38116", "United States", "12345678901",
                     "theking@gmail.com", "", "", "", "", true, false, false);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(AutofillManagerTest, FillPhoneNumber) {
  // In one form, rely on the maxlength attribute to imply phone number parts.
  // In the other form, rely on the autocompletetype attribute.
  FormData form_with_maxlength;
  form_with_maxlength.name = ASCIIToUTF16("MyMaxlengthPhoneForm");
  form_with_maxlength.origin = GURL("http://myform.com/phone_form.html");
  form_with_maxlength.action = GURL("http://myform.com/phone_submit.html");
  form_with_maxlength.user_submitted = true;
  FormData form_with_autocompletetype = form_with_maxlength;
  form_with_autocompletetype.name = ASCIIToUTF16("MyAutocompletetypePhoneForm");

  struct {
    const char* label;
    const char* name;
    size_t max_length;
    const char* autocomplete_attribute;
  } test_fields[] = {
    { "country code", "country_code", 1, "tel-country-code" },
    { "area code", "area_code", 3, "tel-area-code" },
    { "phone", "phone_prefix", 3, "tel-local-prefix" },
    { "-", "phone_suffix", 4, "tel-local-suffix" },
    { "Phone Extension", "ext", 3, "tel-extension" }
  };

  FormFieldData field;
  const size_t default_max_length = field.max_length;
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_fields); ++i) {
    test::CreateTestFormField(
        test_fields[i].label, test_fields[i].name, "", "text", &field);
    field.max_length = test_fields[i].max_length;
    field.autocomplete_attribute = std::string();
    form_with_maxlength.fields.push_back(field);

    field.max_length = default_max_length;
    field.autocomplete_attribute = test_fields[i].autocomplete_attribute;
    form_with_autocompletetype.fields.push_back(field);
  }

  std::vector<FormData> forms;
  forms.push_back(form_with_maxlength);
  forms.push_back(form_with_autocompletetype);
  FormsSeen(forms);

  // We should be able to fill prefix and suffix fields for US numbers.
  AutofillProfile* work_profile = autofill_manager_->GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != NULL);
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("16505554567"));

  GUIDPair guid(work_profile->guid(), 0);
  GUIDPair empty(std::string(), 0);
  int page_id = 1;
  int response_page_id = 0;
  FormData response_data1;
  FillAutofillFormDataAndSaveResults(page_id, form_with_maxlength,
      *form_with_maxlength.fields.begin(),
      PackGUIDs(empty, guid), &response_page_id, &response_data1);
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
      PackGUIDs(empty, guid), &response_page_id, &response_data2);
  EXPECT_EQ(2, response_page_id);

  ASSERT_EQ(5U, response_data2.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), response_data2.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), response_data2.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), response_data2.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), response_data2.fields[3].value);
  EXPECT_EQ(base::string16(), response_data2.fields[4].value);

  // We should not be able to fill prefix and suffix fields for international
  // numbers.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("GB"));
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("447700954321"));
  page_id = 3;
  response_page_id = 0;
  FormData response_data3;
  FillAutofillFormDataAndSaveResults(page_id, form_with_maxlength,
      *form_with_maxlength.fields.begin(),
      PackGUIDs(empty, guid), &response_page_id, &response_data3);
  EXPECT_EQ(3, response_page_id);

  ASSERT_EQ(5U, response_data3.fields.size());
  EXPECT_EQ(ASCIIToUTF16("44"), response_data3.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("7700"), response_data3.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data3.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data3.fields[3].value);
  EXPECT_EQ(base::string16(), response_data3.fields[4].value);

  page_id = 4;
  response_page_id = 0;
  FormData response_data4;
  FillAutofillFormDataAndSaveResults(page_id, form_with_autocompletetype,
      *form_with_autocompletetype.fields.begin(),
      PackGUIDs(empty, guid), &response_page_id, &response_data4);
  EXPECT_EQ(4, response_page_id);

  ASSERT_EQ(5U, response_data4.fields.size());
  EXPECT_EQ(ASCIIToUTF16("44"), response_data4.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("7700"), response_data4.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data4.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), response_data4.fields[3].value);
  EXPECT_EQ(base::string16(), response_data4.fields[4].value);

  // We should fill all phone fields with the same phone number variant.
  std::vector<base::string16> phone_variants;
  phone_variants.push_back(ASCIIToUTF16("16505554567"));
  phone_variants.push_back(ASCIIToUTF16("18887771234"));
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  work_profile->SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, phone_variants);

  page_id = 5;
  response_page_id = 0;
  FormData response_data5;
  GUIDPair variant_guid(work_profile->guid(), 1);
  FillAutofillFormDataAndSaveResults(page_id, form_with_maxlength,
      *form_with_maxlength.fields.begin(),
      PackGUIDs(empty, variant_guid), &response_page_id, &response_data5);
  EXPECT_EQ(5, response_page_id);

  ASSERT_EQ(5U, response_data5.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), response_data5.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("888"), response_data5.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("777"), response_data5.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("1234"), response_data5.fields[3].value);
  EXPECT_EQ(base::string16(), response_data5.fields[4].value);
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

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);
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

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);
}

// Test that we are able to save form data when forms are submitted.
TEST_F(AutofillManagerTest, FormSubmitted) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(1);
  FormSubmitted(response_data);
}

// Test that when Autocomplete is enabled and Autofill is disabled,
// form submissions are still received by AutocompleteHistoryManager.
TEST_F(AutofillManagerTest, FormSubmittedAutocompleteEnabled) {
  TestAutofillClient client;
  autofill_manager_.reset(
      new TestAutofillManager(autofill_driver_.get(), &client, NULL));
  autofill_manager_->set_autofill_enabled(false);
  scoped_ptr<MockAutocompleteHistoryManager> autocomplete_history_manager;
  autocomplete_history_manager.reset(
      new MockAutocompleteHistoryManager(autofill_driver_.get(), &client));
  autofill_manager_->autocomplete_history_manager_ =
      autocomplete_history_manager.Pass();

  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);
  MockAutocompleteHistoryManager* m = static_cast<
      MockAutocompleteHistoryManager*>(
          autofill_manager_->autocomplete_history_manager_.get());
  EXPECT_CALL(*m,
              OnFormSubmitted(_)).Times(1);
  FormSubmitted(form);
}

// Test that when Autocomplete is enabled and Autofill is disabled,
// Autocomplete suggestions are still received.
TEST_F(AutofillManagerTest, AutocompleteSuggestionsWhenAutofillDisabled) {
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
  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions. We should return the autocomplete
  // suggestions, these will be culled by the renderer.
  std::vector<base::string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  base::string16 expected_values[] = {
    ASCIIToUTF16("Jay"),
    ASCIIToUTF16("Jason")
  };
  base::string16 expected_labels[] = { base::string16(), base::string16()};
  base::string16 expected_icons[] = { base::string16(), base::string16()};
  int expected_unique_ids[] = {0, 0};
  external_delegate_->CheckSuggestions(
      kDefaultPageID, arraysize(expected_values), expected_values,
      expected_labels, expected_icons, expected_unique_ids);
}

// Test that we are able to save form data when forms are submitted and we only
// have server data for the field types.
TEST_F(AutofillManagerTest, FormSubmittedServerTypes) {
  // Set up our form data.
  FormData form;
  test::CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  AutofillMetrics metrics_logger;  // ignored
  form_structure->DetermineHeuristicTypes(metrics_logger);

  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<ServerFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure->field(i)->heuristic_type());
  }
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenForm(form_structure);

  // Fill the form.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[0],
      PackGUIDs(empty, guid), &response_page_id, &response_data);
  ExpectFilledAddressFormElvis(
      response_page_id, response_data, kDefaultPageID, false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(1);
  FormSubmitted(response_data);
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
  std::string signature = FormStructure(form).FormSignature();

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
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  int response_page_id = 0;
  FormData response_data;
  FillAutofillFormDataAndSaveResults(kDefaultPageID, form, form.fields[3],
      PackGUIDs(empty, guid), &response_page_id, &response_data);

  // Simulate form submission.  We should call into the PDM to try to save the
  // filled data.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(1);
  FormSubmitted(response_data);

  // Set the address field's value back to the default value.
  response_data.fields[3].value = ASCIIToUTF16("Enter your address");

  // Simulate form submission.  We should not call into the PDM to try to save
  // the filled data, since the filled form is effectively missing an address.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(0);
  FormSubmitted(response_data);
}

// Checks that resetting the auxiliary profile enabled preference does the right
// thing on all platforms.
TEST_F(AutofillManagerTest, AuxiliaryProfilesReset) {
  PrefService* prefs = autofill_client_.GetPrefs();
#if defined(OS_MACOSX) || defined(OS_ANDROID)
  // Auxiliary profiles is implemented on Mac and Android only.
  // OSX: This preference exists for legacy reasons. It is no longer used.
  // Android: enables integration with user's contact profile.
  ASSERT_TRUE(
      prefs->GetBoolean(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled));
  prefs->SetBoolean(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled,
                    false);
  prefs->ClearPref(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled);
  ASSERT_TRUE(
      prefs->GetBoolean(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled));
#else
  ASSERT_FALSE(
      prefs->GetBoolean(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled));
  prefs->SetBoolean(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled, true);
  prefs->ClearPref(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled);
  ASSERT_FALSE(
      prefs->GetBoolean(::autofill::prefs::kAutofillAuxiliaryProfilesEnabled));
#endif  // defined(OS_MACOSX) || defined(OS_ANDROID)
}

TEST_F(AutofillManagerTest, DeterminePossibleFieldTypesForUpload) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  std::vector<ServerFieldTypeSet> expected_types;

  // These fields should all match.
  FormFieldData field;
  ServerFieldTypeSet types;
  test::CreateTestFormField("", "1", "Elvis", "text", &field);
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "2", "Aaron", "text", &field);
  types.clear();
  types.insert(NAME_MIDDLE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "3", "A", "text", &field);
  types.clear();
  types.insert(NAME_MIDDLE_INITIAL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "4", "Presley", "text", &field);
  types.clear();
  types.insert(NAME_LAST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "5", "Elvis Presley", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_NAME);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "6", "Elvis Aaron Presley", "text",
                                     &field);
  types.clear();
  types.insert(NAME_FULL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "7", "theking@gmail.com", "email",
                                     &field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "8", "RCA", "text", &field);
  types.clear();
  types.insert(COMPANY_NAME);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "9", "3734 Elvis Presley Blvd.",
                                     "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_LINE1);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "10", "Apt. 10", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_LINE2);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "11", "Memphis", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_CITY);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "12", "Tennessee", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_STATE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "13", "38116", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_ZIP);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "14", "USA", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_COUNTRY);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "15", "United States", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_COUNTRY);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "16", "+1 (234) 567-8901", "text",
                                     &field);
  types.clear();
  types.insert(PHONE_HOME_WHOLE_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "17", "2345678901", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_CITY_AND_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "18", "1", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_COUNTRY_CODE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "19", "234", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_CITY_CODE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "20", "5678901", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "21", "567", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "22", "8901", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "23", "4234-5678-9012-3456", "text",
                                     &field);
  types.clear();
  types.insert(CREDIT_CARD_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "24", "04", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_MONTH);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "25", "April", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_MONTH);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "26", "2012", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "27", "12", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "28", "04/2012", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // Make sure that we trim whitespace properly.
  test::CreateTestFormField("", "29", "", "text", &field);
  types.clear();
  types.insert(EMPTY_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "30", " ", "text", &field);
  types.clear();
  types.insert(EMPTY_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "31", " Elvis", "text", &field);
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "32", "Elvis ", "text", &field);
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // These fields should not match, as they differ by case.
  test::CreateTestFormField("", "33", "elvis", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "34", "3734 Elvis Presley BLVD",
                                     "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // These fields should not match, as they are unsupported variants.
  test::CreateTestFormField("", "35", "Elvis Aaron", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "36", "Mr. Presley", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "37", "3734 Elvis Presley", "text",
                                     &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "38", "TN", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "39", "38116-1023", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "20", "5", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "20", "56", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "20", "901", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  test::CreateTestFormField("", "40", "mypassword", "password", &field);
  types.clear();
  types.insert(PASSWORD);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_manager_->set_expected_submitted_field_types(expected_types);
  FormSubmitted(form);
}

TEST_F(AutofillManagerTest, RemoveProfile) {
  // Add and remove an Autofill profile.
  AutofillProfile* profile = new AutofillProfile;
  std::string guid = "00000000-0000-0000-0000-000000000102";
  profile->set_guid(guid.c_str());
  autofill_manager_->AddProfile(profile);

  GUIDPair guid_pair(guid, 0);
  GUIDPair empty(std::string(), 0);
  int id = PackGUIDs(empty, guid_pair);

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(autofill_manager_->GetProfileWithGUID(guid.c_str()));
}

TEST_F(AutofillManagerTest, RemoveCreditCard){
  // Add and remove an Autofill credit card.
  CreditCard* credit_card = new CreditCard;
  std::string guid = "00000000-0000-0000-0000-000000100007";
  credit_card->set_guid(guid.c_str());
  autofill_manager_->AddCreditCard(credit_card);

  GUIDPair guid_pair(guid, 0);
  GUIDPair empty(std::string(), 0);
  int id = PackGUIDs(guid_pair, empty);

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  EXPECT_FALSE(autofill_manager_->GetCreditCardWithGUID(guid.c_str()));
}

TEST_F(AutofillManagerTest, RemoveProfileVariant) {
  // Add and remove an Autofill profile.
  AutofillProfile* profile = new AutofillProfile;
  std::string guid = "00000000-0000-0000-0000-000000000102";
  profile->set_guid(guid.c_str());
  autofill_manager_->AddProfile(profile);

  GUIDPair guid_pair(guid, 1);
  GUIDPair empty(std::string(), 0);
  int id = PackGUIDs(empty, guid_pair);

  autofill_manager_->RemoveAutofillProfileOrCreditCard(id);

  // TODO(csharp): Currently variants should not be deleted, but once they are
  // update these expectations.
  // http://crbug.com/124211
  EXPECT_TRUE(autofill_manager_->GetProfileWithGUID(guid.c_str()));
}

namespace {

class MockAutofillClient : public TestAutofillClient {
 public:
  MockAutofillClient() {}

  virtual ~MockAutofillClient() {}

  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const ResultCallback& callback) OVERRIDE {
    callback.Run(user_supplied_data_ ? AutocompleteResultSuccess :
                                       AutocompleteResultErrorDisabled,
                 base::string16(),
                 user_supplied_data_.get());
  }

  void SetUserSuppliedData(scoped_ptr<FormStructure> user_supplied_data) {
    user_supplied_data_.reset(user_supplied_data.release());
  }

 private:
  scoped_ptr<FormStructure> user_supplied_data_;

  DISALLOW_COPY_AND_ASSIGN(MockAutofillClient);
};

}  // namespace

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

}  // namespace autofill
