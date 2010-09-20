// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/ref_counted.h"
#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/tuple.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/browser/tab_contents/test_tab_contents.h"
#include "chrome/common/ipc_test_sink.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;

typedef Tuple5<int,
               std::vector<string16>,
               std::vector<string16>,
               std::vector<string16>,
               std::vector<int> > AutoFillParam;

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    CreateTestAutoFillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  virtual void InitializeIfNeeded() {}
  virtual void SaveImportedFormData() {}
  virtual bool IsDataLoaded() const { return true; }

  AutoFillProfile* GetLabeledProfile(const char* label) {
    for (std::vector<AutoFillProfile *>::iterator it = web_profiles_.begin();
         it != web_profiles_.end(); ++it) {
       if (!(*it)->Label().compare(ASCIIToUTF16(label)))
         return *it;
    }
    return NULL;
  }

  void AddProfile(AutoFillProfile* profile) {
    web_profiles_->push_back(profile);
  }

 private:
  void CreateTestAutoFillProfiles(ScopedVector<AutoFillProfile>* profiles) {
    AutoFillProfile* profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Home", "Elvis", "Aaron",
                                      "Presley", "theking@gmail.com", "RCA",
                                      "3734 Elvis Presley Blvd.", "Apt. 10",
                                      "Memphis", "Tennessee", "38116", "USA",
                                      "12345678901", "");
    profile->set_unique_id(1);
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Work", "Charles", "Hardin",
                                      "Holley", "buddy@gmail.com", "Decca",
                                      "123 Apple St.", "unit 6", "Lubbock",
                                      "Texas", "79401", "USA", "23456789012",
                                      "");
    profile->set_unique_id(2);
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Empty", "", "", "", "", "", "",
                                      "", "", "", "", "", "", "");
    profile->set_unique_id(3);
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "First", "Elvis Presley",
                                         "Visa", "1234567890123456", "04",
                                         "2012", 1);
    credit_card->set_unique_id(4);
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "Second", "Buddy Holly",
                                         "Mastercard", "0987654321098765", "10",
                                         "2014", 2);
    credit_card->set_unique_id(5);
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "Empty", "", "", "", "",
                                         "", 3);
    credit_card->set_unique_id(6);
    credit_cards->push_back(credit_card);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestAutoFillManager : public AutoFillManager {
 public:
  TestAutoFillManager(TabContents* tab_contents,
                      TestPersonalDataManager* personal_manager)
      : AutoFillManager(tab_contents, NULL) {
    test_personal_data_ = personal_manager;
    set_personal_data_manager(personal_manager);
    // Download manager requests are disabled for purposes of this unit-test.
    // These request are tested in autofill_download_unittest.cc.
    set_disable_download_manager_requests(true);
  }

  virtual bool IsAutoFillEnabled() const { return true; }

  AutoFillProfile* GetLabeledProfile(const char* label) {
    return test_personal_data_->GetLabeledProfile(label);
  }

  void AddProfile(AutoFillProfile* profile) {
    test_personal_data_->AddProfile(profile);
  }

 private:
  TestPersonalDataManager* test_personal_data_;

  DISALLOW_COPY_AND_ASSIGN(TestAutoFillManager);
};

void CreateTestFormData(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");
  form->user_submitted = true;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "City", "city", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "State", "state", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Country", "country", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email", "email", "", "text", &field);
  form->fields.push_back(field);
}

void CreateTestFormDataBilling(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("https://myform.com/form.html");
  form->action = GURL("https://myform.com/submit.html");
  form->user_submitted = true;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Address Line 1", "billingAddr1", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Address Line 2", "billingAddr2", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "City", "billingCity", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "State", "billingState", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Postal Code", "billingZipcode", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Country", "billingCountry", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email", "email", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Name on Card", "nameoncard", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Expiration Date", "ccmonth", "", "text", &field);
  form->fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "", "ccyear", "", "text", &field);
  form->fields.push_back(field);
}

