// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/ref_counted.h"
#include "base/tuple.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autocheckout_manager.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_manager_delegate.h"
#include "chrome/browser/autofill/form_structure.h"
#include "chrome/common/autofill_messages.h"
#include "chrome/common/form_data.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"


using content::BrowserThread;

namespace {

typedef Tuple2<std::vector<FormData>,
               autofill::WebElementDescriptor> AutofillParam;

FormFieldData BuildFieldWithValue(
    const std::string& autocomplete_attribute,
    const std::string& value) {
  FormFieldData field;
  field.name = ASCIIToUTF16(autocomplete_attribute);
  field.value = ASCIIToUTF16(value);
  field.autocomplete_attribute = autocomplete_attribute;
  field.form_control_type = "text";
  return field;
}

FormFieldData BuildField(const std::string& autocomplete_attribute) {
  return BuildFieldWithValue(autocomplete_attribute, autocomplete_attribute);
}

scoped_ptr<FormStructure> CreateTestFormStructure(
    const std::vector<AutofillFieldType>& autofill_types) {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.user_submitted = true;

  // Add some fields, autocomplete_attribute is not important and we
  // fake that server sends authoritative field mappings.
  for (size_t i = 0; i < autofill_types.size(); ++i)
    form.fields.push_back(BuildField("SomeField"));

  scoped_ptr<FormStructure> form_structure(
      new FormStructure(form, std::string()));

  // Set mocked Autofill server field types.
  for (size_t i = 0; i < autofill_types.size(); ++i)
    form_structure->field(i)->set_server_type(autofill_types[i]);

  return form_structure.Pass();
}

scoped_ptr<FormStructure> CreateTestAddressFormStructure() {
  std::vector<AutofillFieldType> autofill_types;
  autofill_types.push_back(NAME_FULL);
  autofill_types.push_back(PHONE_HOME_WHOLE_NUMBER);
  autofill_types.push_back(EMAIL_ADDRESS);

  return CreateTestFormStructure(autofill_types);
}

scoped_ptr<FormStructure> CreateTestCreditCardFormStructure() {
  std::vector<AutofillFieldType> autofill_types;
  autofill_types.push_back(CREDIT_CARD_NAME);
  autofill_types.push_back(CREDIT_CARD_NUMBER);
  autofill_types.push_back(CREDIT_CARD_EXP_MONTH);
  autofill_types.push_back(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  autofill_types.push_back(CREDIT_CARD_VERIFICATION_CODE);
  autofill_types.push_back(ADDRESS_HOME_LINE1);
  autofill_types.push_back(ADDRESS_HOME_CITY);
  autofill_types.push_back(ADDRESS_HOME_STATE);
  autofill_types.push_back(ADDRESS_HOME_COUNTRY);
  autofill_types.push_back(ADDRESS_HOME_ZIP);
  return CreateTestFormStructure(autofill_types);
}

scoped_ptr<FormStructure> CreateTestFormStructureWithDefaultValues() {
  FormData form;
  form.name = ASCIIToUTF16("MyForm");
  form.method = ASCIIToUTF16("POST");
  form.origin = GURL("https://myform.com/form.html");
  form.action = GURL("https://myform.com/submit.html");
  form.user_submitted = true;

  // Add two radio button fields.
  FormFieldData male = BuildFieldWithValue("sex", "male");
  male.is_checkable = true;
  form.fields.push_back(male);
  FormFieldData female = BuildFieldWithValue("sex", "female");
  female.is_checkable = true;
  form.fields.push_back(female);

  scoped_ptr<FormStructure> form_structure(
      new FormStructure(form, std::string()));

  // Fake server response. Set all fields as fields with default value.
  form_structure->field(0)->set_server_type(FIELD_WITH_DEFAULT_VALUE);
  form_structure->field(0)->set_default_value("female");
  form_structure->field(1)->set_server_type(FIELD_WITH_DEFAULT_VALUE);
  form_structure->field(1)->set_default_value("female");

  return form_structure.Pass();
}

struct TestField {
  const char* const field_type;
  const char* const field_value;
  AutofillFieldType autofill_type;
};

const TestField kTestFields[] = {
  {"name", "Test User", NAME_FULL},
  {"tel", "650-123-9909", PHONE_HOME_WHOLE_NUMBER},
  {"email", "blah@blah.com", EMAIL_ADDRESS},
  {"cc-name", "Test User", CREDIT_CARD_NAME},
  {"cc-number", "4444444444444448", CREDIT_CARD_NUMBER},
  {"cc-exp-month", "10", CREDIT_CARD_EXP_MONTH},
  {"cc-exp-year", "2020", CREDIT_CARD_EXP_4_DIGIT_YEAR},
  {"cc-csc", "123", CREDIT_CARD_VERIFICATION_CODE},
  {"street-address", "Fake Street", ADDRESS_HOME_LINE1},
  {"locality", "Mocked City", ADDRESS_HOME_CITY},
  {"region", "California", ADDRESS_HOME_STATE},
  {"country", "USA", ADDRESS_HOME_COUNTRY},
  {"postal-code", "49012", ADDRESS_HOME_ZIP}
};

// Build Autocheckout specific form data to be consumed by
// AutofillDialogController to show the Autocheckout specific UI.
scoped_ptr<FormStructure> FakeUserSubmittedFormStructure() {
  FormData formdata;
  for (size_t i = 0; i < arraysize(kTestFields); i++) {
    formdata.fields.push_back(
      BuildFieldWithValue(kTestFields[i].field_type,
                          kTestFields[i].field_value));
  }
  scoped_ptr<FormStructure> form_structure;
  form_structure.reset(new FormStructure(formdata, std::string()));
  for (size_t i = 0; i < arraysize(kTestFields); ++i)
    form_structure->field(i)->set_server_type(kTestFields[i].autofill_type);

  return form_structure.Pass();
}

class TestAutofillManagerDelegate : public autofill::AutofillManagerDelegate {
 public:
  explicit TestAutofillManagerDelegate(
      content::BrowserContext* browser_context)
      : browser_context_(browser_context) {
  }

  virtual ~TestAutofillManagerDelegate() {
  }

  virtual content::BrowserContext* GetBrowserContext() const OVERRIDE {
    return browser_context_;
  }

  virtual content::BrowserContext* GetOriginalBrowserContext() const OVERRIDE {
    return browser_context_;
  }

  virtual Profile* GetOriginalProfile() const OVERRIDE {
    return NULL;
  }

  virtual InfoBarService* GetInfoBarService() OVERRIDE {
    return NULL;
  }

  virtual PrefServiceBase* GetPrefs() OVERRIDE {
    return NULL;
  }

  virtual ProfileSyncServiceBase* GetProfileSyncService() OVERRIDE {
    return NULL;
  }

  virtual bool IsSavingPasswordsEnabled() const OVERRIDE {
    return false;
  }

  virtual void ShowAutofillSettings() OVERRIDE {
  }

  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      autofill::PasswordGenerator* generator) OVERRIDE {
  }

  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounds,
      const gfx::NativeView& native_view,
      const base::Closure& callback) OVERRIDE {
    callback.Run();
  }

  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const base::Callback<void(const FormStructure*)>& callback) OVERRIDE {
    callback.Run(user_supplied_data_.get());
  }

  virtual void RequestAutocompleteDialogClosed() OVERRIDE {
  }

  virtual void UpdateProgressBar(double value) OVERRIDE {
  }

  void SetUserSuppliedData(scoped_ptr<FormStructure> user_supplied_data) {
    user_supplied_data_.reset(user_supplied_data.release());
  }

 private:
  content::BrowserContext* browser_context_;
  scoped_ptr<FormStructure> user_supplied_data_;
};

