// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/tuple.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autocomplete_history_manager.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/renderer_host/test_render_view_host.h"
#include "content/browser/tab_contents/test_tab_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;
using webkit_glue::FormField;

namespace {

// The page ID sent to the AutofillManager from the RenderView, used to send
// an IPC message back to the renderer.
const int kDefaultPageID = 137;

typedef Tuple5<int,
               std::vector<string16>,
               std::vector<string16>,
               std::vector<string16>,
               std::vector<int> > AutoFillParam;

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    CreateTestAutofillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  virtual void InitializeIfNeeded() {}
  virtual void SaveImportedFormData() {}
  virtual bool IsDataLoaded() const { return true; }

  AutofillProfile* GetProfileWithGUID(const char* guid) {
    for (std::vector<AutofillProfile *>::iterator it = web_profiles_.begin();
         it != web_profiles_.end(); ++it) {
      if (!(*it)->guid().compare(guid))
        return *it;
    }
    return NULL;
  }

  void AddProfile(AutofillProfile* profile) {
    web_profiles_->push_back(profile);
  }

  void ClearAutofillProfiles() {
    web_profiles_.reset();
  }

  void ClearCreditCards() {
    credit_cards_.reset();
  }

  void CreateTestCreditCardsYearAndMonth(const char* year, const char* month) {
    ClearCreditCards();
    CreditCard* credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Miku Hatsune",
                                     "4234567890654321", // Visa
                                     month, year);
    credit_card->set_guid("00000000-0000-0000-0000-000000000007");
    credit_cards_->push_back(credit_card);
 }

 private:
  void CreateTestAutofillProfiles(ScopedVector<AutofillProfile>* profiles) {
    AutofillProfile* profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "Elvis", "Aaron",
                                  "Presley", "theking@gmail.com", "RCA",
                                  "3734 Elvis Presley Blvd.", "Apt. 10",
                                  "Memphis", "Tennessee", "38116", "USA",
                                  "12345678901", "");
    profile->set_guid("00000000-0000-0000-0000-000000000001");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "Charles", "Hardin",
                                  "Holley", "buddy@gmail.com", "Decca",
                                  "123 Apple St.", "unit 6", "Lubbock",
                                  "Texas", "79401", "USA", "23456789012",
                                  "");
    profile->set_guid("00000000-0000-0000-0000-000000000002");
    profiles->push_back(profile);
    profile = new AutofillProfile;
    autofill_test::SetProfileInfo(profile, "", "", "", "", "", "", "",
                                  "", "", "", "", "", "");
    profile->set_guid("00000000-0000-0000-0000-000000000003");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_test::SetCreditCardInfo(credit_card, "Elvis Presley",
                                     "4234567890123456",  // Visa
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

  FormField field;
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
      "Fax", "fax", "", "text", &field);
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

  FormField field;
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
                      const char* fax,
                      const char* email,
                      const char* name_on_card,
                      const char* card_number,
                      const char* expiration_month,
                      const char* expiration_year,
                      bool has_address_fields,
                      bool has_credit_card_fields,
                      bool use_month_type) {
  // The number of fields in the address and credit card forms created above.
  const size_t kAddressFormSize = 12;
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

  FormField field;
  if (has_address_fields) {
    autofill_test::CreateTestFormField(
        "First Name", "firstname", first, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[0]));
    autofill_test::CreateTestFormField(
        "Middle Name", "middlename", middle, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[1]));
    autofill_test::CreateTestFormField(
        "Last Name", "lastname", last, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[2]));
    autofill_test::CreateTestFormField(
        "Address Line 1", "addr1", address1, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[3]));
    autofill_test::CreateTestFormField(
        "Address Line 2", "addr2", address2, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[4]));
    autofill_test::CreateTestFormField(
        "City", "city", city, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[5]));
    autofill_test::CreateTestFormField(
        "State", "state", state, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[6]));
    autofill_test::CreateTestFormField(
        "Postal Code", "zipcode", postal_code, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[7]));
    autofill_test::CreateTestFormField(
        "Country", "country", country, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[8]));
    autofill_test::CreateTestFormField(
        "Phone Number", "phonenumber", phone, "tel", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[9]));
    autofill_test::CreateTestFormField(
        "Fax", "fax", fax, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[10]));
    autofill_test::CreateTestFormField(
        "Email", "email", email, "email", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[11]));
  }

  if (has_credit_card_fields) {
    size_t offset = has_address_fields? kAddressFormSize : 0;
    autofill_test::CreateTestFormField(
        "Name on Card", "nameoncard", name_on_card, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[offset + 0]));
    autofill_test::CreateTestFormField(
        "Card Number", "cardnumber", card_number, "text", &field);
    EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[offset + 1]));
    if (use_month_type) {
      std::string exp_year = expiration_year;
      std::string exp_month = expiration_month;
      std::string date;
      if (!exp_year.empty() && !exp_month.empty())
        date = exp_year + "-" + exp_month;
      autofill_test::CreateTestFormField(
          "Expiration Date", "ccmonth", date.c_str(), "month", &field);
      EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[offset + 2]));
    } else {
      autofill_test::CreateTestFormField(
          "Expiration Date", "ccmonth", expiration_month, "text", &field);
      EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[offset + 2]));
      autofill_test::CreateTestFormField(
          "", "ccyear", expiration_year, "text", &field);
      EXPECT_TRUE(field.StrictlyEqualsHack(filled_form.fields[offset + 3]));
    }
  }
}