class AutoFillManagerTest : public RenderViewHostTestHarness {
 public:
  AutoFillManagerTest() {}
  virtual ~AutoFillManagerTest() {
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

  Profile* profile() { return contents()->profile(); }

  bool GetAutoFillSuggestionsMessage(int* page_id,
                                     std::vector<string16>* values,
                                     std::vector<string16>* labels) {
    const uint32 kMsgID = ViewMsg_AutoFillSuggestionsReturned::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;

    AutoFillParam autofill_param;
    ViewMsg_AutoFillSuggestionsReturned::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (values)
      *values = autofill_param.b;
    if (labels)
      *labels = autofill_param.c;
    return true;
  }

  bool GetAutoFillFormDataFilledMessage(int *page_id, FormData* results) {
    const uint32 kMsgID = ViewMsg_AutoFillFormDataFilled::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple2<int, FormData> autofill_param;
    ViewMsg_AutoFillFormDataFilled::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (results)
      *results = autofill_param.b;
    return true;
  }

 protected:
  scoped_ptr<TestAutoFillManager> autofill_manager_;
  scoped_refptr<TestPersonalDataManager> test_personal_data_;

 private:
  DISALLOW_COPY_AND_ASSIGN(AutoFillManagerTest);
};

TEST_F(AutoFillManagerTest, GetProfileSuggestionsEmptyValue) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles"), values[1]);
  ASSERT_EQ(2U, labels.size());
  // Inferred labels now include full first relevant field, which in this case
  // the address #1.
  EXPECT_EQ(ASCIIToUTF16("3734 Elvis Presley Blvd."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("123 Apple St."), labels[1]);
}

TEST_F(AutoFillManagerTest, GetProfileSuggestionsMatchCharacter) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "E", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("3734 Elvis Presley Blvd."), labels[0]);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsEmptyValue) {
  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Card Number", "cardnumber", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(6U, values.size());
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[2]);
  EXPECT_EQ(ASCIIToUTF16("************8765"), values[3]);
  EXPECT_EQ(ASCIIToUTF16("************8765"), values[4]);
  EXPECT_EQ(ASCIIToUTF16("************8765"), values[5]);
  ASSERT_EQ(6U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *3456"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *3456"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("*3456"), labels[2]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *8765"), labels[3]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *8765"), labels[4]);
  EXPECT_EQ(ASCIIToUTF16("*8765"), labels[5]);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsMatchCharacter) {
  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Card Number", "cardnumber", "1", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(3U, values.size());
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[2]);
  ASSERT_EQ(3U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *3456"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *3456"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("*3456"), labels[2]);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsNonCCNumber) {
  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Name on Card", "nameoncard", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(6U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[2]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[3]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[4]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[5]);
  ASSERT_EQ(6U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *3456"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *3456"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("*3456"), labels[2]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *8765"), labels[3]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *8765"), labels[4]);
  EXPECT_EQ(ASCIIToUTF16("*8765"), labels[5]);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsSemicolon) {
  // |profile| will be owned by the mock PersonalDataManager.
  AutoFillProfile* profile = new AutoFillProfile;
  autofill_unittest::SetProfileInfo(profile, "Home; 8765", "Joe", "", "Ely",
                                    "flatlander@gmail.com", "MCA",
                                    "916 16th St.", "Apt. 6", "Lubbock",
                                    "Texas", "79401", "USA",
                                    "12345678901", "");
  autofill_manager_->AddProfile(profile);

  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Name on Card", "nameoncard", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(8U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[2]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Presley"), values[3]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[4]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[5]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[6]);
  EXPECT_EQ(ASCIIToUTF16("Buddy Holly"), values[7]);
  ASSERT_EQ(8U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *3456"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *3456"), labels[1]);
  EXPECT_EQ(ASCIIToUTF16("*3456"), labels[2]);
  EXPECT_EQ(ASCIIToUTF16("Joe Ely; *3456"), labels[3]);
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *8765"), labels[4]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *8765"), labels[5]);
  EXPECT_EQ(ASCIIToUTF16("*8765"), labels[6]);
  EXPECT_EQ(ASCIIToUTF16("Joe Ely; *8765"), labels[7]);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsNonHTTPS) {
  FormData form;
  CreateTestFormDataBilling(&form);
  form.origin = GURL("http://myform.com/form.html");

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Card Number", "cardnumber", "", "text", &field);
  EXPECT_FALSE(
      autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));
}

TEST_F(AutoFillManagerTest, GetCombinedAutoFillAndAutocompleteSuggestions) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // Add some Autocomplete suggestions.
  // This triggers the combined message send.
  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("Jay"));
  suggestions.push_back(ASCIIToUTF16("Jason"));
  rvh()->AutocompleteSuggestionsReturned(kPageID, suggestions);

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(4U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles"), values[1]);
  EXPECT_EQ(ASCIIToUTF16("Jay"), values[2]);
  EXPECT_EQ(ASCIIToUTF16("Jason"), values[3]);
  ASSERT_EQ(4U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("3734 Elvis Presley Blvd."), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("123 Apple St."), labels[1]);
  EXPECT_EQ(string16(), labels[2]);
  EXPECT_EQ(string16(), labels[3]);
}

