// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <vector>

#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/prefs/public/pref_service_base.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/tuple.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_metrics.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/test_autofill_external_delegate.h"
#include "chrome/browser/password_manager/password_manager.h"
#include "chrome/browser/password_manager/password_manager_delegate_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/autofill/tab_autofill_manager_delegate.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/form_data.h"
#include "chrome/common/form_field_data.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/ssl_status.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebAutofillClient.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebFormElement.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/rect.h"

typedef PersonalDataManager::GUIDPair GUIDPair;
using content::BrowserThread;
using content::WebContents;
using testing::_;
using WebKit::WebFormElement;

namespace {

// The page ID sent to the AutofillManager from the RenderView, used to send
// an IPC message back to the renderer.
const int kDefaultPageID = 137;

typedef Tuple5<int,
               std::vector<string16>,
               std::vector<string16>,
               std::vector<string16>,
               std::vector<int> > AutofillParam;

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    CreateTestAutofillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  void SetBrowserContext(content::BrowserContext* context) {
    set_browser_context(context);
  }

  // Factory method for keyed service.  PersonalDataManager is NULL for testing.
  static ProfileKeyedService* Build(Profile* profile) {
    return NULL;
  }

  MOCK_METHOD1(SaveImportedProfile, void(const AutofillProfile&));

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
  virtual void LoadAuxiliaryProfiles() OVERRIDE {}

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
    autofill_test::SetCreditCardInfo(credit_card, "Miku Hatsune",
                                     "4234567890654321", // Visa
                                     month, year);
    credit_card->set_guid("00000000-0000-0000-0000-000000000007");
    credit_cards_.push_back(credit_card);
  }

 private:
  void CreateTestAutofillProfiles(ScopedVector<AutofillProfile>* profiles) {
    AutofillProfile* profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "Elvis", "Aaron",
                                  "Presley", "theking@gmail.com", "RCA",
                                  "3734 Elvis Presley Blvd.", "Apt. 10",
                                  "Memphis", "Tennessee", "38116", "US",
                                  "12345678901");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "Charles", "Hardin",
                                  "Holley", "buddy@gmail.com", "Decca",
                                  "123 Apple St.", "unit 6", "Lubbock",
                                  "Texas", "79401", "US", "23456789012");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "", "", "", "", "", "", "", "", "",
                                  "", "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Elvis Presley",
                                     "4234 5678 9012 3456",  // Visa
                                     "04", "2012");
    credit_card->set_guid("00000000-0000-0000-0000-000000000004");
    credit_cards->push_back(credit_card);

    credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Buddy Holly",
                                     "5187654321098765",  // Mastercard
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

// Populates |form| with data corresponding to a simple address form.
// Note that this actually appends fields to the form data, which can be useful
// for building up more complex test forms.
void CreateTestAddressFormData(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  form->user_submitted = true;

  FormFieldData field;
  autofill_test::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "City", "city", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "State", "state", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Country", "country", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Phone Number", "phonenumber", "", "tel", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Email", "email", "", "email", &field);
  form->fields.push_back(field);
}

// Populates |form| with data corresponding to a simple credit card form.
// Note that this actually appends fields to the form data, which can be useful
// for building up more complex test forms.
void CreateTestCreditCardFormData(FormData* form,
                                  bool is_https,
                                  bool use_month_type) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  if (is_https) {
    form->origin = GURL("https://myform.com/form.html");
    form->action = GURL("https://myform.com/submit.html");
  } else {
    form->origin = GURL("http://myform.com/form.html");
    form->action = GURL("http://myform.com/submit.html");
  }
  form->user_submitted = true;

  FormFieldData field;
  autofill_test::CreateTestFormField(
      "Name on Card", "nameoncard", "", "text", &field);
  form->fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  if (use_month_type) {
    autofill_test::CreateTestFormField(
        "Expiration Date", "ccmonth", "", "month", &field);
    form->fields.push_back(field);
  } else {
    autofill_test::CreateTestFormField(
        "Expiration Date", "ccmonth", "", "text", &field);
    form->fields.push_back(field);
    autofill_test::CreateTestFormField(
        "", "ccyear", "", "text", &field);
    form->fields.push_back(field);
  }
}