void ExpectFilledAddressFormElvis(int page_id,
                                  const FormData& filled_form,
                                  int expected_page_id,
                                  bool has_credit_card_fields) {
  ExpectFilledForm(page_id, filled_form, expected_page_id, "Elvis", "Aaron",
                   "Presley", "3734 Elvis Presley Blvd.", "Apt. 10", "Memphis",
                   "Tennessee", "38116", "United States", "12345678901", "",
                   "theking@gmail.com", "", "", "", "", true,
                   has_credit_card_fields, false);
}

void ExpectFilledCreditCardFormElvis(int page_id,
                                     const FormData& filled_form,
                                     int expected_page_id,
                                     bool has_address_fields) {
  ExpectFilledForm(page_id, filled_form, expected_page_id,
                   "", "", "", "", "", "", "", "", "", "", "", "",
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
                   "", "", "", "", "", "", "", "", "", "", "", "",
                   "Miku Hatsune", "4234567890654321", month, year,
                   has_address_fields, true, true);
}

class TestAutofillManager : public AutofillManager {
 public:
  TestAutofillManager(TabContents* tab_contents,
                      TestPersonalDataManager* personal_manager)
      : AutofillManager(tab_contents, personal_manager),
        autofill_enabled_(true) {
    test_personal_data_ = personal_manager;
  }

  virtual bool IsAutoFillEnabled() const { return autofill_enabled_; }

  void set_autofill_enabled(bool autofill_enabled) {
    autofill_enabled_ = autofill_enabled;
  }

  AutofillProfile* GetProfileWithGUID(const char* guid) {
    return test_personal_data_->GetProfileWithGUID(guid);
  }

  void AddProfile(AutofillProfile* profile) {
    test_personal_data_->AddProfile(profile);
  }

  int GetPackedCreditCardID(int credit_card_id) {
    return PackGUIDs(IDToGUID(credit_card_id), std::string());
  }

  virtual int GUIDToID(const std::string& guid) OVERRIDE {
    if (guid.empty())
      return 0;

    int id;
    EXPECT_TRUE(base::StringToInt(guid.substr(guid.rfind("-") + 1), &id));
    return id;
  }

  virtual const std::string IDToGUID(int id) OVERRIDE {
    EXPECT_TRUE(id >= 0);
    if (id <= 0)
      return std::string();

    return base::StringPrintf("00000000-0000-0000-0000-%012d", id);
  }