TEST_F(AutoFillManagerTest, GetFieldSuggestionsFormIsAutoFilled) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, true, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles"), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);
}

TEST_F(AutoFillManagerTest, GetFieldSuggestionsForAutocompleteOnly) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Some Field", "somefield", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, true, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  // In this case, we're simulating a cancel of Autocomplete with a different
  // page ID and an empty vector of suggestions.
  std::vector<string16> suggestions;
  suggestions.push_back(ASCIIToUTF16("one"));
  suggestions.push_back(ASCIIToUTF16("two"));
  rvh()->AutocompleteSuggestionsReturned(kPageID, suggestions);

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("one"), values[0]);
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("two"), values[1]);
  EXPECT_EQ(string16(), labels[1]);
}

TEST_F(AutoFillManagerTest, GetFieldSuggestionsWithDuplicateValues) {
  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // |profile| will be owned by the mock PersonalDataManager.
  AutoFillProfile* profile = new AutoFillProfile;
  autofill_unittest::SetProfileInfo(profile, "Duplicate", "Elvis", "", "", "",
                                    "", "", "", "", "", "", "", "", "");
  autofill_manager_->AddProfile(profile);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, true, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles"), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(string16(), labels[0]);
  EXPECT_EQ(string16(), labels[1]);
}

TEST_F(AutoFillManagerTest, GetBillingSuggestionsAddress1) {
  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "Address Line 1", "billingAddr1", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, false, field));

  // No suggestions provided, so send an empty vector as the results.
  // This triggers the combined message send.
  rvh()->AutocompleteSuggestionsReturned(kPageID, std::vector<string16>());

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("3734 Elvis Presley Blvd."), values[0]);
  EXPECT_EQ(ASCIIToUTF16("123 Apple St."), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis Aaron Presley; *3456"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles Hardin Holley; *8765"), labels[1]);
}

TEST_F(AutoFillManagerTest, FillCreditCardForm) {
  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(autofill_manager_->FillAutoFillFormData(
      kPageID, form, AutoFillManager::PackIDs(4, 1)));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("https://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("https://myform.com/submit.html"), results.action);
  ASSERT_EQ(15U, results.fields.size());

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "Elvis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "Aaron", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  autofill_unittest::CreateTestFormField(
      "Address Line 1", "billingAddr1",
                      "3734 Elvis Presley Blvd.", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  autofill_unittest::CreateTestFormField(
      "Address Line 2", "billingAddr2", "Apt. 10", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
  autofill_unittest::CreateTestFormField(
      "City", "billingCity", "Memphis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[5]));
  autofill_unittest::CreateTestFormField(
      "State", "billingState", "Tennessee", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[6]));
  autofill_unittest::CreateTestFormField(
      "Postal Code", "billingZipcode", "38116", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[7]));
  autofill_unittest::CreateTestFormField(
      "Country", "billingCountry", "USA", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[8]));
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "12345678901", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[9]));
  autofill_unittest::CreateTestFormField(
      "Email", "email", "theking@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[10]));
  autofill_unittest::CreateTestFormField(
      "Name on Card", "nameoncard", "Elvis Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[11]));
  autofill_unittest::CreateTestFormField(
      "Card Number", "cardnumber", "1234567890123456", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[12]));
  autofill_unittest::CreateTestFormField(
      "Expiration Date", "ccmonth", "04", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[13]));
  autofill_unittest::CreateTestFormField(
      "", "ccyear", "2012", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[14]));
}

