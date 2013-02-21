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

namespace autofill {

namespace {

typedef Tuple2<std::vector<FormData>, WebElementDescriptor> AutofillParam;

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

scoped_ptr<WebElementDescriptor> CreateProceedElement() {
  scoped_ptr<WebElementDescriptor> proceed_element(new WebElementDescriptor());
  proceed_element->descriptor = "#foo";
  proceed_element->retrieval_method = WebElementDescriptor::ID;
  return proceed_element.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateStartOfFlowMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> start_of_flow(
      new AutocheckoutPageMetaData());
  start_of_flow->current_page_number = 0;
  start_of_flow->total_pages = 3;
  start_of_flow->proceed_element_descriptor = CreateProceedElement().Pass();
  return start_of_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateInFlowMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> in_flow(new AutocheckoutPageMetaData());
  in_flow->current_page_number = 1;
  in_flow->total_pages = 3;
  in_flow->proceed_element_descriptor = CreateProceedElement().Pass();
  return in_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateEndOfFlowMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> end_of_flow(
      new AutocheckoutPageMetaData());
  end_of_flow->current_page_number = 2;
  end_of_flow->total_pages = 3;
  return end_of_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateMissingProceedMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> missing_proceed(
      new AutocheckoutPageMetaData());
  missing_proceed->current_page_number = 1;
  missing_proceed->total_pages = 3;
  return missing_proceed.Pass();
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

class TestAutofillManagerDelegate : public AutofillManagerDelegate {
 public:
  TestAutofillManagerDelegate()
      : request_autocomplete_dialog_open_(false),
        autocheckout_bubble_shown_(false) {
  }

  virtual ~TestAutofillManagerDelegate() {
  }

  virtual InfoBarService* GetInfoBarService() OVERRIDE {
    return NULL;
  }

  virtual PersonalDataManager* GetPersonalDataManager() OVERRIDE {
    return NULL;
  }

  virtual PrefService* GetPrefs() OVERRIDE {
    return NULL;
  }

  virtual ProfileSyncServiceBase* GetProfileSyncService() OVERRIDE {
    return NULL;
  }

  virtual void HideRequestAutocompleteDialog() OVERRIDE {
    request_autocomplete_dialog_open_ = false;
  }

  virtual bool IsSavingPasswordsEnabled() const OVERRIDE {
    return false;
  }

  MOCK_METHOD0(OnAutocheckoutError, void());

  virtual void ShowAutofillSettings() OVERRIDE {
  }

  virtual void ShowPasswordGenerationBubble(
      const gfx::Rect& bounds,
      const content::PasswordForm& form,
      PasswordGenerator* generator) OVERRIDE {
  }

  virtual void ShowAutocheckoutBubble(
      const gfx::RectF& bounds,
      const gfx::NativeView& native_view,
      const base::Closure& callback) OVERRIDE {
    autocheckout_bubble_shown_ = true;
    callback.Run();
  }

  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      const content::SSLStatus& ssl_status,
      const AutofillMetrics& metric_logger,
      DialogType dialog_type,
      const base::Callback<void(const FormStructure*)>& callback) OVERRIDE {
    request_autocomplete_dialog_open_ = true;
    callback.Run(user_supplied_data_.get());
  }

  virtual void RequestAutocompleteDialogClosed() OVERRIDE {
  }

  MOCK_METHOD1(UpdateProgressBar, void(double value));

  void SetUserSuppliedData(scoped_ptr<FormStructure> user_supplied_data) {
    user_supplied_data_.reset(user_supplied_data.release());
  }

  bool autocheckout_bubble_shown() const {
    return autocheckout_bubble_shown_;
  }

  void set_autocheckout_bubble_shown(bool autocheckout_bubble_shown) {
    autocheckout_bubble_shown_ = autocheckout_bubble_shown;
  }

  bool request_autocomplete_dialog_open() const {
    return request_autocomplete_dialog_open_;
  }

 private:
  bool request_autocomplete_dialog_open_;
  bool autocheckout_bubble_shown_;
  scoped_ptr<FormStructure> user_supplied_data_;
};

class TestAutofillManager : public AutofillManager {
 public:
  explicit TestAutofillManager(content::WebContents* contents,
                               AutofillManagerDelegate* delegate)
      : AutofillManager(contents, delegate, NULL) {
  }

  void SetFormStructure(scoped_ptr<FormStructure> form_structure) {
    form_structures()->clear();
    form_structures()->push_back(form_structure.release());
  }

 private:
  virtual ~TestAutofillManager() {}
};


class TestAutocheckoutManager: public AutocheckoutManager {
  public:
   explicit TestAutocheckoutManager(AutofillManager* autofill_manager)
       : AutocheckoutManager(autofill_manager) {}

   using AutocheckoutManager::in_autocheckout_flow;
   using AutocheckoutManager::autocheckout_bubble_shown;
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

  void CheckIpcMessageSent() {
    EXPECT_EQ(1U, process()->sink().message_count());
    uint32 kMsgID = AutofillMsg_FillFormsAndClick::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    EXPECT_TRUE(message);
    ClearIpcSink();
  }

  void ClearIpcSink() {
    process()->sink().ClearMessages();
  }