void ExpectSuggestions(int page_id,
                       const std::vector<string16>& values,
                       const std::vector<string16>& labels,
                       const std::vector<string16>& icons,
                       const std::vector<int>& unique_ids,
                       int expected_page_id,
                       size_t expected_num_suggestions,
                       const string16 expected_values[],
                       const string16 expected_labels[],
                       const string16 expected_icons[],
                       const int expected_unique_ids[]) {
  EXPECT_EQ(expected_page_id, page_id);
  ASSERT_EQ(expected_num_suggestions, values.size());
  ASSERT_EQ(expected_num_suggestions, labels.size());
  ASSERT_EQ(expected_num_suggestions, icons.size());
  ASSERT_EQ(expected_num_suggestions, unique_ids.size());
  for (size_t i = 0; i < expected_num_suggestions; ++i) {
    SCOPED_TRACE(StringPrintf("i: %" PRIuS, i));
    EXPECT_EQ(expected_values[i], values[i]);
    EXPECT_EQ(expected_labels[i], labels[i]);
    EXPECT_EQ(expected_icons[i], icons[i]);
    EXPECT_EQ(expected_unique_ids[i], unique_ids[i]);
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
  EXPECT_EQ(ASCIIToUTF16("POST"), filled_form.method);
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

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(content::WebContents* web_contents,
                      autofill::AutofillManagerDelegate* delegate,
                      TestPersonalDataManager* personal_data)
      : AutofillManager(web_contents, delegate, personal_data),
        personal_data_(personal_data),
        autofill_enabled_(true),
        did_finish_async_form_submit_(false),
        message_loop_is_running_(false) {
  }

  virtual bool IsAutofillEnabled() const OVERRIDE { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  const std::vector<std::pair<WebFormElement::AutocompleteResult, FormData> >&
      request_autocomplete_results() const {
    return request_autocomplete_results_;
  }


  void set_expected_submitted_field_types(
      const std::vector<FieldTypeSet>& expected_types) {
    expected_submitted_field_types_ = expected_types;
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

    // If we have expected field types set, make sure they match.
    if (!expected_submitted_field_types_.empty()) {
      ASSERT_EQ(expected_submitted_field_types_.size(),
                submitted_form->field_count());
      for (size_t i = 0; i < expected_submitted_field_types_.size(); ++i) {
        SCOPED_TRACE(
            StringPrintf("Field %d with value %s", static_cast<int>(i),
                         UTF16ToUTF8(submitted_form->field(i)->value).c_str()));
        const FieldTypeSet& possible_types =
            submitted_form->field(i)->possible_types();
        EXPECT_EQ(expected_submitted_field_types_[i].size(),
                  possible_types.size());
        for (FieldTypeSet::const_iterator it =
                 expected_submitted_field_types_[i].begin();
             it != expected_submitted_field_types_[i].end(); ++it) {
          EXPECT_TRUE(possible_types.count(*it))
              << "Expected type: " << AutofillType::FieldTypeToString(*it);
        }
      }
    }

    AutofillManager::UploadFormDataAsyncCallback(submitted_form,
                                                 load_time,
                                                 interaction_time,
                                                 submission_time);
  }

  // Wait for the asynchronous OnFormSubmitted() call to complete.
  void WaitForAsyncFormSubmit() {
    if (!did_finish_async_form_submit_) {
      // TODO(isherman): It seems silly to need this variable.  Is there some
      // way I can just query the message loop's state?
      message_loop_is_running_ = true;
      MessageLoop::current()->Run();
    } else {
      did_finish_async_form_submit_ = false;
    }
  }

  virtual void UploadFormData(const FormStructure& submitted_form) OVERRIDE {
    submitted_form_signature_ = submitted_form.FormSignature();
  }

  virtual void SendPasswordGenerationStateToRenderer(
      content::RenderViewHost* host, bool enabled) OVERRIDE {
    sent_states_.push_back(enabled);
  }

  const std::vector<bool>& GetSentStates() {
    return sent_states_;
  }

  void ClearSentStates() {
    sent_states_.clear();
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

  virtual void ReturnAutocompleteResult(
      WebFormElement::AutocompleteResult result,
      const FormData& form_data) OVERRIDE {
    request_autocomplete_results_.push_back(std::make_pair(result, form_data));
  }

 private:
  // AutofillManager is ref counted.
  virtual ~TestAutofillManager() {}

  // Weak reference.
  TestPersonalDataManager* personal_data_;

  bool autofill_enabled_;
  std::vector<std::pair<WebFormElement::AutocompleteResult, FormData> >
      request_autocomplete_results_;

  bool did_finish_async_form_submit_;
  bool message_loop_is_running_;

  std::string submitted_form_signature_;
  std::vector<FieldTypeSet> expected_submitted_field_types_;
  std::vector<bool> sent_states_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

}  // namespace

class AutofillManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutofillManagerTest()
      : ChromeRenderViewHostTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_),
        file_thread_(BrowserThread::FILE) {
  }

  virtual ~AutofillManagerTest() {
  }

  virtual void SetUp() OVERRIDE {
    Profile* profile = new TestingProfile();
    browser_context_.reset(profile);
    PersonalDataManagerFactory::GetInstance()->SetTestingFactory(
        profile, TestPersonalDataManager::Build);

    ChromeRenderViewHostTestHarness::SetUp();
    TabAutofillManagerDelegate::CreateForWebContents(web_contents());
    personal_data_.SetBrowserContext(profile);
    autofill_manager_ = new TestAutofillManager(
        web_contents(),
        TabAutofillManagerDelegate::FromWebContents(web_contents()),
        &personal_data_);

    file_thread_.Start();
  }

  virtual void TearDown() OVERRIDE {
    // Order of destruction is important as AutofillManager relies on
    // PersonalDataManager to be around when it gets destroyed. Also, a real
    // AutofillManager is tied to the lifetime of the WebContents, so it must
    // be destroyed at the destruction of the WebContents.
    autofill_manager_ = NULL;
    file_thread_.Stop();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void UpdatePasswordGenerationState(bool new_renderer) {
    autofill_manager_->UpdatePasswordGenerationState(NULL, new_renderer);
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

  void AutocompleteSuggestionsReturned(const std::vector<string16>& result) {
    autofill_manager_->autocomplete_history_manager_.SendSuggestions(&result);
  }

  void FormsSeen(const std::vector<FormData>& forms) {
    autofill_manager_->OnFormsSeen(forms, base::TimeTicks());
  }

  void LoadServerPredictions(const std::string& response_xml) {
    autofill_manager_->OnLoadedServerPredictions(response_xml);
  }

  void FormSubmitted(const FormData& form) {
    if (autofill_manager_->OnFormSubmitted(form, base::TimeTicks::Now()))
      autofill_manager_->WaitForAsyncFormSubmit();
  }

  void FillAutofillFormData(int query_id,
                            const FormData& form,
                            const FormFieldData& field,
                            int unique_id) {
    autofill_manager_->OnFillAutofillFormData(query_id, form, field, unique_id);
  }

  int PackGUIDs(const GUIDPair& cc_guid, const GUIDPair& profile_guid) const {
    return autofill_manager_->PackGUIDs(cc_guid, profile_guid);
  }

  bool GetAutofillSuggestionsMessage(int* page_id,
                                     std::vector<string16>* values,
                                     std::vector<string16>* labels,
                                     std::vector<string16>* icons,
                                     std::vector<int>* unique_ids) {
    const uint32 kMsgID = AutofillMsg_SuggestionsReturned::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;

    AutofillParam autofill_param;
    AutofillMsg_SuggestionsReturned::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (values)
      *values = autofill_param.b;
    if (labels)
      *labels = autofill_param.c;
    if (icons)
      *icons = autofill_param.d;
    if (unique_ids)
      *unique_ids = autofill_param.e;

    autofill_manager_->autocomplete_history_manager_.CancelPendingQuery();
    process()->sink().ClearMessages();
    return true;
  }

  bool GetAutofillFormDataFilledMessage(int* page_id, FormData* results) {
    const uint32 kMsgID = AutofillMsg_FormDataFilled::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple2<int, FormData> autofill_param;
    AutofillMsg_FormDataFilled::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (results)
      *results = autofill_param.b;

    process()->sink().ClearMessages();
    return true;
  }

 protected:
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  scoped_refptr<TestAutofillManager> autofill_manager_;
  TestPersonalDataManager personal_data_;

  // Used when we want an off the record profile. This will store the original
  // profile from which the off the record profile is derived.
  scoped_ptr<Profile> other_browser_context_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillManagerTest);
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

 private:
  DISALLOW_COPY_AND_ASSIGN(TestFormStructure);
};

// Test that we return all address profile suggestions when all form fields are
// empty.
TEST_F(AutofillManagerTest, GetProfileSuggestionsEmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  GetAutofillSuggestionsMessage(
      &page_id, &values, &labels, &icons, &unique_ids);

  string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  // Inferred labels include full first relevant field, which in this case is
  // the address line 1.
  string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  string16 expected_icons[] = {string16(), string16()};
  int expected_unique_ids[] = {1, 2};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return only matching address profile suggestions when the
// selected form field has been partially filled out.
TEST_F(AutofillManagerTest, GetProfileSuggestionsMatchCharacter) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field;
  autofill_test::CreateTestFormField("First Name", "firstname", "E", "text",
                                     &field);
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {ASCIIToUTF16("Elvis")};
  string16 expected_labels[] = {ASCIIToUTF16("3734 Elvis Presley Blvd.")};
  string16 expected_icons[] = {string16()};
  int expected_unique_ids[] = {1};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return no suggestions when the form has no relevant fields.
TEST_F(AutofillManagerTest, GetProfileSuggestionsUnknownFields) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;
  autofill_test::CreateTestFormField("Username", "username", "", "text",
                                     &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Password", "password", "", "password",
                                     &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Quest", "quest", "", "quest", &field);
  form.fields.push_back(field);
  autofill_test::CreateTestFormField("Color", "color", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(GetAutofillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we cull duplicate profile suggestions.
TEST_F(AutofillManagerTest, GetProfileSuggestionsWithDuplicates) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
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
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  string16 expected_icons[] = {string16(), string16()};
  int expected_unique_ids[] = {1, 2};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return no suggestions when autofill is disabled.
TEST_F(AutofillManagerTest, GetProfileSuggestionsAutofillDisabledByUser) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Disable Autofill.
  autofill_manager_->set_autofill_enabled(false);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(GetAutofillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we return a warning explaining that autofill suggestions are
// unavailable when the form method is GET rather than POST.
TEST_F(AutofillManagerTest, GetProfileSuggestionsMethodGet) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  form.method = ASCIIToUTF16("GET");
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED)
  };
  string16 expected_labels[] = {string16()};
  string16 expected_icons[] = {string16()};
  int expected_unique_ids[] =
      {WebKit::WebAutofillClient::MenuItemIDWarningMessage};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);

  // Now add some Autocomplete suggestions. We should return the autocomplete
  // suggestions and the warning; these will be culled by the renderer.
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values2[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED),
    ASCIIToUTF16("Jay"),
    ASCIIToUTF16("Jason")
  };
  string16 expected_labels2[] = {string16(), string16(), string16()};
  string16 expected_icons2[] = {string16(), string16(), string16()};
  int expected_unique_ids2[] = {-1, 0, 0};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kPageID2, arraysize(expected_values2), expected_values2,
                    expected_labels2, expected_icons2, expected_unique_ids2);

  // Now clear the test profiles and try again -- we shouldn't return a warning.
  personal_data_.ClearAutofillProfiles();
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(GetAutofillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
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
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("************3456"),
    ASCIIToUTF16("************8765")
  };
  string16 expected_labels[] = {ASCIIToUTF16("*3456"), ASCIIToUTF16("*8765")};
  string16 expected_icons[] = {
    ASCIIToUTF16("visaCC"),
    ASCIIToUTF16("genericCC")
  };
  int expected_unique_ids[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5)
  };
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
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
  autofill_test::CreateTestFormField(
      "Card Number", "cardnumber", "4", "text", &field);
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {ASCIIToUTF16("************3456")};
  string16 expected_labels[] = {ASCIIToUTF16("*3456")};
  string16 expected_icons[] = {ASCIIToUTF16("visaCC")};
  int expected_unique_ids[] = {autofill_manager_->GetPackedCreditCardID(4)};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
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
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("Elvis Presley"),
    ASCIIToUTF16("Buddy Holly")
  };
  string16 expected_labels[] = {ASCIIToUTF16("*3456"), ASCIIToUTF16("*8765")};
  string16 expected_icons[] = {
    ASCIIToUTF16("visaCC"),
    ASCIIToUTF16("genericCC")
  };
  int expected_unique_ids[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5)
  };
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
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
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION)
  };
  string16 expected_labels[] = {string16()};
  string16 expected_icons[] = {string16()};
  int expected_unique_ids[] = {-1};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);

  // Now add some Autocomplete suggestions. We should show the autocomplete
  // suggestions and the warning.
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));
  string16 expected_values2[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION),
    ASCIIToUTF16("Jay"),
    ASCIIToUTF16("Jason")
  };
  string16 expected_labels2[] = {string16(), string16(), string16()};
  string16 expected_icons2[] = {string16(), string16(), string16()};
  int expected_unique_ids2[] = {-1, 0, 0};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kPageID2, arraysize(expected_values2), expected_values2,
                    expected_labels2, expected_icons2, expected_unique_ids2);

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(GetAutofillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we return all credit card suggestions in the case that two cards
// have the same obfuscated number.
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsRepeatedObfuscatedNumber) {
  // Add a credit card with the same obfuscated number as Elvis's.
  // |credit_card| will be owned by the mock PersonalDataManager.
  CreditCard* credit_card = new CreditCard;
  autofill_test::SetCreditCardInfo(credit_card, "Elvis Presley",
                                   "5231567890123456",  // Mastercard
                                   "04", "2012");
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
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("************3456"),
    ASCIIToUTF16("************8765"),
    ASCIIToUTF16("************3456")
  };
  string16 expected_labels[] = {
    ASCIIToUTF16("*3456"),
    ASCIIToUTF16("*8765"),
    ASCIIToUTF16("*3456"),
  };
  string16 expected_icons[] = {
    ASCIIToUTF16("visaCC"),
    ASCIIToUTF16("genericCC"),
    ASCIIToUTF16("masterCardCC")
  };
  int expected_unique_ids[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5),
    autofill_manager_->GetPackedCreditCardID(7)
  };
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return profile and credit card suggestions for combined forms.
TEST_F(AutofillManagerTest, GetAddressAndCreditCardSuggestions) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right address suggestions to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  string16 expected_icons[] = {string16(), string16()};
  int expected_unique_ids[] = {1, 2};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);

  const int kPageID2 = 2;
  autofill_test::CreateTestFormField(
      "Card Number", "cardnumber", "", "text", &field);
  GetAutofillSuggestions(kPageID2, form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the credit card suggestions to the renderer.
  page_id = 0;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values2[] = {
    ASCIIToUTF16("************3456"),
    ASCIIToUTF16("************8765")
  };
  string16 expected_labels2[] = {ASCIIToUTF16("*3456"), ASCIIToUTF16("*8765")};
  string16 expected_icons2[] = {
    ASCIIToUTF16("visaCC"),
    ASCIIToUTF16("genericCC")
  };
  int expected_unique_ids2[] = {
    autofill_manager_->GetPackedCreditCardID(4),
    autofill_manager_->GetPackedCreditCardID(5)
  };
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
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
  CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, false, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormFieldData field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right address suggestions to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St.")
  };
  string16 expected_icons[] = {string16(), string16()};
  int expected_unique_ids[] = {1, 2};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);

  autofill_test::CreateTestFormField(
      "Card Number", "cardnumber", "", "text", &field);
  const int kPageID2 = 2;
  GetAutofillSuggestions(kPageID2, form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values2[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_INSECURE_CONNECTION)
  };
  string16 expected_labels2[] = {string16()};
  string16 expected_icons2[] = {string16()};
  int expected_unique_ids2[] = {-1};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kPageID2, arraysize(expected_values2), expected_values2,
                    expected_labels2, expected_icons2, expected_unique_ids2);

  // Clear the test credit cards and try again -- we shouldn't return a warning.
  personal_data_.ClearCreditCards();
  GetAutofillSuggestions(form, field);
  EXPECT_FALSE(GetAutofillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we correctly combine autofill and autocomplete suggestions.
TEST_F(AutofillManagerTest, GetCombinedAutofillAndAutocompleteSuggestions) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  // This suggestion is a duplicate, and should be trimmed.
  suggestions.push_back(ASCIIToUTF16("Elvis"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles"),
    ASCIIToUTF16("Jay"),
    ASCIIToUTF16("Jason")
  };
  string16 expected_labels[] = {
    ASCIIToUTF16("3734 Elvis Presley Blvd."),
    ASCIIToUTF16("123 Apple St."),
    string16(),
    string16()
  };
  string16 expected_icons[] = {string16(), string16(), string16(), string16()};
  int expected_unique_ids[] = {1, 2, 0, 0};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we return autocomplete-like suggestions when trying to autofill
// already filled forms.
TEST_F(AutofillManagerTest, GetFieldSuggestionsWhenFormIsAutofilled) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Mark one of the fields as filled.
  form.fields[2].is_autofilled = true;
  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));
  string16 expected_values[] = {
    ASCIIToUTF16("Elvis"),
    ASCIIToUTF16("Charles")
  };
  string16 expected_labels[] = {string16(), string16()};
  string16 expected_icons[] = {string16(), string16()};
  int expected_unique_ids[] = {1, 2};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that nothing breaks when there are autocomplete suggestions but no