TEST_F(AutoFillManagerTest, FillNonBillingFormSemicolon) {
  // |profile| will be owned by the mock PersonalDataManager.
  AutoFillProfile* profile = new AutoFillProfile;
  autofill_unittest::SetProfileInfo(profile, "Home; 8765", "Joe", "", "Ely",
                                    "flatlander@gmail.com", "MCA",
                                    "916 16th St.", "Apt. 6", "Lubbock",
                                    "Texas", "79401", "USA",
                                    "12345678901", "");
  profile->set_unique_id(7);
  autofill_manager_->AddProfile(profile);

  FormData form;
  CreateTestFormData(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(autofill_manager_->FillAutoFillFormData(
      kPageID, form, AutoFillManager::PackIDs(4, 7)));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  ASSERT_EQ(11U, results.fields.size());

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "Joe", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "Ely", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  autofill_unittest::CreateTestFormField(
      "Address Line 1", "addr1", "916 16th St.", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  autofill_unittest::CreateTestFormField(
      "Address Line 2", "addr2", "Apt. 6", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
  autofill_unittest::CreateTestFormField(
      "City", "city", "Lubbock", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[5]));
  autofill_unittest::CreateTestFormField(
      "State", "state", "Texas", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[6]));
  autofill_unittest::CreateTestFormField(
      "Postal Code", "zipcode", "79401", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[7]));
  autofill_unittest::CreateTestFormField(
      "Country", "country", "USA", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[8]));
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "12345678901", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[9]));
  autofill_unittest::CreateTestFormField(
      "Email", "email", "flatlander@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[10]));
}

TEST_F(AutoFillManagerTest, FillBillFormSemicolon) {
  // |profile| will be owned by the mock PersonalDataManager.
  AutoFillProfile* profile = new AutoFillProfile;
  autofill_unittest::SetProfileInfo(profile, "Home; 8765", "Joe", "", "Ely",
                                    "flatlander@gmail.com", "MCA",
                                    "916 16th St.", "Apt. 6", "Lubbock",
                                    "Texas", "79401", "USA",
                                    "12345678901", "");
  profile->set_unique_id(7);
  autofill_manager_->AddProfile(profile);

  FormData form;
  CreateTestFormDataBilling(&form);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(autofill_manager_->FillAutoFillFormData(
      kPageID, form, AutoFillManager::PackIDs(4, 7)));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("https://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("https://myform.com/submit.html"), results.action);
  ASSERT_EQ(15U, results.fields.size());

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "Joe", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "Ely", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  autofill_unittest::CreateTestFormField(
      "Address Line 1", "billingAddr1",
                      "3734 Elvis Presley Blvd.", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  autofill_unittest::CreateTestFormField(
      "Address Line 2", "billingAddr2", "Apt. 10", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
  autofill_unittest::CreateTestFormField(
      "City", "billingCity", "Memphis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[5]));
  autofill_unittest::CreateTestFormField(
      "State", "billingState", "Tennessee", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[6]));
  autofill_unittest::CreateTestFormField(
      "Postal Code", "billingZipcode", "38116", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[7]));
  autofill_unittest::CreateTestFormField(
      "Country", "billingCountry", "USA", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[8]));
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "12345678901", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[9]));
  autofill_unittest::CreateTestFormField(
      "Email", "email", "flatlander@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[10]));
  autofill_unittest::CreateTestFormField(
      "Name on Card", "nameoncard", "Elvis Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[11]));
  autofill_unittest::CreateTestFormField(
      "Card Number", "cardnumber", "1234567890123456", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[12]));
  autofill_unittest::CreateTestFormField(
      "Expiration Date", "ccmonth", "04", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[13]));
  autofill_unittest::CreateTestFormField(
      "", "ccyear", "2012", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[14]));
}

TEST_F(AutoFillManagerTest, FillPhoneNumber) {
  FormData form;

  form.name = ASCIIToUTF16("MyPhoneForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/phone_form.html");
  form.action = GURL("http://myform.com/phone_submit.html");
  form.user_submitted = true;

  webkit_glue::FormField field;

  autofill_unittest::CreateTestFormField(
      "country code", "country code", "", "text", &field);
  field.set_size(1);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "area code", "area code", "", "text", &field);
  field.set_size(3);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "phone", "phone prefix", "1", "text", &field);
  field.set_size(3);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "-", "phone suffix", "", "text", &field);
  field.set_size(4);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Phone Extension", "ext", "", "text", &field);
  field.set_size(3);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  AutoFillProfile *work_profile = autofill_manager_->GetLabeledProfile("Work");
  EXPECT_TRUE(work_profile != NULL);
  const AutoFillType phone_type(PHONE_HOME_NUMBER);
  string16 saved_phone = work_profile->GetFieldText(phone_type);

  char test_data[] = "1234567890123456";
  for (int i = arraysize(test_data) - 1; i >= 0; --i) {
    test_data[i] = 0;
    work_profile->SetInfo(phone_type, ASCIIToUTF16(test_data));
    // The page ID sent to the AutoFillManager from the RenderView, used to send
    // an IPC message back to the renderer.
    int page_id = 100 - i;
    process()->sink().ClearMessages();
    EXPECT_TRUE(
        autofill_manager_->FillAutoFillFormData(page_id,
                                                form,
                                                work_profile->unique_id()));
    page_id = 0;
    FormData results;
    EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));

    if (i != 7) {
      EXPECT_EQ(ASCIIToUTF16(test_data), results.fields[2].value());
      EXPECT_EQ(ASCIIToUTF16(test_data), results.fields[3].value());
    } else {
      // The only size that is parsed and split, right now is 7:
      EXPECT_EQ(ASCIIToUTF16("123"), results.fields[2].value());
      EXPECT_EQ(ASCIIToUTF16("4567"), results.fields[3].value());
    }
  }

  work_profile->SetInfo(phone_type, saved_phone);
}

