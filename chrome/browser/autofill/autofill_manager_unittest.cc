// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/scoped_ptr.h"
#include "base/scoped_vector.h"
#include "base/string16.h"
#include "base/tuple.h"
#include "chrome/browser/autofill/autofill_common_unittest.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/renderer_host/test/test_render_view_host.h"
#include "chrome/common/ipc_test_sink.h"
#include "chrome/common/render_messages.h"
#include "googleurl/src/gurl.h"
#include "webkit/glue/form_data.h"
#include "webkit/glue/form_field.h"

using webkit_glue::FormData;

namespace {

class TestPersonalDataManager : public PersonalDataManager {
 public:
  TestPersonalDataManager() {
    CreateTestAutoFillProfiles(&web_profiles_);
    CreateTestCreditCards(&credit_cards_);
  }

  virtual void InitializeIfNeeded() {}

 private:
  void CreateTestAutoFillProfiles(ScopedVector<AutoFillProfile>* profiles) {
    AutoFillProfile* profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Home", "Elvis", "Aaron",
                                      "Presley", "theking@gmail.com", "RCA",
                                      "3734 Elvis Presley Blvd.", "Apt. 10",
                                      "Memphis", "Tennessee", "38116", "USA",
                                      "12345678901", "");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Work", "Charles", "Hardin",
                                      "Holley", "buddy@gmail.com", "Decca",
                                      "123 Apple St.", "unit 6", "Lubbock",
                                      "Texas", "79401", "USA", "23456789012",
                                      "");
    profiles->push_back(profile);
    profile = new AutoFillProfile;
    autofill_unittest::SetProfileInfo(profile, "Empty", "", "", "", "", "", "",
                                      "", "", "", "", "", "", "");
    profiles->push_back(profile);
  }

  void CreateTestCreditCards(ScopedVector<CreditCard>* credit_cards) {
    CreditCard* credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "First", "Elvis Presley",
                                         "Visa", "1234567890123456", "04",
                                         "2012", "456", "", "");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "Second", "Buddy Holly",
                                         "Mastercard", "0987654321098765", "10",
                                         "2014", "678", "", "");
    credit_cards->push_back(credit_card);
    credit_card = new CreditCard;
    autofill_unittest::SetCreditCardInfo(credit_card, "Empty", "", "", "", "",
                                         "", "", "", "");
    credit_cards->push_back(credit_card);
  }

  DISALLOW_COPY_AND_ASSIGN(TestPersonalDataManager);
};

class TestAutoFillManager : public AutoFillManager {
 public:
  explicit TestAutoFillManager(TabContents* tab_contents)
      : AutoFillManager(tab_contents, &test_personal_data_) {
  }

  virtual bool IsAutoFillEnabled() const { return true; }

 private:
  TestPersonalDataManager test_personal_data_;

  DISALLOW_COPY_AND_ASSIGN(TestAutoFillManager);
};

void CreateTestFormField(const char* label,
                         const char* name,
                         const char* value,
                         const char* type,
                         webkit_glue::FormField* field) {
  *field = webkit_glue::FormField(ASCIIToUTF16(label), ASCIIToUTF16(name),
                                  ASCIIToUTF16(value), ASCIIToUTF16(type));
}

void CreateTestFormData(FormData* form) {
  form->name = ASCIIToUTF16("MyForm");
  form->method = ASCIIToUTF16("POST");
  form->origin = GURL("http://myform.com/form.html");
  form->action = GURL("http://myform.com/submit.html");

  webkit_glue::FormField field;
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Middle Name", "middlename", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Last Name", "lastname", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Address Line 1", "addr1", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Address Line 2", "addr2", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("City", "city", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("State", "state", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Postal Code", "zipcode", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Country", "country", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Phone Number", "phonenumber", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Email", "email", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  form->fields.push_back(field);
  CreateTestFormField("", "ccyear", "", "text", &field);
  form->fields.push_back(field);
}

class AutoFillManagerTest : public RenderViewHostTestHarness {
 public:
  AutoFillManagerTest() {}

  virtual void SetUp() {
    RenderViewHostTestHarness::SetUp();
    autofill_manager_.reset(new TestAutoFillManager(contents()));
  }

  bool GetAutoFillSuggestionsMessage(int *page_id,
                                     std::vector<string16>* values,
                                     std::vector<string16>* labels,
                                     int* default_idx) {
    const uint32 kMsgID = ViewMsg_AutoFillSuggestionsReturned::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    if (!message)
      return false;
    Tuple4<int, std::vector<string16>, std::vector<string16>, int>
        autofill_param;
    ViewMsg_AutoFillSuggestionsReturned::Read(message, &autofill_param);
    if (page_id)
      *page_id = autofill_param.a;
    if (values)
      *values = autofill_param.b;
    if (labels)
      *labels = autofill_param.c;
    if (default_idx)
      *default_idx = autofill_param.d;
    return true;
  }

 protected:
  scoped_ptr<TestAutoFillManager> autofill_manager_;

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
  CreateTestFormField("First Name", "firstname", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("Charles"), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Work"), labels[1]);
  EXPECT_EQ(-1, idx);
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
  CreateTestFormField("First Name", "firstname", "E", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ(ASCIIToUTF16("Elvis"), values[0]);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("Home"), labels[0]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsEmptyValue) {
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
  CreateTestFormField("Card Number", "cardnumber", "", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(2U, values.size());
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[0]);
  EXPECT_EQ(ASCIIToUTF16("************8765"), values[1]);
  ASSERT_EQ(2U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("First"), labels[0]);
  EXPECT_EQ(ASCIIToUTF16("Second"), labels[1]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsMatchCharacter) {
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
  CreateTestFormField("Card Number", "cardnumber", "1", "text", &field);
  EXPECT_TRUE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));

  // Test that we sent the right message to the renderer.
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  EXPECT_TRUE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
  EXPECT_EQ(kPageID, page_id);
  ASSERT_EQ(1U, values.size());
  EXPECT_EQ(ASCIIToUTF16("************3456"), values[0]);
  ASSERT_EQ(1U, labels.size());
  EXPECT_EQ(ASCIIToUTF16("First"), labels[0]);
  EXPECT_EQ(-1, idx);
}

TEST_F(AutoFillManagerTest, GetCreditCardSuggestionsNonCCNumber) {
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
  int page_id = 0;
  std::vector<string16> values;
  std::vector<string16> labels;
  int idx = 0;
  CreateTestFormField("Name on Card", "nameoncard", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));

  CreateTestFormField("Expiration Date", "ccmonth", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));

  CreateTestFormField("", "ccyear", "", "text", &field);
  EXPECT_FALSE(autofill_manager_->GetAutoFillSuggestions(kPageID, field));
  EXPECT_FALSE(GetAutoFillSuggestionsMessage(&page_id, &values, &labels, &idx));
}

}  // namespace