class TestAutofillManager : public AutofillManager {
 public:
  explicit TestAutofillManager(content::WebContents* contents,
                               autofill::AutofillManagerDelegate* delegate)
      : AutofillManager(contents, delegate, NULL) {
  }

  void SetFormStructure(scoped_ptr<FormStructure> form_structure) {
    form_structures()->clear();
    form_structures()->push_back(form_structure.release());
  }

 private:
  virtual ~TestAutofillManager() {}
};

}  // namespace

class AutocheckoutManagerTest : public ChromeRenderViewHostTestHarness {
 public:
  AutocheckoutManagerTest()
      : ChromeRenderViewHostTestHarness(),
        ui_thread_(BrowserThread::UI, &message_loop_) {
  }

  std::vector<FormData> ReadFilledForms() {
    uint32 kMsgID = AutofillMsg_FillFormsAndClick::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    AutofillParam autofill_param;
    AutofillMsg_FillFormsAndClick::Read(message, &autofill_param);
    return autofill_param.a;
  }

  void ClearIpcSink() {
    process()->sink().ClearMessages();
  }

 protected:
  content::TestBrowserThread ui_thread_;
  scoped_refptr<TestAutofillManager> autofill_manager_;
  scoped_ptr<AutocheckoutManager> autocheckout_manager_;
  scoped_ptr<TestAutofillManagerDelegate> autofill_manager_delegate_;