TEST_F(AutoFillManagerTest, FormChangesRemoveField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email", "email", "", "text", &field);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // Now, after the call to |FormsSeen| we remove the phone number field before
  // filling.
  form.fields.erase(form.fields.begin() + 3);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(autofill_manager_->FillAutoFillFormData(kPageID, form, 1));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  EXPECT_TRUE(results.user_submitted);
  ASSERT_EQ(4U, results.fields.size());

  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "Elvis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "Aaron", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  autofill_unittest::CreateTestFormField(
      "Email", "email", "theking@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
}

TEST_F(AutoFillManagerTest, FormChangesAddField) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "", "text", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "", "text", &field);
  // Note: absent phone number.  Adding this below.
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "Email", "email", "", "text", &field);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // Now, after the call to |FormsSeen| we add the phone number field before
  // filling.
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "", "text", &field);
  form.fields.insert(form.fields.begin() + 3, field);

  // The page ID sent to the AutoFillManager from the RenderView, used to send
  // an IPC message back to the renderer.
  const int kPageID = 1;
  EXPECT_TRUE(autofill_manager_->FillAutoFillFormData(kPageID, form, 1));

  int page_id = 0;
  FormData results;
  EXPECT_TRUE(GetAutoFillFormDataFilledMessage(&page_id, &results));
  EXPECT_EQ(ASCIIToUTF16("MyForm"), results.name);
  EXPECT_EQ(ASCIIToUTF16("POST"), results.method);
  EXPECT_EQ(GURL("http://myform.com/form.html"), results.origin);
  EXPECT_EQ(GURL("http://myform.com/submit.html"), results.action);
  EXPECT_TRUE(results.user_submitted);
  ASSERT_EQ(5U, results.fields.size());

  autofill_unittest::CreateTestFormField(
      "First Name", "firstname", "Elvis", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[0]));
  autofill_unittest::CreateTestFormField(
      "Middle Name", "middlename", "Aaron", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[1]));
  autofill_unittest::CreateTestFormField(
      "Last Name", "lastname", "Presley", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[2]));
  autofill_unittest::CreateTestFormField(
      "Phone Number", "phonenumber", "", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[3]));
  autofill_unittest::CreateTestFormField(
      "Email", "email", "theking@gmail.com", "text", &field);
  EXPECT_TRUE(field.StrictlyEqualsHack(results.fields[4]));
}

TEST_F(AutoFillManagerTest, HiddenFields) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("http://myform.com/form.html");
  form.action = GURL("http://myform.com/submit.html");
  form.user_submitted = true;

  webkit_glue::FormField field;
  autofill_unittest::CreateTestFormField(
      "E-mail", "one", "one", "hidden", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "E-mail", "two", "two", "hidden", &field);
  form.fields.push_back(field);
  autofill_unittest::CreateTestFormField(
      "E-mail", "three", "three", "hidden", &field);
  form.fields.push_back(field);

  // Set up our FormStructures.
  std::vector<FormData> forms;
  forms.push_back(form);
  autofill_manager_->FormsSeen(forms);

  // Submit the form.
  autofill_manager_->FormSubmitted(form);

  // TODO(jhawkins): We can't use the InfoBar anymore to determine if we saved
  // fields.  Need to query the PDM.
}

// Checks that resetting the auxiliary profile enabled preference does the right
// thing on all platforms.
TEST_F(AutoFillManagerTest, AuxiliaryProfilesReset) {
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