 private:
  TestPersonalDataManager* test_personal_data_;
  bool autofill_enabled_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillManager);
};

}  // namespace

class AutofillManagerTest : public RenderViewHostTestHarness {
 public:
  AutofillManagerTest() {}
  virtual ~AutofillManagerTest() {
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

  Profile* profile() { return contents()->profile(); }

  void GetAutoFillSuggestions(int query_id,
                              const webkit_glue::FormData& form,
                              const webkit_glue::FormField& field) {
    autofill_manager_->OnQueryFormFieldAutoFill(query_id, form, field);
  }

  void GetAutoFillSuggestions(const webkit_glue::FormData& form,
                              const webkit_glue::FormField& field) {
    GetAutoFillSuggestions(kDefaultPageID, form, field);
  }

  void AutocompleteSuggestionsReturned(const std::vector<string16>& result) {
    autofill_manager_->tab_contents()->autocomplete_history_manager()->
        SendSuggestions(&result);
  }

  void FormsSeen(const std::vector<webkit_glue::FormData>& forms) {
    autofill_manager_->OnFormsSeen(forms);
  }

  void FillAutoFillFormData(int query_id,
                            const webkit_glue::FormData& form,
                            const webkit_glue::FormField& field,
                            int unique_id) {
    autofill_manager_->OnFillAutoFillFormData(query_id, form, field, unique_id);
  }

  bool GetAutoFillSuggestionsMessage(int* page_id,
                                     std::vector<string16>* values,
                                     std::vector<string16>* labels,
                                     std::vector<string16>* icons,
                                     std::vector<int>* unique_ids) {
    const uint32 kMsgID = AutoFillMsg_SuggestionsReturned::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;

    AutoFillParam autofill_param;
    AutoFillMsg_SuggestionsReturned::Read(message, &autofill_param);
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

    autofill_manager_->tab_contents()->autocomplete_history_manager()->
        CancelPendingQuery();
    process()->sink().ClearMessages();
    return true;
  }

  bool GetAutoFillFormDataFilledMessage(int *page_id, FormData* results) {
    const uint32 kMsgID = AutoFillMsg_FormDataFilled::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple2<int, FormData> autofill_param;
    AutoFillMsg_FormDataFilled::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (results)
      *results = autofill_param.b;

    process()->sink().ClearMessages();
    return true;
  }

 protected:
  scoped_ptr<TestAutofillManager> autofill_manager_;
  scoped_refptr<TestPersonalDataManager> test_personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillManagerTest);
};

// Test that we return all address profile suggestions when all form fields are
// empty.
TEST_F(AutofillManagerTest, GetProfileSuggestionsEmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  GetAutoFillSuggestionsMessage(
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

  FormField field;
  autofill_test::CreateTestFormField("First Name", "firstname", "E", "text",
                                     &field);
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

  FormField field;
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

  GetAutoFillSuggestions(form, field);
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
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

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

  // Disable AutoFill.
  autofill_manager_->set_autofill_enabled(false);

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
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

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
                                            &unique_ids));

  string16 expected_values[] = {
    l10n_util::GetStringUTF16(IDS_AUTOFILL_WARNING_FORM_DISABLED)
  };
  string16 expected_labels[] = {string16()};
  string16 expected_icons[] = {string16()};
  int expected_unique_ids[] = {-1};
  ExpectSuggestions(page_id, values, labels, icons, unique_ids,
                    kDefaultPageID, arraysize(expected_values), expected_values,
                    expected_labels, expected_icons, expected_unique_ids);

  // Now add some Autocomplete suggestions. We should return the autocomplete
  // suggestions and the warning; these will be culled by the renderer.
  const int kPageID2 = 2;
  GetAutoFillSuggestions(kPageID2, form, field);

  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  test_personal_data_->ClearAutofillProfiles();
  GetAutoFillSuggestions(form, field);
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we return all credit card profile suggestions when all form fields
// are empty.
TEST_F(AutofillManagerTest, GetCreditCardSuggestionsEmptyValue) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormField field = form.fields[1];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

  FormField field;
  autofill_test::CreateTestFormField(
      "Card Number", "cardnumber", "4", "text", &field);
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  GetAutoFillSuggestions(kPageID2, form, field);

  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  AutocompleteSuggestionsReturned(suggestions);

  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  test_personal_data_->ClearCreditCards();
  GetAutoFillSuggestions(form, field);
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we return profile and credit card suggestions for combined forms.
TEST_F(AutofillManagerTest, GetAddressAndCreditCardSuggestions) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  FormField field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right address suggestions to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  GetAutoFillSuggestions(kPageID2, form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the credit card suggestions to the renderer.
  page_id = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

  FormField field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right address suggestions to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  GetAutoFillSuggestions(kPageID2, form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  test_personal_data_->ClearCreditCards();
  GetAutoFillSuggestions(form, field);
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(NULL, NULL, NULL, NULL, NULL));
}