// autofill suggestions.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForAutocompleteOnly) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  FormFieldData field;
  autofill_test::CreateTestFormField(
      "Some Field", "somefield", "", "text", &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutofillSuggestions(form, field);

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("one"));
  suggestions.push_back(ASCIIToUTF16("two"));
  AutocompleteSuggestionsReturned(suggestions);

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("one"),
    ASCIIToUTF16("two")
  };
  string16 expected_labels[] = {string16(), string16()};
  string16 expected_icons[] = {string16(), string16()};
  int expected_unique_ids[] = {0, 0};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we do not return duplicate values drawn from multiple profiles when
// filling an already filled field.
TEST_F(AutofillManagerTest, GetFieldSuggestionsWithDuplicateValues) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile* profile = new AutofillProfile;
  autofill_test::SetProfileInfo(profile, "Elvis", "", "", "", "", "", "", "",
                                "", "", "", "");
  profile->set_guid("00000000-0000-0000-0000-000000000101");
  autofill_manager_->AddProfile(profile);

  FormFieldData& field = form.fields[0];
  field.is_autofilled = true;
  field.value = ASCIIToUTF16("Elvis");
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = { ASCIIToUTF16("Elvis") };
  string16 expected_labels[] = { string16() };
  string16 expected_icons[] = { string16() };
  int expected_unique_ids[] = { 1 };
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that a non-default value is suggested for multi-valued profile, on an
// unfilled form.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForMultiValuedProfileUnfilled) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile* profile = new AutofillProfile;
  autofill_test::SetProfileInfo(profile, "Elvis", "", "Presley", "me@x.com", "",
                                "", "", "", "", "", "", "");
  profile->set_guid("00000000-0000-0000-0000-000000000101");
  std::vector<string16> multi_values(2);
  multi_values[0] = ASCIIToUTF16("Elvis Presley");
  multi_values[1] = ASCIIToUTF16("Elena Love");
  profile->SetRawMultiInfo(NAME_FULL, multi_values);
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
    AutocompleteSuggestionsReturned(std::vector<string16>());

    // Test that we sent the right message to the renderer.
    int page_id = 0;
    std::vector<string16> values;
    std::vector<string16> labels;
    std::vector<string16> icons;
    std::vector<int> unique_ids;
    EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels,
                                              &icons, &unique_ids));
    string16 expected_values[] = {
      ASCIIToUTF16("Elvis"),
      ASCIIToUTF16("Elena")
    };
    string16 expected_labels[] = {
      ASCIIToUTF16("me@x.com"),
      ASCIIToUTF16("me@x.com")
    };
    string16 expected_icons[] = { string16(), string16() };
    int expected_unique_ids[] = { 1, 2 };
    ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                      kDefaultPageID, arraysize(expected_values),
                      expected_values, expected_labels, expected_icons,
                      expected_unique_ids);
  }

  {
    // Get the first name field.
    // This time, start out with "Ele", hoping for "Elena".
    FormFieldData& field = form.fields[0];
    field.value = ASCIIToUTF16("Ele");
    field.is_autofilled = false;
    GetAutofillSuggestions(form, field);

    // Trigger the |Send|.
    AutocompleteSuggestionsReturned(std::vector<string16>());

    // Test that we sent the right message to the renderer.
    int page_id = 0;
    std::vector<string16> values;
    std::vector<string16> labels;
    std::vector<string16> icons;
    std::vector<int> unique_ids;
    EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels,
                                              &icons, &unique_ids));

    string16 expected_values[] = { ASCIIToUTF16("Elena") };
    string16 expected_labels[] = { ASCIIToUTF16("me@x.com") };
    string16 expected_icons[] = { string16() };
    int expected_unique_ids[] = { 2 };
    ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                      kDefaultPageID, arraysize(expected_values),
                      expected_values, expected_labels, expected_icons,
                      expected_unique_ids);
  }
}