 private:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    autofill_manager_delegate_.reset(
        new TestAutofillManagerDelegate(browser_context()));
    autofill_manager_ = new TestAutofillManager(
        web_contents(),
        autofill_manager_delegate_.get());
    autocheckout_manager_.reset(new AutocheckoutManager(autofill_manager_));
  }

  virtual void TearDown() OVERRIDE {
    autocheckout_manager_.reset();
    autofill_manager_delegate_.reset();
    autofill_manager_ = NULL;
    ChromeRenderViewHostTestHarness::TearDown();
  }

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutManagerTest);
};

TEST_F(AutocheckoutManagerTest, TestFillForms) {
  // Simulate the user submitting some data via the requestAutocomplete UI.
  autofill_manager_delegate_->SetUserSuppliedData(
      FakeUserSubmittedFormStructure());

  // Set up page meta data.
  scoped_ptr<autofill::AutocheckoutPageMetaData> page_meta_data(
      new autofill::AutocheckoutPageMetaData());
  page_meta_data->proceed_element_descriptor.reset(
      new autofill::WebElementDescriptor());
  autocheckout_manager_->OnLoadedPageMetaData(page_meta_data.Pass());

  GURL frame_url;
  content::SSLStatus ssl_status;
  autocheckout_manager_->ShowAutocheckoutDialog(frame_url, ssl_status);

  ClearIpcSink();

  // Test if autocheckout manager can fill the first page.
  autofill_manager_->SetFormStructure(CreateTestAddressFormStructure());

  autocheckout_manager_->FillForms();

  std::vector<FormData> filled_forms = ReadFilledForms();
  ASSERT_EQ(1U, filled_forms.size());
  ASSERT_EQ(3U, filled_forms[0].fields.size());
  EXPECT_EQ(ASCIIToUTF16("Test User"), filled_forms[0].fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650-123-9909"), filled_forms[0].fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("blah@blah.com"), filled_forms[0].fields[2].value);

  filled_forms.clear();
  ClearIpcSink();

  // Test if autocheckout manager can fill form on second page.
  autofill_manager_->SetFormStructure(CreateTestCreditCardFormStructure());

  autocheckout_manager_->FillForms();

  filled_forms = ReadFilledForms();
  ASSERT_EQ(1U, filled_forms.size());
  ASSERT_EQ(10U, filled_forms[0].fields.size());
  EXPECT_EQ(ASCIIToUTF16("Test User"), filled_forms[0].fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("4444444444444448"), filled_forms[0].fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("10"), filled_forms[0].fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("2020"), filled_forms[0].fields[3].value);
  EXPECT_EQ(ASCIIToUTF16("123"), filled_forms[0].fields[4].value);
  EXPECT_EQ(ASCIIToUTF16("Fake Street"), filled_forms[0].fields[5].value);
  EXPECT_EQ(ASCIIToUTF16("Mocked City"), filled_forms[0].fields[6].value);
  EXPECT_EQ(ASCIIToUTF16("California"), filled_forms[0].fields[7].value);
  EXPECT_EQ(ASCIIToUTF16("United States"), filled_forms[0].fields[8].value);
  EXPECT_EQ(ASCIIToUTF16("49012"), filled_forms[0].fields[9].value);

  filled_forms.clear();
  ClearIpcSink();

  // Test form with default values.
  autofill_manager_->SetFormStructure(
      CreateTestFormStructureWithDefaultValues());

  autocheckout_manager_->FillForms();

  filled_forms = ReadFilledForms();
  ASSERT_EQ(1U, filled_forms.size());
  ASSERT_EQ(2U, filled_forms[0].fields.size());
  EXPECT_FALSE(filled_forms[0].fields[0].is_checked);
  EXPECT_EQ(ASCIIToUTF16("male"), filled_forms[0].fields[0].value);
  EXPECT_TRUE(filled_forms[0].fields[1].is_checked);
  EXPECT_EQ(ASCIIToUTF16("female"), filled_forms[0].fields[1].value);
}