// Test that we correctly combine autofill and autocomplete suggestions.
TEST_F(AutofillManagerTest, GetCombinedAutoFillAndAutocompleteSuggestions) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

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
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
TEST_F(AutofillManagerTest, GetFieldSuggestionsWhenFormIsAutoFilled) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Mark one of the fields as filled.
  form.fields[2].is_autofilled = true;
  const FormField& field = form.fields[0];
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  FormField field;
  autofill_test::CreateTestFormField(
      "Some Field", "somefield", "", "text", &field);
  form.fields.push_back(field);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  GetAutoFillSuggestions(form, field);

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
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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
  autofill_test::SetProfileInfo(profile, "Elvis", "", "", "", "",
                                "", "", "", "", "", "", "", "");
  profile->set_guid("00000000-0000-0000-0000-000000000101");
  autofill_manager_->AddProfile(profile);

  FormField& field = form.fields[0];
  field.is_autofilled = true;
  GetAutoFillSuggestions(form, field);

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  AutocompleteSuggestionsReturned(std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  std::vector<string16> icons;
  std::vector<int> unique_ids;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &icons,
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

// Test that we correctly fill an address form.
TEST_F(AutofillManagerTest, FillAddressForm) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(
      kDefaultPageID, form, form.fields[0],
      autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we correctly fill a credit card form.
TEST_F(AutofillManagerTest, FillCreditCardForm) {
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::string guid = "00000000-0000-0000-0000-000000000004";
  FillAutoFillFormData(
      kDefaultPageID, form, *form.fields.begin(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we correctly fill a credit card form with month input type.
// 1. year empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardFormNoYearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  test_personal_data_->CreateTestCreditCardsYearAndMonth("", "");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::string guid = "00000000-0000-0000-0000-000000000007";
  FillAutoFillFormData(
      kDefaultPageID, form, *form.fields.begin(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
      kDefaultPageID, false, "", "");
}


// Test that we correctly fill a credit card form with month input type.
// 2. year empty, month non-empty
TEST_F(AutofillManagerTest, FillCreditCardFormNoYearMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  test_personal_data_->CreateTestCreditCardsYearAndMonth("", "04");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::string guid = "00000000-0000-0000-0000-000000000007";
  FillAutoFillFormData(
      kDefaultPageID, form, *form.fields.begin(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
      kDefaultPageID, false, "", "04");
}

// Test that we correctly fill a credit card form with month input type.
// 3. year non-empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardFormYearNoMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  test_personal_data_->CreateTestCreditCardsYearAndMonth("2012", "");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::string guid = "00000000-0000-0000-0000-000000000007";
  FillAutoFillFormData(
      kDefaultPageID, form, *form.fields.begin(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledCreditCardYearMonthWithYearMonth(page_id, results,
      kDefaultPageID, false, "2012", "");
}

// Test that we correctly fill a credit card form with month input type.
// 4. year non-empty, month empty
TEST_F(AutofillManagerTest, FillCreditCardFormYearMonth) {
  // Same as the SetUp(), but generate 4 credit cards with year month
  // combination.
  test_personal_data_->ClearCreditCards();
  test_personal_data_->CreateTestCreditCardsYearAndMonth("2012", "04");
  // Set up our form data.
  FormData form;
  CreateTestCreditCardFormData(&form, true, true);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  std::string guid = "00000000-0000-0000-0000-000000000007";
  FillAutoFillFormData(
      kDefaultPageID, form, *form.fields.begin(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
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
  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(kDefaultPageID, form, form.fields[0],
                       autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Address");
    ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, true);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  guid = "00000000-0000-0000-0000-000000000004";
  FillAutoFillFormData(
      kPageID2, form, form.fields.back(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  page_id = 0;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
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
  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(kDefaultPageID, form, form.fields[0],
                       autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
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
  guid = "00000000-0000-0000-0000-000000000001";
  ASSERT_LT(9U, kAddressFormSize);
  FillAutoFillFormData(kPageID2, form, form.fields[kAddressFormSize + 9],
                       autofill_manager_->PackGUIDs(std::string(), guid));

  page_id = 0;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
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

// Test that we correctly fill a form that has a single logical section with
// multiple email address fields.
TEST_F(AutofillManagerTest, FillFormWithMultipleEmails) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  FormField field;
  autofill_test::CreateTestFormField(
      "Confirm email", "email2", "", "text", &field);
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Fill the form.
  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(kDefaultPageID, form, form.fields[0],
                       autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));

  // The second email address should be filled.
  EXPECT_EQ(ASCIIToUTF16("theking@gmail.com"), results.fields.back().value);

  // The remainder of the form should be filled as usual.
  results.fields.pop_back();
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we correctly fill a previously auto-filled form.
TEST_F(AutofillManagerTest, FillAutoFilledForm) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);
  // Mark one of the address fields as autofilled.
  form.fields[4].is_autofilled = true;
  CreateTestCreditCardFormData(&form, true, false);
  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // First fill the address data.
  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(
      kDefaultPageID, form, *form.fields.begin(),
      autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Address");
    ExpectFilledForm(page_id, results, kDefaultPageID,
                     "Elvis", "", "", "", "", "", "", "", "", "", "", "",
                     "", "", "", "", true, true, false);
  }

  // Now fill the credit card data.
  const int kPageID2 = 2;
  guid = "00000000-0000-0000-0000-000000000004";
  FillAutoFillFormData(
      kPageID2, form, form.fields.back(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  page_id = 0;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Credit card 1");
    ExpectFilledCreditCardFormElvis(page_id, results, kPageID2, true);
  }

  // Now set the credit card fields to also be auto-filled, and try again to
  // fill the credit card data
  for (std::vector<FormField>::iterator iter = form.fields.begin();
       iter != form.fields.end();
       ++iter) {
    iter->is_autofilled = true;
  }

  const int kPageID3 = 3;
  FillAutoFillFormData(
      kPageID3, form, *form.fields.rbegin(),
      autofill_manager_->PackGUIDs(guid, std::string()));

  page_id = 0;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  {
    SCOPED_TRACE("Credit card 2");
    ExpectFilledForm(page_id, results, kPageID3,
                   "", "", "", "", "", "", "", "", "", "", "", "",
                   "", "", "", "2012", true, true, false);
  }
}

// Test that we correctly fill a phone number split across multiple fields.
TEST_F(AutofillManagerTest, FillPhoneNumber) {
  // Set up our form data.
  FormData form;
  form.name = ASCIIToUTF16("MyPhoneForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/phone_form.html");
  form.action = GURL("http://myform.com/phone_submit.html");
  form.user_submitted = true;

  FormField field;
  autofill_test::CreateTestFormField(
      "country code", "country code", "", "text", &field);
  field.max_length = 1;
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "area code", "area code", "", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "phone", "phone prefix", "1", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "-", "phone suffix", "", "text", &field);
  field.max_length = 4;
  form.fields.push_back(field);
  autofill_test::CreateTestFormField(
      "Phone Extension", "ext", "", "text", &field);
  field.max_length = 3;
  form.fields.push_back(field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  AutofillProfile *work_profile = autofill_manager_->GetProfileWithGUID(
      "00000000-0000-0000-0000-000000000002");
  ASSERT_TRUE(work_profile != NULL);
  const AutofillType phone_type(PHONE_HOME_NUMBER);
  string16 saved_phone = work_profile->GetFieldText(phone_type);

  char test_data[] = "1234567890123456";
  for (int i = arraysize(test_data) - 1; i >= 0; --i) {
    test_data[i] = 0;
    SCOPED_TRACE(StringPrintf("Testing phone: %s", test_data));
    work_profile->SetInfo(phone_type, ASCIIToUTF16(test_data));
    // The page ID sent to the AutofillManager from the RenderView, used to send
    // an IPC message back to the renderer.
    int page_id = 100 - i;
    FillAutoFillFormData(
        page_id, form, *form.fields.begin(),
        autofill_manager_->PackGUIDs(std::string(), work_profile->guid()));
    page_id = 0;
    FormData results;
    EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));

    if (i != 7) {
      EXPECT_EQ(ASCIIToUTF16(test_data), results.fields[2].value);
      EXPECT_EQ(ASCIIToUTF16(test_data), results.fields[3].value);
    } else {
      // The only size that is parsed and split, right now is 7:
      EXPECT_EQ(ASCIIToUTF16("123"), results.fields[2].value);
      EXPECT_EQ(ASCIIToUTF16("4567"), results.fields[3].value);
    }
  }

  work_profile->SetInfo(phone_type, saved_phone);
}

// Test that we can still fill a form when a field has been removed from it.
TEST_F(AutofillManagerTest, FormChangesRemoveField) {
  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);

  // Add a field -- we'll remove it again later.
  FormField field;
  autofill_test::CreateTestFormField("Some", "field", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we remove the field before filling.
  form.fields.erase(form.fields.begin() + 3);

  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(
      kDefaultPageID, form, form.fields[0],
      autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Test that we can still fill a form when a field has been added to it.
TEST_F(AutofillManagerTest, FormChangesAddField) {
  // The offset of the fax field in the address form.
  const int kFaxFieldOffset = 10;

  // Set up our form data.
  FormData form;
  CreateTestAddressFormData(&form);

  // Remove the fax field -- we'll add it back later.
  std::vector<FormField>::iterator pos = form.fields.begin() + kFaxFieldOffset;
  FormField field = *pos;
  pos = form.fields.erase(pos);

  std::vector<FormData> forms(1, form);
  FormsSeen(forms);

  // Now, after the call to |FormsSeen|, we restore the field before filling.
  form.fields.insert(pos, field);

  std::string guid = "00000000-0000-0000-0000-000000000001";
  FillAutoFillFormData(
      kDefaultPageID, form, form.fields[0],
      autofill_manager_->PackGUIDs(std::string(), guid));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  ExpectFilledAddressFormElvis(page_id, results, kDefaultPageID, false);
}

// Checks that resetting the auxiliary profile enabled preference does the right
// thing on all platforms.
TEST_F(AutofillManagerTest, AuxiliaryProfilesReset) {
#if defined(OS_MACOSX)
  // Auxiliary profiles is implemented on Mac only.  It enables Mac Address
  // Book integration.
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
  profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, false);
  profile()->GetPrefs()->ClearPref(prefs::kAutoFillAuxiliaryProfilesEnabled);
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
#else
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
  profile()->GetPrefs()->SetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled, true);
  profile()->GetPrefs()->ClearPref(prefs::kAutoFillAuxiliaryProfilesEnabled);
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      prefs::kAutoFillAuxiliaryProfilesEnabled));
#endif
}