// Test that all values are suggested for multi-valued profile, on a filled
// form.  This is the per-field "override" case.
TEST_F(AutofillManagerTest, GetFieldSuggestionsForMultiValuedProfileFilled) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutofillProfile* profile = new AutofillProfile;
  profile->set_guid("00000000-0000-0000-0000-000000000102");
  std::vector<string16> multi_values(3);
  multi_values[0] = ASCIIToUTF16("Travis Smith");
  multi_values[1] = ASCIIToUTF16("Cynthia Love");
  multi_values[2] = ASCIIToUTF16("Zac Mango");
  profile->SetRawMultiInfo(NAME_FULL, multi_values);
  autofill_manager_->AddProfile(profile);

  // Get the first name field.  And start out with "Travis", hoping for all the
  // multi-valued variants as suggestions.
  FormFieldData& field = form.fields[0];
  field.value = ASCIIToUTF16("Travis");
  field.is_autofilled = true;
  GetAutofillSuggestions(form, field);

  // Trigger the |Send|.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutofillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    ASCIIToUTF16("Travis"),
    ASCIIToUTF16("Cynthia"),
    ASCIIToUTF16("Zac")
  };
  string16 expected_labels[] = { string16(), string16(), string16() };
  string16 expected_icons[] = { string16(), string16(), string16() };
  int expected_unique_ids[] = { 1, 2, 3 };
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

TEST_F(AutofillManagerTest, GetProfileSuggestionsFancyPhone) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile* profile = new AutofillProfile;
  profile->set_guid("00000000-0000-0000-0000-000000000103");
  std::vector<string16> multi_values(1);
  multi_values[0] = ASCIIToUTF16("Natty Bumppo");
  profile->SetRawMultiInfo(NAME_FULL, multi_values);
  multi_values[0] = ASCIIToUTF16("1800PRAIRIE");
  profile->SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, multi_values);
  autofill_manager_->AddProfile(profile);

  const FormFieldData& field = form.fields[9];
  GetAutofillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  GetAutofillSuggestionsMessage(
      &page_id, &values, &labels, &icons, &unique_ids);

  string16 expected_values[] = {
    ASCIIToUTF16("12345678901"),
    ASCIIToUTF16("23456789012"),
    ASCIIToUTF16("18007724743"),  // 1800PRAIRIE
  };
  // Inferred labels include full first relevant field, which in this case is
  // the address line 1.
  string16 expected_labels[] = {
    ASCIIToUTF16("Elvis Aaron Presley"),
    ASCIIToUTF16("Charles Hardin Holley"),
    ASCIIToUTF16("Natty Bumppo"),
  };
  string16 expected_icons[] = {string16(), string16(), string16()};
  int expected_unique_ids[] = {1, 2, 3};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);
}