  void OpenRequestAutocompleteDialog() {
    EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_FALSE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
    autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
    // Simulate the user submitting some data via the requestAutocomplete UI.
    autofill_manager_delegate_->SetUserSuppliedData(
        FakeUserSubmittedFormStructure());
    GURL frame_url;
    content::SSLStatus ssl_status;
    EXPECT_CALL(*autofill_manager_delegate_,
                UpdateProgressBar(testing::DoubleEq(1.0/3.0))).Times(1);
    autocheckout_manager_->ShowAutocheckoutDialog(frame_url, ssl_status);
    CheckIpcMessageSent();
    EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_TRUE(autofill_manager_delegate_->request_autocomplete_dialog_open());
  }

  void HideRequestAutocompleteDialog() {
    EXPECT_TRUE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
    autofill_manager_delegate_->HideRequestAutocompleteDialog();
    EXPECT_FALSE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
  }

 protected:
  content::TestBrowserThread ui_thread_;
  scoped_refptr<TestAutofillManager> autofill_manager_;
  scoped_ptr<TestAutocheckoutManager> autocheckout_manager_;
  scoped_ptr<TestAutofillManagerDelegate> autofill_manager_delegate_;

 private:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    autofill_manager_delegate_.reset(new TestAutofillManagerDelegate());
    autofill_manager_ = new TestAutofillManager(
        web_contents(),
        autofill_manager_delegate_.get());
    autocheckout_manager_.reset(new TestAutocheckoutManager(autofill_manager_));
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
  OpenRequestAutocompleteDialog();

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

TEST_F(AutocheckoutManagerTest, OnFormsSeenTest) {
  GURL frame_url;
  content::SSLStatus ssl_status;
  gfx::NativeView native_view;
  gfx::RectF bounding_box;
  EXPECT_TRUE(autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url,
                                                                 ssl_status,
                                                                 native_view,
                                                                 bounding_box));
  EXPECT_TRUE(autocheckout_manager_->autocheckout_bubble_shown());
  // OnFormsSeen resets whether or not the bubble was shown.
  autocheckout_manager_->OnFormsSeen();
  EXPECT_FALSE(autocheckout_manager_->autocheckout_bubble_shown());
}

TEST_F(AutocheckoutManagerTest, MaybeShowAutocheckoutBubbleTest) {
  GURL frame_url;
  content::SSLStatus ssl_status;
  gfx::NativeView native_view;
  gfx::RectF bounding_box;
  // MaybeShowAutocheckoutBubble shows bubble if it has not been shown.
  EXPECT_TRUE(autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url,
                                                                 ssl_status,
                                                                 native_view,
                                                                 bounding_box));
  EXPECT_TRUE(autocheckout_manager_->autocheckout_bubble_shown());
  EXPECT_TRUE(autofill_manager_delegate_->autocheckout_bubble_shown());

  // Reset |autofill_manager_delegate_|.
  HideRequestAutocompleteDialog();
  autofill_manager_delegate_->set_autocheckout_bubble_shown(false);

  // MaybeShowAutocheckoutBubble does nothing if the bubble was already shown
  // for the current page.
  EXPECT_FALSE(autocheckout_manager_->MaybeShowAutocheckoutBubble(
      frame_url,
      ssl_status,
      native_view,
      bounding_box));
  EXPECT_TRUE(autocheckout_manager_->autocheckout_bubble_shown());
  EXPECT_FALSE(autofill_manager_delegate_->autocheckout_bubble_shown());
  EXPECT_FALSE(autofill_manager_delegate_->request_autocomplete_dialog_open());
}

TEST_F(AutocheckoutManagerTest, OnLoadedPageMetaDataTest) {
  // Gettting no meta data after any autocheckout page is an error.
  OpenRequestAutocompleteDialog();
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(
      scoped_ptr<AutocheckoutPageMetaData>());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  HideRequestAutocompleteDialog();

  // Getting start page twice in a row is an error.
  OpenRequestAutocompleteDialog();
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  HideRequestAutocompleteDialog();

  // A missing proceed element when not at the end of a flow is an error.
  OpenRequestAutocompleteDialog();
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateMissingProceedMetaData());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  HideRequestAutocompleteDialog();

  // Repeating a page is an error.
  OpenRequestAutocompleteDialog();
  // Go to second page.
  EXPECT_CALL(*autofill_manager_delegate_,
              UpdateProgressBar(testing::DoubleEq(2.0/3.0))).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
  CheckIpcMessageSent();
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  HideRequestAutocompleteDialog();

  // If not in flow, OnLoadedPageMetaData does not fill forms.
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
  // Go to second page.
  EXPECT_CALL(*autofill_manager_delegate_,
              UpdateProgressBar(testing::_)).Times(0);
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_EQ(0U, process()->sink().message_count());

  // Test for progression through last page.
  OpenRequestAutocompleteDialog();
  // Go to second page.
  EXPECT_CALL(*autofill_manager_delegate_,
              UpdateProgressBar(testing::DoubleEq(2.0/3.0))).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
  CheckIpcMessageSent();
  // Go to third page.
  EXPECT_CALL(*autofill_manager_delegate_, UpdateProgressBar(1)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateEndOfFlowMetaData());
  CheckIpcMessageSent();
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_FALSE(autofill_manager_delegate_->request_autocomplete_dialog_open());
}

}  // namespace autofill