// Test that we correctly fill an address form.
TEST_F(AutofillManagerTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we correctly fill an address form from an auxiliary profile.
TEST_F(AutofillManagerTest, FillAddressFormFromAuxiliaryProfile) {
  personal_data_.ClearAutofillProfiles();
  PrefServiceBase* prefs = PrefServiceBase::FromBrowserContext(profile());
  prefs->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled, true);
  personal_data_.CreateTestAuxiliaryProfiles();

  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
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
  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       PackGUIDs(guid, empty));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardFormElvis(page_id, results, kDefaultPageID, false);
}

TEST_F(AutofillManagerTest, FillCheckableElements) {
  FormData form;
  CreateTestAddressFormData(&form);

  // Add a checkbox field.
  FormFieldData field;
  autofill_test::CreateTestFormField(
      "Checkbox", "checkbx", "fill-me", "checkbox", &field);
  field.is_checkable = true;
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Checkbox", "checkbx", "fill-me", "checkbox", &field);
  field.is_checkable = true;
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Replicate server response with XML.
  const std::string response_xml =
      "<autofillqueryresponse>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"0\"/>"
      "<field autofilltype=\"61\" defaultvalue=\"fill-me\"/>"
      "<field autofilltype=\"61\" defaultvalue=\"dont-fill-me\"/>"
      "</autofillqueryresponse>";
  LoadServerPredictions(response_xml);

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));
  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  // Second to last field should be checked.
  EXPECT_TRUE(results.fields[results.fields.size()-2].is_checked);
  // Last field shouldn't be checked as values are not same.
  EXPECT_FALSE(results.fields[results.fields.size()-1].is_checked);
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
  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       PackGUIDs(guid, empty));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
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
  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       PackGUIDs(guid, empty));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
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
  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       PackGUIDs(guid, empty));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
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
  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       PackGUIDs(guid, empty));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
      kDefaultPageID, false, "2012", "04");
}

// Test that we correctly fill a combined address and credit card form.
TEST_F(AutofillManagerTest, FillAddressAndCreditCardForm) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Address");
    ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, true);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000004", 0);
  FillAutofillFormData(kPageID2, form, form.fields.back(),
                       PackGUIDs(guid2, empty));

  page_id = 0;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Credit card");
    ExpectFilledCreditCardFormElvis(page_id, results, kPageID2, true);
  }
}

// Test that we correctly fill a form that has multiple logical sections, e.g.
// both a billing and a shipping address.
TEST_F(AutofillManagerTest, FillFormWithMultipleSections) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  const size_t kAddressFormSize = form.fields.size();
  CreateTestAddressFormData(&form);
  for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
    // Make sure the fields have distinct names.
    form.fields[i].name = form.fields[i].name + ASCIIToUTF16("_");
  }
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the first section.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Address 1");

    // The second address section should be empty.
    ASSERT_EQ(results.fields.size(), 2*kAddressFormSize);
    for (size_t i = kAddressFormSize; i < form.fields.size(); ++i) {
      EXPECT_EQ(string16(), results.fields[i].value);
    }

    // The first address section should be filled with Elvis's data.
    results.fields.resize(kAddressFormSize);
    ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
  }

  // Fill the second section, with the initiating field somewhere in the middle
  // of the section.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000001", 0);
  ASSERT_LT(9U, kAddressFormSize);
  FillAutofillFormData(kPageID2, form, form.fields[kAddressFormSize + 9],
                       PackGUIDs(empty, guid2));

  page_id = 0;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Address 2");
    ASSERT_EQ(results.fields.size(), form.fields.size());

    // The first address section should be empty.
    ASSERT_EQ(results.fields.size(), 2*kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      EXPECT_EQ(string16(), results.fields[i].value);
    }

    // The second address section should be filled with Elvis's data.
    FormData secondSection = results;
    secondSection.fields.erase(secondSection.fields.begin(),
                               secondSection.fields.begin() + kAddressFormSize);
    for (size_t i = 0; i < kAddressFormSize; ++i) {
      // Restore the expected field names.
      string16 name = secondSection.fields[i].name;
      string16 original_name = name.substr(0, name.size() - 1);
      secondSection.fields[i].name = original_name;
    }
    ExpectFilledAddressFormElvis(page_id, secondSection, kPageID2, false);
  }
}

// Test that we correctly fill a form that has author-specified sections, which
// might not match our expected section breakdown.
TEST_F(AutofillManagerTest, FillFormWithAuthorSpecifiedSections) {
  // Create a form with a billing section and an unnamed section, interleaved.
  // The billing section includes both address and credit card fields.
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.user_submitted = true;

  FormFieldData field;

  autofill_test::CreateTestFormField("", "country", "", "text", &field);
  field.autocomplete_attribute = "section-billing country";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "firstname", "", "text", &field);
  field.autocomplete_attribute = "given-name";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "lastname", "", "text", &field);
  field.autocomplete_attribute = "family-name";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "address", "", "text", &field);
  field.autocomplete_attribute = "section-billing street-address";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "city", "", "text", &field);
  field.autocomplete_attribute = "section-billing locality";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "state", "", "text", &field);
  field.autocomplete_attribute = "section-billing region";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "zip", "", "text", &field);
  field.autocomplete_attribute = "section-billing postal-code";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "ccname", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-name";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "ccnumber", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-number";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "ccexp", "", "text", &field);
  field.autocomplete_attribute = "section-billing cc-exp";
  form.fields.push_back(field);

  autofill_test::CreateTestFormField("", "email", "", "text", &field);
  field.autocomplete_attribute = "email";
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the unnamed section.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[1],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Unnamed section");
    EXPECT_EQ(kDefaultPageID, page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
    EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
    EXPECT_EQ(GURL("https://myform.com/form.html"), results.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), results.action);
    EXPECT_TRUE(results.user_submitted);
    ASSERT_EQ(11U, results.fields.size());

    ExpectFilledField("", "country", "", "text", results.fields[0]);
    ExpectFilledField("", "firstname", "Elvis", "text", results.fields[1]);
    ExpectFilledField("", "lastname", "Presley", "text", results.fields[2]);
    ExpectFilledField("", "address", "", "text", results.fields[3]);
    ExpectFilledField("", "city", "", "text", results.fields[4]);
    ExpectFilledField("", "state", "", "text", results.fields[5]);
    ExpectFilledField("", "zip", "", "text", results.fields[6]);
    ExpectFilledField("", "ccname", "", "text", results.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", results.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", results.fields[9]);
    ExpectFilledField("", "email", "theking@gmail.com", "text",
                      results.fields[10]);
  }

  // Fill the address portion of the billing section.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000001", 0);
  FillAutofillFormData(kPageID2, form, form.fields[0],
                       PackGUIDs(empty, guid2));

  page_id = 0;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Billing address");
    EXPECT_EQ(kPageID2, page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
    EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
    EXPECT_EQ(GURL("https://myform.com/form.html"), results.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), results.action);
    EXPECT_TRUE(results.user_submitted);
    ASSERT_EQ(11U, results.fields.size());

    ExpectFilledField("", "country", "United States", "text",
                      results.fields[0]);
    ExpectFilledField("", "firstname", "", "text", results.fields[1]);
    ExpectFilledField("", "lastname", "", "text", results.fields[2]);
    ExpectFilledField("", "address", "3734 Elvis Presley Blvd.", "text",
                      results.fields[3]);
    ExpectFilledField("", "city", "Memphis", "text", results.fields[4]);
    ExpectFilledField("", "state", "Tennessee", "text", results.fields[5]);
    ExpectFilledField("", "zip", "38116", "text", results.fields[6]);
    ExpectFilledField("", "ccname", "", "text", results.fields[7]);
    ExpectFilledField("", "ccnumber", "", "text", results.fields[8]);
    ExpectFilledField("", "ccexp", "", "text", results.fields[9]);
    ExpectFilledField("", "email", "", "text", results.fields[10]);
  }

  // Fill the credit card portion of the billing section.
  const int kPageID3 = 3;
  GUIDPair guid3("00000000-0000-0000-0000-000000000004", 0);
  FillAutofillFormData(kPageID3, form, form.fields[form.fields.size() - 2],
                       PackGUIDs(guid3, empty));

  page_id = 0;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Credit card");
    EXPECT_EQ(kPageID3, page_id);
    EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
    EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
    EXPECT_EQ(GURL("https://myform.com/form.html"), results.origin);
    EXPECT_EQ(GURL("https://myform.com/submit.html"), results.action);
    EXPECT_TRUE(results.user_submitted);
    ASSERT_EQ(11U, results.fields.size());

    ExpectFilledField("", "country", "", "text", results.fields[0]);
    ExpectFilledField("", "firstname", "", "text", results.fields[1]);
    ExpectFilledField("", "lastname", "", "text", results.fields[2]);
    ExpectFilledField("", "address", "", "text", results.fields[3]);
    ExpectFilledField("", "city", "", "text", results.fields[4]);
    ExpectFilledField("", "state", "", "text", results.fields[5]);
    ExpectFilledField("", "zip", "", "text", results.fields[6]);
    ExpectFilledField("", "ccname", "Elvis Presley", "text", results.fields[7]);
    ExpectFilledField("", "ccnumber", "4234567890123456", "text",
                      results.fields[8]);
    ExpectFilledField("", "ccexp", "04/2012", "text", results.fields[9]);
    ExpectFilledField("", "email", "", "text", results.fields[10]);
  }
}

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(AutofillManagerTest, FillFormWithMultipleEmails) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  FormFieldData field;
  autofill_test::CreateTestFormField(
      "Confirm email", "email2", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));

  // The second email address should be filled.
  EXPECT_EQ(ASCIIToUTF16("theking@gmail.com"), results.fields.back().value);

  // The remainder of the form should be filled as usual.
  results.fields.pop_back();
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_F(AutofillManagerTest, FillAutofilledForm) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  // Mark one of the address fields as autofilled.
  form.fields[4].is_autofilled = true;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, *form.fields.begin(),
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(page_id, results, kDefaultPageID,
                     "Elvis", "", "", "", "", "", "", "", "", "", "", "", "",
                     "", "", true, true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  GUIDPair guid2("00000000-0000-0000-0000-000000000004", 0);
  FillAutofillFormData(kPageID2, form, form.fields.back(),
                       PackGUIDs(guid2, empty));

  page_id = 0;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(page_id, results, kPageID2, true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (std::vector<FormFieldData>::iterator iter = form.fields.begin();
       iter != form.fields.end();
       ++iter) {
    iter->is_autofilled = true;
  }

  const int kPageID3 = 3;
  FillAutofillFormData(kPageID3, form, *form.fields.rbegin(),
                       PackGUIDs(guid2, empty));

  page_id = 0;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(page_id, results, kPageID3,
                     "", "", "", "", "", "", "", "", "", "", "", "", "", "",
                     "2012", true, true, false);
  }
}

// Test that we correctly fill an address form with a non-default variant for a
// multi-valued field.
TEST_F(AutofillManagerTest, FillAddressFormWithVariantType) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Add a name variant to the Elvis profile.
  AutofillProfile* profile = autofill_manager_->GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000001");
  const string16 elvis_name = profile->GetRawInfo(NAME_FULL);

  std::vector<string16> name_variants;
  name_variants.push_back(ASCIIToUTF16("Some Other Guy"));
  name_variants.push_back(elvis_name);
  profile->SetRawMultiInfo(NAME_FULL, name_variants);

  GUIDPair guid(profile->guid(), 1);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results1;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results1));
  {
    SCOPED_TRACE("Valid variant");
    ExpectFilledAddressFormElvis(page_id, results1, kDefaultPageID, false);
  }

  // Try filling with a variant that doesn't exist.  The fields to which this
  // variant would normally apply should not be filled.
  const int kPageID2 = 2;
  GUIDPair guid2(profile->guid(), 2);
  FillAutofillFormData(kPageID2, form, form.fields[0],
                       PackGUIDs(empty, guid2));

  page_id = 0;
  FormData results2;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results2));
  {
    SCOPED_TRACE("Invalid variant");
    ExpectFilledForm(page_id, results2, kPageID2, "", "", "",
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
  form_with_maxlength.method = ASCIIToUTF16("POST");
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
    autofill_test::CreateTestFormField(
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
  FillAutofillFormData(page_id, form_with_maxlength,
                       *form_with_maxlength.fields.begin(),
                       PackGUIDs(empty, guid));
  page_id = 0;
  FormData results1;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results1));
  EXPECT_EQ(1, page_id);

  ASSERT_EQ(5U, results1.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), results1.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), results1.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), results1.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), results1.fields[3].value);
  EXPECT_EQ(string16(), results1.fields[4].value);

  page_id = 2;
  FillAutofillFormData(page_id, form_with_autocompletetype,
                       *form_with_autocompletetype.fields.begin(),
                       PackGUIDs(empty, guid));
  page_id = 0;
  FormData results2;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results2));
  EXPECT_EQ(2, page_id);

  ASSERT_EQ(5U, results2.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), results2.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650"), results2.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("555"), results2.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("4567"), results2.fields[3].value);
  EXPECT_EQ(string16(), results2.fields[4].value);

  // We should not be able to fill prefix and suffix fields for international
  // numbers.
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("GB"));
  work_profile->SetRawInfo(PHONE_HOME_WHOLE_NUMBER,
                           ASCIIToUTF16("447700954321"));
  page_id = 3;
  FillAutofillFormData(page_id, form_with_maxlength,
                       *form_with_maxlength.fields.begin(),
                       PackGUIDs(empty, guid));
  page_id = 0;
  FormData results3;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results3));
  EXPECT_EQ(3, page_id);

  ASSERT_EQ(5U, results3.fields.size());
  EXPECT_EQ(ASCIIToUTF16("44"), results3.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("7700"), results3.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), results3.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), results3.fields[3].value);
  EXPECT_EQ(string16(), results3.fields[4].value);

  page_id = 4;
  FillAutofillFormData(page_id, form_with_autocompletetype,
                       *form_with_autocompletetype.fields.begin(),
                       PackGUIDs(empty, guid));
  page_id = 0;
  FormData results4;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results4));
  EXPECT_EQ(4, page_id);

  ASSERT_EQ(5U, results4.fields.size());
  EXPECT_EQ(ASCIIToUTF16("44"), results4.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("7700"), results4.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), results4.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("954321"), results4.fields[3].value);
  EXPECT_EQ(string16(), results4.fields[4].value);

  // We should fill all phone fields with the same phone number variant.
  std::vector<string16> phone_variants;
  phone_variants.push_back(ASCIIToUTF16("16505554567"));
  phone_variants.push_back(ASCIIToUTF16("18887771234"));
  work_profile->SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  work_profile->SetRawMultiInfo(PHONE_HOME_WHOLE_NUMBER, phone_variants);

  page_id = 5;
  GUIDPair variant_guid(work_profile->guid(), 1);
  FillAutofillFormData(page_id, form_with_maxlength,
                       *form_with_maxlength.fields.begin(),
                       PackGUIDs(empty, variant_guid));
  page_id = 0;
  FormData results5;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results5));
  EXPECT_EQ(5, page_id);

  ASSERT_EQ(5U, results5.fields.size());
  EXPECT_EQ(ASCIIToUTF16("1"), results5.fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("888"), results5.fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("777"), results5.fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("1234"), results5.fields[3].value);
  EXPECT_EQ(string16(), results5.fields[4].value);
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(AutofillManagerTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);

  // Add a field -- we'll remove it again later.
  FormFieldData field;
  autofill_test::CreateTestFormField("Some", "field", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(AutofillManagerTest, FormChangesAddField) {
  // The offset of the phone field in the address form.
  const int kPhoneFieldOffset = 9;

  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);

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
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we are able to save form data when forms are submitted.
TEST_F(AutofillManagerTest, FormSubmitted) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(1);
  FormSubmitted(results);
}

// Test that we are able to save form data when forms are submitted and we only
// have server data for the field types.
TEST_F(AutofillManagerTest, FormSubmittedServerTypes) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);

  // Simulate having seen this form on page load.
  // |form_structure| will be owned by |autofill_manager_|.
  TestFormStructure* form_structure = new TestFormStructure(form);
  AutofillMetrics metrics_logger;  // ignored
  form_structure->DetermineHeuristicTypes(metrics_logger);

  // Clear the heuristic types, and instead set the appropriate server types.
  std::vector<AutofillFieldType> heuristic_types, server_types;
  for (size_t i = 0; i < form.fields.size(); ++i) {
    heuristic_types.push_back(UNKNOWN_TYPE);
    server_types.push_back(form_structure->field(i)->type());
  }
  form_structure->SetFieldTypes(heuristic_types, server_types);
  autofill_manager_->AddSeenForm(form_structure);

  // Fill the form.
  GUIDPair guid("00000000-0000-0000-0000-000000000001", 0);
  GUIDPair empty(std::string(), 0);
  FillAutofillFormData(kDefaultPageID, form, form.fields[0],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);

  // Simulate form submission. We should call into the PDM to try to save the
  // filled data.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(1);
  FormSubmitted(results);
}

// Test that the form signature for an uploaded form always matches the form
// signature from the query.
TEST_F(AutofillManagerTest, FormSubmittedWithDifferentFields) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
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
  CreateTestAddressFormData(&form);
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
  FillAutofillFormData(kDefaultPageID, form, form.fields[3],
                       PackGUIDs(empty, guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutofillFormDataFilledMessage(&page_id, &results));

  // Simulate form submission.  We should call into the PDM to try to save the
  // filled data.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(1);
  FormSubmitted(results);

  // Set the address field's value back to the default value.
  results.fields[3].value = ASCIIToUTF16("Enter your address");

  // Simulate form submission.  We should not call into the PDM to try to save
  // the filled data, since the filled form is effectively missing an address.
  EXPECT_CALL(personal_data_, SaveImportedProfile(::testing::_)).Times(0);
  FormSubmitted(results);
}

// Checks that resetting the auxiliary profile enabled preference does the right
// thing on all platforms.
TEST_F(AutofillManagerTest, AuxiliaryProfilesReset) {
  PrefServiceBase* prefs = PrefServiceBase::FromBrowserContext(profile());
#if defined(OS_MACOSX)
  // Auxiliary profiles is implemented on Mac only.  It enables Mac Address
  // Book integration.
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));
  prefs->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled, false);
  prefs->ClearPref(prefs::kAutofillAuxiliaryProfilesEnabled);
  ASSERT_TRUE(prefs->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));
#else
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));
  prefs->SetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled, true);
  prefs->ClearPref(prefs::kAutofillAuxiliaryProfilesEnabled);
  ASSERT_FALSE(prefs->GetBoolean(prefs::kAutofillAuxiliaryProfilesEnabled));
#endif
}

TEST_F(AutofillManagerTest, DeterminePossibleFieldTypesForUpload) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  std::vector<FieldTypeSet> expected_types;

  // These fields should all match.
  FormFieldData field;
  FieldTypeSet types;
  autofill_test::CreateTestFormField("", "1", "Elvis", "text", &field);
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "2", "Aaron", "text", &field);
  types.clear();
  types.insert(NAME_MIDDLE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "3", "A", "text", &field);
  types.clear();
  types.insert(NAME_MIDDLE_INITIAL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "4", "Presley", "text", &field);
  types.clear();
  types.insert(NAME_LAST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "5", "Elvis Presley", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_NAME);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "6", "Elvis Aaron Presley", "text",
                                     &field);
  types.clear();
  types.insert(NAME_FULL);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "7", "theking@gmail.com", "email",
                                     &field);
  types.clear();
  types.insert(EMAIL_ADDRESS);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "8", "RCA", "text", &field);
  types.clear();
  types.insert(COMPANY_NAME);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "9", "3734 Elvis Presley Blvd.",
                                     "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_LINE1);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "10", "Apt. 10", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_LINE2);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "11", "Memphis", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_CITY);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "12", "Tennessee", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_STATE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "13", "38116", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_ZIP);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "14", "USA", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_COUNTRY);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "15", "United States", "text", &field);
  types.clear();
  types.insert(ADDRESS_HOME_COUNTRY);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "16", "+1 (234) 567-8901", "text",
                                     &field);
  types.clear();
  types.insert(PHONE_HOME_WHOLE_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "17", "2345678901", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_CITY_AND_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "18", "1", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_COUNTRY_CODE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "19", "234", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_CITY_CODE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "20", "5678901", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "21", "567", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "22", "8901", "text", &field);
  types.clear();
  types.insert(PHONE_HOME_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "23", "4234-5678-9012-3456", "text",
                                     &field);
  types.clear();
  types.insert(CREDIT_CARD_NUMBER);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "24", "04", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_MONTH);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "25", "April", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_MONTH);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "26", "2012", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "27", "12", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "28", "04/2012", "text", &field);
  types.clear();
  types.insert(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // Make sure that we trim whitespace properly.
  autofill_test::CreateTestFormField("", "29", "", "text", &field);
  types.clear();
  types.insert(EMPTY_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "30", " ", "text", &field);
  types.clear();
  types.insert(EMPTY_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "31", " Elvis", "text", &field);
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "32", "Elvis ", "text", &field);
  types.clear();
  types.insert(NAME_FIRST);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // These fields should not match, as they differ by case.
  autofill_test::CreateTestFormField("", "33", "elvis", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "34", "3734 Elvis Presley BLVD",
                                     "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  // These fields should not match, as they are unsupported variants.
  autofill_test::CreateTestFormField("", "35", "Elvis Aaron", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "36", "Mr. Presley", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "37", "3734 Elvis Presley", "text",
                                     &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "38", "TN", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "39", "38116-1023", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "20", "5", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "20", "56", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_test::CreateTestFormField("", "20", "901", "text", &field);
  types.clear();
  types.insert(UNKNOWN_TYPE);
  form.fields.push_back(field);
  expected_types.push_back(types);

  autofill_manager_->set_expected_submitted_field_types(expected_types);
  FormSubmitted(form);
}

TEST_F(AutofillManagerTest, UpdatePasswordSyncState) {
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefServiceBase* prefs = PrefServiceBase::FromBrowserContext(profile());

  // Allow this test to control what should get synced.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  // Always set password generation enabled check box so we can test the
  // behavior of password sync.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);

  // Sync some things, but not passwords. Shouldn't send anything since
  // password generation is disabled by default.
  ProfileSyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      profile());
  sync_service->SetSyncSetupCompleted();
  syncer::ModelTypeSet preferred_set;
  preferred_set.Put(syncer::EXTENSIONS);
  preferred_set.Put(syncer::PREFERENCES);
  sync_service->ChangePreferredDataTypes(preferred_set);
  syncer::ModelTypeSet new_set = sync_service->GetPreferredDataTypes();
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(0u, autofill_manager_->GetSentStates().size());

  // Now sync passwords.
  preferred_set.Put(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(preferred_set);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(1u, autofill_manager_->GetSentStates().size());
  EXPECT_TRUE(autofill_manager_->GetSentStates()[0]);
  autofill_manager_->ClearSentStates();

  // Add some additional synced state. Nothing should be sent.
  preferred_set.Put(syncer::THEMES);
  sync_service->ChangePreferredDataTypes(preferred_set);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(0u, autofill_manager_->GetSentStates().size());

  // Disable syncing. This should disable the feature.
  sync_service->DisableForUser();
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(1u, autofill_manager_->GetSentStates().size());
  EXPECT_FALSE(autofill_manager_->GetSentStates()[0]);
  autofill_manager_->ClearSentStates();

  // Disable password manager by going incognito, and re-enable syncing. The
  // feature should still be disabled, and nothing will be sent.
  sync_service->SetSyncSetupCompleted();
  profile()->set_incognito(true);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(0u, autofill_manager_->GetSentStates().size());

  // When a new render_view is created, we send the state even if it's the
  // same.
  UpdatePasswordGenerationState(true);
  EXPECT_EQ(1u, autofill_manager_->GetSentStates().size());
  EXPECT_FALSE(autofill_manager_->GetSentStates()[0]);
  autofill_manager_->ClearSentStates();
}

TEST_F(AutofillManagerTest, UpdatePasswordGenerationState) {
  PasswordManagerDelegateImpl::CreateForWebContents(web_contents());
  PasswordManager::CreateForWebContentsAndDelegate(
      web_contents(),
      PasswordManagerDelegateImpl::FromWebContents(web_contents()));

  PrefServiceBase* prefs = PrefServiceBase::FromBrowserContext(profile());

  // Always set password sync enabled so we can test the behavior of password
  // generation.
  prefs->SetBoolean(prefs::kSyncKeepEverythingSynced, false);
  ProfileSyncService* sync_service = ProfileSyncServiceFactory::GetForProfile(
      profile());
  sync_service->SetSyncSetupCompleted();
  syncer::ModelTypeSet preferred_set;
  preferred_set.Put(syncer::PASSWORDS);
  sync_service->ChangePreferredDataTypes(preferred_set);

  // Enabled state remains false, should not sent.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, false);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(0u, autofill_manager_->GetSentStates().size());

  // Enabled state from false to true, should sent true.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(1u, autofill_manager_->GetSentStates().size());
  EXPECT_TRUE(autofill_manager_->GetSentStates()[0]);
  autofill_manager_->ClearSentStates();

  // Enabled states remains true, should not sent.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, true);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(0u, autofill_manager_->GetSentStates().size());

  // Enabled states from true to false, should sent false.
  prefs->SetBoolean(prefs::kPasswordGenerationEnabled, false);
  UpdatePasswordGenerationState(false);
  EXPECT_EQ(1u, autofill_manager_->GetSentStates().size());
  EXPECT_FALSE(autofill_manager_->GetSentStates()[0]);
  autofill_manager_->ClearSentStates();

  // When a new render_view is created, we send the state even if it's the
  // same.
  UpdatePasswordGenerationState(true);
  EXPECT_EQ(1u, autofill_manager_->GetSentStates().size());
  EXPECT_FALSE(autofill_manager_->GetSentStates()[0]);
  autofill_manager_->ClearSentStates();
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

TEST_F(AutofillManagerTest, DisabledAutofillDispatchesError) {
  EXPECT_TRUE(autofill_manager_->request_autocomplete_results().empty());

  autofill_manager_->set_autofill_enabled(false);
  autofill_manager_->OnRequestAutocomplete(FormData(),
                                           GURL(),
                                           content::SSLStatus());

  EXPECT_EQ(1U, autofill_manager_->request_autocomplete_results().size());
  EXPECT_EQ(WebFormElement::AutocompleteResultErrorDisabled,
            autofill_manager_->request_autocomplete_results()[0].first);
}

namespace {

class MockAutofillExternalDelegate :
      public autofill::TestAutofillExternalDelegate {
 public:
  explicit MockAutofillExternalDelegate(content::WebContents* web_contents,
                                        AutofillManager* autofill_manager)
      : TestAutofillExternalDelegate(web_contents, autofill_manager) {}
  virtual ~MockAutofillExternalDelegate() {}

  MOCK_METHOD5(OnQuery, void(int query_id,
                             const FormData& form,
                             const FormFieldData& field,
                             const gfx::Rect& bounds,
                             bool display_warning));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillExternalDelegate);
};

}  // namespace

// Test our external delegate is called at the right time.
TEST_F(AutofillManagerTest, TestExternalDelegate) {
  MockAutofillExternalDelegate external_delegate(web_contents(),
                                                 autofill_manager_);
  EXPECT_CALL(external_delegate, OnQuery(_, _, _, _, _));
  autofill_manager_->SetExternalDelegate(&external_delegate);

  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);
  const FormFieldData& field = form.fields[0];
  GetAutofillSuggestions(form, field);  // should call the delegate's OnQuery()

  autofill_manager_->SetExternalDelegate(NULL);
}
