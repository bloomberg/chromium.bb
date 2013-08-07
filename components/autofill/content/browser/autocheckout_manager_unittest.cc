// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/strings/utf_string_conversions.h"
#include "base/tuple.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/content/browser/autocheckout_manager.h"
#include "components/autofill/core/browser/autofill_common_test.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/test_autofill_driver.h"
#include "components/autofill/core/browser/test_autofill_manager_delegate.h"
#include "components/autofill/core/common/autofill_messages.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_test_sink.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace autofill {

namespace {

typedef Tuple4<std::vector<FormData>,
               std::vector<WebElementDescriptor>,
               std::vector<WebElementDescriptor>,
               WebElementDescriptor> AutofillParam;

enum ProceedElementPresence {
  NO_PROCEED_ELEMENT,
  HAS_PROCEED_ELEMENT,
};

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
    const std::vector<ServerFieldType>& autofill_types) {
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
  for (size_t i = 0; i < autofill_types.size(); ++i) {
    form_structure->field(i)->set_server_type(autofill_types[i]);
    // Set heuristic type to make sure that server_types are used and not
    // heuritic type.
    form_structure->field(i)->set_heuristic_type(CREDIT_CARD_NUMBER);
  }

  return form_structure.Pass();
}

scoped_ptr<FormStructure> CreateTestAddressFormStructure() {
  std::vector<ServerFieldType> autofill_types;
  autofill_types.push_back(NAME_FULL);
  autofill_types.push_back(PHONE_HOME_WHOLE_NUMBER);
  autofill_types.push_back(EMAIL_ADDRESS);
  autofill_types.push_back(ADDRESS_HOME_LINE1);
  autofill_types.push_back(ADDRESS_HOME_CITY);
  autofill_types.push_back(ADDRESS_HOME_STATE);
  autofill_types.push_back(ADDRESS_HOME_COUNTRY);
  autofill_types.push_back(ADDRESS_HOME_ZIP);
  autofill_types.push_back(NO_SERVER_DATA);
  return CreateTestFormStructure(autofill_types);
}

scoped_ptr<FormStructure> CreateTestCreditCardFormStructure() {
  std::vector<ServerFieldType> autofill_types;
  autofill_types.push_back(CREDIT_CARD_NAME);
  autofill_types.push_back(CREDIT_CARD_NUMBER);
  autofill_types.push_back(CREDIT_CARD_EXP_MONTH);
  autofill_types.push_back(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  autofill_types.push_back(CREDIT_CARD_VERIFICATION_CODE);
  autofill_types.push_back(ADDRESS_BILLING_LINE1);
  autofill_types.push_back(ADDRESS_BILLING_CITY);
  autofill_types.push_back(ADDRESS_BILLING_STATE);
  autofill_types.push_back(ADDRESS_BILLING_COUNTRY);
  autofill_types.push_back(ADDRESS_BILLING_ZIP);
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
  FormFieldData select = BuildField("SelectField");
  select.form_control_type = "select-one";
  form.fields.push_back(select);

  scoped_ptr<FormStructure> form_structure(
      new FormStructure(form, std::string()));

  // Fake server response. Set all fields as fields with default value.
  form_structure->field(0)->set_server_type(FIELD_WITH_DEFAULT_VALUE);
  form_structure->field(0)->set_default_value("female");
  form_structure->field(1)->set_server_type(FIELD_WITH_DEFAULT_VALUE);
  form_structure->field(1)->set_default_value("female");
  form_structure->field(2)->set_server_type(FIELD_WITH_DEFAULT_VALUE);
  form_structure->field(2)->set_default_value("Default Value");

  return form_structure.Pass();
}

void PopulateClickElement(WebElementDescriptor* proceed_element,
                          const std::string& descriptor) {
  proceed_element->descriptor = descriptor;
  proceed_element->retrieval_method = WebElementDescriptor::ID;
}

scoped_ptr<AutocheckoutPageMetaData> CreateStartOfFlowMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> start_of_flow(
      new AutocheckoutPageMetaData());
  start_of_flow->current_page_number = 0;
  start_of_flow->total_pages = 3;
  start_of_flow->page_types[0].push_back(AUTOCHECKOUT_STEP_SHIPPING);
  start_of_flow->page_types[1].push_back(AUTOCHECKOUT_STEP_DELIVERY);
  start_of_flow->page_types[2].push_back(AUTOCHECKOUT_STEP_BILLING);
  PopulateClickElement(&start_of_flow->proceed_element_descriptor, "#foo");
  return start_of_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateInFlowMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> in_flow(new AutocheckoutPageMetaData());
  in_flow->current_page_number = 1;
  in_flow->total_pages = 3;
  PopulateClickElement(&in_flow->proceed_element_descriptor, "#foo");
  return in_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateNotInFlowMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> not_in_flow(
      new AutocheckoutPageMetaData());
  PopulateClickElement(&not_in_flow->proceed_element_descriptor, "#foo");
  return not_in_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateEndOfFlowMetaData(
    ProceedElementPresence proceed_element_presence) {
  scoped_ptr<AutocheckoutPageMetaData> end_of_flow(
      new AutocheckoutPageMetaData());
  end_of_flow->current_page_number = 2;
  end_of_flow->total_pages = 3;
  if (proceed_element_presence == HAS_PROCEED_ELEMENT)
    PopulateClickElement(&end_of_flow->proceed_element_descriptor, "#foo");
  return end_of_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateOnePageFlowMetaData(
    ProceedElementPresence proceed_element_presence) {
  scoped_ptr<AutocheckoutPageMetaData> one_page_flow(
      new AutocheckoutPageMetaData());
  one_page_flow->current_page_number = 0;
  one_page_flow->total_pages = 1;
  one_page_flow->page_types[0].push_back(AUTOCHECKOUT_STEP_SHIPPING);
  one_page_flow->page_types[0].push_back(AUTOCHECKOUT_STEP_DELIVERY);
  one_page_flow->page_types[0].push_back(AUTOCHECKOUT_STEP_BILLING);
  if (proceed_element_presence == HAS_PROCEED_ELEMENT)
    PopulateClickElement(&one_page_flow->proceed_element_descriptor, "#foo");
  return one_page_flow.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateMissingProceedMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> missing_proceed(
      new AutocheckoutPageMetaData());
  missing_proceed->current_page_number = 1;
  missing_proceed->total_pages = 3;
  return missing_proceed.Pass();
}

scoped_ptr<AutocheckoutPageMetaData> CreateMultiClickMetaData() {
  scoped_ptr<AutocheckoutPageMetaData> metadata(new AutocheckoutPageMetaData());
  metadata->current_page_number = 1;
  metadata->total_pages = 3;
  PopulateClickElement(&metadata->proceed_element_descriptor, "#foo");
  WebElementDescriptor element;
  PopulateClickElement(&element, "#before_form_fill_1");
  metadata->click_elements_before_form_fill.push_back(element);
  PopulateClickElement(&element, "#before_form_fill_2");
  metadata->click_elements_before_form_fill.push_back(element);
  PopulateClickElement(&element, "#after_form_fill");
  metadata->click_elements_after_form_fill.push_back(element);
  return metadata.Pass();
}

struct TestField {
  const char* const field_type;
  const char* const field_value;
  ServerFieldType autofill_type;
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
  {"postal-code", "49012", ADDRESS_HOME_ZIP},
  {"billing-street-address", "Billing Street", ADDRESS_BILLING_LINE1},
  {"billing-locality", "Billing City", ADDRESS_BILLING_CITY},
  {"billing-region", "BillingState", ADDRESS_BILLING_STATE},
  {"billing-country", "Canada", ADDRESS_BILLING_COUNTRY},
  {"billing-postal-code", "11111", ADDRESS_BILLING_ZIP}
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

class MockAutofillManagerDelegate : public TestAutofillManagerDelegate {
 public:
  MockAutofillManagerDelegate()
      : request_autocomplete_dialog_open_(false),
        autocheckout_bubble_shown_(false),
        should_autoclick_bubble_(true) {}

  virtual ~MockAutofillManagerDelegate() {}

  virtual void HideRequestAutocompleteDialog() OVERRIDE {
    request_autocomplete_dialog_open_ = false;
  }

  MOCK_METHOD0(OnAutocheckoutError, void());
  MOCK_METHOD0(OnAutocheckoutSuccess, void());

  virtual bool ShowAutocheckoutBubble(
      const gfx::RectF& bounds,
      bool is_google_user,
      const base::Callback<void(AutocheckoutBubbleState)>& callback) OVERRIDE {
    autocheckout_bubble_shown_ = true;
    if (should_autoclick_bubble_)
      callback.Run(AUTOCHECKOUT_BUBBLE_ACCEPTED);
    return true;
  }

  virtual void ShowRequestAutocompleteDialog(
      const FormData& form,
      const GURL& source_url,
      DialogType dialog_type,
      const base::Callback<void(const FormStructure*,
                                const std::string&)>& callback) OVERRIDE {
    request_autocomplete_dialog_open_ = true;
    callback.Run(user_supplied_data_.get(), "google_transaction_id");
  }

  virtual void AddAutocheckoutStep(AutocheckoutStepType step_type) OVERRIDE {
    if (autocheckout_steps_.count(step_type) == 0)
      autocheckout_steps_[step_type] = AUTOCHECKOUT_STEP_UNSTARTED;
  }

  virtual void UpdateAutocheckoutStep(
      AutocheckoutStepType step_type,
      AutocheckoutStepStatus step_status) OVERRIDE {
    autocheckout_steps_[step_type] = step_status;
  }

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

  void set_should_autoclick_bubble(bool should_autoclick_bubble) {
    should_autoclick_bubble_ = should_autoclick_bubble;
  }

  bool AutocheckoutStepExistsWithStatus(
      AutocheckoutStepType step_type,
      AutocheckoutStepStatus step_status) const {
    std::map<AutocheckoutStepType, AutocheckoutStepStatus>::const_iterator it =
        autocheckout_steps_.find(step_type);
    return it != autocheckout_steps_.end() && it->second == step_status;
  }

 private:
  // Whether or not ShowRequestAutocompleteDialog method has been called.
  bool request_autocomplete_dialog_open_;

  // Whether or not Autocheckout bubble is displayed to user.
  bool autocheckout_bubble_shown_;

  // Whether or not to accept the Autocheckout bubble when offered.
  bool should_autoclick_bubble_;

  // User specified data that would be returned to AutocheckoutManager when
  // dialog is accepted.
  scoped_ptr<FormStructure> user_supplied_data_;

  // Step status of various Autocheckout steps in this checkout flow.
  std::map<AutocheckoutStepType, AutocheckoutStepStatus> autocheckout_steps_;
};

class TestAutofillManager : public AutofillManager {
 public:
  explicit TestAutofillManager(AutofillDriver* driver,
                               AutofillManagerDelegate* delegate)
      : AutofillManager(driver, delegate, NULL) {
  }
  virtual ~TestAutofillManager() {}

  void SetFormStructure(scoped_ptr<FormStructure> form_structure) {
    form_structures()->clear();
    form_structures()->push_back(form_structure.release());
  }
};

class MockAutofillMetrics : public AutofillMetrics {
 public:
  MockAutofillMetrics() {}
  MOCK_CONST_METHOD1(LogAutocheckoutBubbleMetric, void(BubbleMetric));
  MOCK_CONST_METHOD1(LogAutocheckoutBuyFlowMetric,
                     void(AutocheckoutBuyFlowMetric));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockAutofillMetrics);
};

class TestAutocheckoutManager: public AutocheckoutManager {
 public:
  explicit TestAutocheckoutManager(AutofillManager* autofill_manager)
      : AutocheckoutManager(autofill_manager) {
    set_metric_logger(scoped_ptr<AutofillMetrics>(new MockAutofillMetrics));
  }

  const MockAutofillMetrics& metric_logger() const {
    return static_cast<const MockAutofillMetrics&>(
       AutocheckoutManager::metric_logger());
  }

  virtual void MaybeShowAutocheckoutBubble(
      const GURL& frame_url,
      const gfx::RectF& bounding_box) OVERRIDE {
    AutocheckoutManager::MaybeShowAutocheckoutBubble(frame_url,
                                                     bounding_box);
    // Needed for AutocheckoutManager to post task on IO thread.
    content::RunAllPendingInMessageLoop(BrowserThread::IO);
  }

  using AutocheckoutManager::in_autocheckout_flow;
  using AutocheckoutManager::should_show_bubble;
  using AutocheckoutManager::MaybeShowAutocheckoutDialog;
  using AutocheckoutManager::ReturnAutocheckoutData;
};

}  // namespace

class AutocheckoutManagerTest : public ChromeRenderViewHostTestHarness {
 protected:
  virtual void SetUp() OVERRIDE {
    SetThreadBundleOptions(content::TestBrowserThreadBundle::REAL_IO_THREAD);
    ChromeRenderViewHostTestHarness::SetUp();
    autofill_manager_delegate_.reset(new MockAutofillManagerDelegate());
    autofill_driver_.reset(new TestAutofillDriver(web_contents()));
    autofill_manager_.reset(new TestAutofillManager(
        autofill_driver_.get(),
        autofill_manager_delegate_.get()));
    autocheckout_manager_.reset(
        new TestAutocheckoutManager(autofill_manager_.get()));
  }

  virtual void TearDown() OVERRIDE {
    autocheckout_manager_.reset();
    autofill_manager_delegate_.reset();
    autofill_manager_.reset();
    autofill_driver_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  std::vector<FormData> ReadFilledForms() {
    uint32 kMsgID = AutofillMsg_FillFormsAndClick::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    AutofillParam autofill_param;
    AutofillMsg_FillFormsAndClick::Read(message, &autofill_param);
    return autofill_param.a;
  }

  void CheckFillFormsAndClickIpc(
      ProceedElementPresence proceed_element_presence) {
    EXPECT_EQ(1U, process()->sink().message_count());
    uint32 kMsgID = AutofillMsg_FillFormsAndClick::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    EXPECT_TRUE(message);
    AutofillParam autofill_param;
    AutofillMsg_FillFormsAndClick::Read(message, &autofill_param);
    if (proceed_element_presence == HAS_PROCEED_ELEMENT) {
      EXPECT_EQ(WebElementDescriptor::ID, autofill_param.d.retrieval_method);
      EXPECT_EQ("#foo", autofill_param.d.descriptor);
    } else {
      EXPECT_EQ(WebElementDescriptor::NONE, autofill_param.d.retrieval_method);
    }
    ClearIpcSink();
  }

  void ClearIpcSink() {
    process()->sink().ClearMessages();
  }

  void OpenRequestAutocompleteDialog() {
    EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_FALSE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
    EXPECT_CALL(autocheckout_manager_->metric_logger(),
                LogAutocheckoutBubbleMetric(
                    AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED)).Times(1);
    autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
    // Simulate the user submitting some data via the requestAutocomplete UI.
    autofill_manager_delegate_->SetUserSuppliedData(
        FakeUserSubmittedFormStructure());
    GURL frame_url;
    EXPECT_CALL(autocheckout_manager_->metric_logger(),
                LogAutocheckoutBuyFlowMetric(
                    AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_STARTED)).Times(1);
    autocheckout_manager_->MaybeShowAutocheckoutDialog(
        frame_url,
        AUTOCHECKOUT_BUBBLE_ACCEPTED);
    CheckFillFormsAndClickIpc(HAS_PROCEED_ELEMENT);
    EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_TRUE(autocheckout_manager_->should_preserve_dialog());
    EXPECT_TRUE(autofill_manager_delegate_->request_autocomplete_dialog_open());
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_SHIPPING,
                                           AUTOCHECKOUT_STEP_STARTED));
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_DELIVERY,
                                           AUTOCHECKOUT_STEP_UNSTARTED));
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_BILLING,
                                           AUTOCHECKOUT_STEP_UNSTARTED));
  }

  void HideRequestAutocompleteDialog() {
    EXPECT_TRUE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
    autofill_manager_delegate_->HideRequestAutocompleteDialog();
    EXPECT_FALSE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
  }

  // Test a multi-page Autocheckout flow end to end.
  // |proceed_element_presence_on_last_page| indicates whether the last page
  // of the flow should have a proceed element.
  void TestFullAutocheckoutFlow(
      ProceedElementPresence proceed_element_presence_on_last_page) {
    // Test for progression through last page.
    OpenRequestAutocompleteDialog();
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_SHIPPING,
                                           AUTOCHECKOUT_STEP_STARTED));
    // Complete the first page.
    autocheckout_manager_->OnAutocheckoutPageCompleted(SUCCESS);
    EXPECT_TRUE(autocheckout_manager_->should_preserve_dialog());
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_SHIPPING,
                                           AUTOCHECKOUT_STEP_COMPLETED));

    // Go to the second page.
    EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutSuccess()).Times(1);
    autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
    EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_TRUE(autocheckout_manager_->should_preserve_dialog());
    CheckFillFormsAndClickIpc(HAS_PROCEED_ELEMENT);
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_DELIVERY,
                                           AUTOCHECKOUT_STEP_STARTED));
    autocheckout_manager_->OnAutocheckoutPageCompleted(SUCCESS);
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_DELIVERY,
                                           AUTOCHECKOUT_STEP_COMPLETED));

    // Go to the third page.
    EXPECT_CALL(autocheckout_manager_->metric_logger(),
                LogAutocheckoutBuyFlowMetric(
                    AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_SUCCESS)).Times(1);
    autocheckout_manager_->OnLoadedPageMetaData(
        CreateEndOfFlowMetaData(proceed_element_presence_on_last_page));
    CheckFillFormsAndClickIpc(proceed_element_presence_on_last_page);
    EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_TRUE(autocheckout_manager_->should_preserve_dialog());
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_BILLING,
                                           AUTOCHECKOUT_STEP_STARTED));
    autocheckout_manager_->OnAutocheckoutPageCompleted(SUCCESS);
    EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_TRUE(autocheckout_manager_->should_preserve_dialog());
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_BILLING,
                                           AUTOCHECKOUT_STEP_COMPLETED));

    EXPECT_TRUE(autofill_manager_delegate_->request_autocomplete_dialog_open());

    // Go to the page after the flow.
    autocheckout_manager_->OnLoadedPageMetaData(
          scoped_ptr<AutocheckoutPageMetaData>());
    EXPECT_EQ(proceed_element_presence_on_last_page == HAS_PROCEED_ELEMENT,
              autocheckout_manager_->should_preserve_dialog());
    // Go to another page and we should not preserve the dialog now.
    autocheckout_manager_->OnLoadedPageMetaData(
          scoped_ptr<AutocheckoutPageMetaData>());
    EXPECT_FALSE(autocheckout_manager_->should_preserve_dialog());
  }

  // Test a signle-page Autocheckout flow. |proceed_element_presence| indicates
  // whether the page should have a proceed element.
  void TestSinglePageFlow(ProceedElementPresence proceed_element_presence) {
    // Test one page flow.
    EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_FALSE(
        autofill_manager_delegate_->request_autocomplete_dialog_open());
    EXPECT_CALL(autocheckout_manager_->metric_logger(),
                LogAutocheckoutBubbleMetric(
                    AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED)).Times(1);
    EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutSuccess()).Times(1);
    autocheckout_manager_->OnLoadedPageMetaData(
        CreateOnePageFlowMetaData(proceed_element_presence));
    // Simulate the user submitting some data via the requestAutocomplete UI.
    autofill_manager_delegate_->SetUserSuppliedData(
        FakeUserSubmittedFormStructure());
    GURL frame_url;
    EXPECT_CALL(autocheckout_manager_->metric_logger(),
                LogAutocheckoutBuyFlowMetric(
                    AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_STARTED)).Times(1);
    EXPECT_CALL(autocheckout_manager_->metric_logger(),
                LogAutocheckoutBuyFlowMetric(
                    AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_SUCCESS)).Times(1);
    autocheckout_manager_->MaybeShowAutocheckoutDialog(
        frame_url,
        AUTOCHECKOUT_BUBBLE_ACCEPTED);
    autocheckout_manager_->OnAutocheckoutPageCompleted(SUCCESS);
    CheckFillFormsAndClickIpc(proceed_element_presence);
    EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
    EXPECT_TRUE(autocheckout_manager_->should_preserve_dialog());
    EXPECT_TRUE(autofill_manager_delegate_->request_autocomplete_dialog_open());
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_SHIPPING,
                                           AUTOCHECKOUT_STEP_COMPLETED));
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_DELIVERY,
                                           AUTOCHECKOUT_STEP_COMPLETED));
    EXPECT_TRUE(autofill_manager_delegate_
        ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_BILLING,
                                           AUTOCHECKOUT_STEP_COMPLETED));
    // Go to the page after the flow.
    autocheckout_manager_->OnLoadedPageMetaData(
          scoped_ptr<AutocheckoutPageMetaData>());
    EXPECT_EQ(proceed_element_presence == HAS_PROCEED_ELEMENT,
              autocheckout_manager_->should_preserve_dialog());
    // Go to another page, and we should not preserve the dialog now.
    autocheckout_manager_->OnLoadedPageMetaData(
          scoped_ptr<AutocheckoutPageMetaData>());
    EXPECT_FALSE(autocheckout_manager_->should_preserve_dialog());
  }

 protected:
  scoped_ptr<TestAutofillDriver> autofill_driver_;
  scoped_ptr<TestAutofillManager> autofill_manager_;
  scoped_ptr<TestAutocheckoutManager> autocheckout_manager_;
  scoped_ptr<MockAutofillManagerDelegate> autofill_manager_delegate_;
};

TEST_F(AutocheckoutManagerTest, TestFillForms) {
  OpenRequestAutocompleteDialog();

  // Test if autocheckout manager can fill the first page.
  autofill_manager_->SetFormStructure(CreateTestAddressFormStructure());

  autocheckout_manager_->FillForms();

  std::vector<FormData> filled_forms = ReadFilledForms();
  ASSERT_EQ(1U, filled_forms.size());
  ASSERT_EQ(9U, filled_forms[0].fields.size());
  EXPECT_EQ(ASCIIToUTF16("Test User"), filled_forms[0].fields[0].value);
  EXPECT_EQ(ASCIIToUTF16("650-123-9909"), filled_forms[0].fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("blah@blah.com"), filled_forms[0].fields[2].value);
  EXPECT_EQ(ASCIIToUTF16("Fake Street"), filled_forms[0].fields[3].value);
  EXPECT_EQ(ASCIIToUTF16("Mocked City"), filled_forms[0].fields[4].value);
  EXPECT_EQ(ASCIIToUTF16("California"), filled_forms[0].fields[5].value);
  EXPECT_EQ(ASCIIToUTF16("United States"), filled_forms[0].fields[6].value);
  EXPECT_EQ(ASCIIToUTF16("49012"), filled_forms[0].fields[7].value);
  // Last field should not be filled, because there is no server mapping
  // available for it.
  EXPECT_EQ(ASCIIToUTF16("SomeField"), filled_forms[0].fields[8].value);

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
  EXPECT_EQ(ASCIIToUTF16("Billing Street"), filled_forms[0].fields[5].value);
  EXPECT_EQ(ASCIIToUTF16("Billing City"), filled_forms[0].fields[6].value);
  EXPECT_EQ(ASCIIToUTF16("BillingState"), filled_forms[0].fields[7].value);
  EXPECT_EQ(ASCIIToUTF16("Canada"), filled_forms[0].fields[8].value);
  EXPECT_EQ(ASCIIToUTF16("11111"), filled_forms[0].fields[9].value);

  filled_forms.clear();
  ClearIpcSink();

  // Test form with default values.
  autofill_manager_->SetFormStructure(
      CreateTestFormStructureWithDefaultValues());

  autocheckout_manager_->FillForms();

  filled_forms = ReadFilledForms();
  ASSERT_EQ(1U, filled_forms.size());
  ASSERT_EQ(3U, filled_forms[0].fields.size());
  EXPECT_FALSE(filled_forms[0].fields[0].is_checked);
  EXPECT_EQ(ASCIIToUTF16("male"), filled_forms[0].fields[0].value);
  EXPECT_TRUE(filled_forms[0].fields[1].is_checked);
  EXPECT_EQ(ASCIIToUTF16("female"), filled_forms[0].fields[1].value);
  EXPECT_EQ(ASCIIToUTF16("Default Value"), filled_forms[0].fields[2].value);
}

TEST_F(AutocheckoutManagerTest, OnFormsSeenTest) {
  GURL frame_url;
  gfx::RectF bounding_box;
  EXPECT_CALL(autocheckout_manager_->metric_logger(),
              LogAutocheckoutBubbleMetric(
                  AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
  autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url, bounding_box);

  EXPECT_FALSE(autocheckout_manager_->should_show_bubble());
  // OnFormsSeen resets whether or not the bubble was shown.
  autocheckout_manager_->OnFormsSeen();
  EXPECT_TRUE(autocheckout_manager_->should_show_bubble());
}

TEST_F(AutocheckoutManagerTest, OnClickFailedTestMissingAdvance) {
  OpenRequestAutocompleteDialog();

  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(
          AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_MISSING_ADVANCE_ELEMENT))
      .Times(1);
  autocheckout_manager_->OnAutocheckoutPageCompleted(MISSING_ADVANCE);
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());
  EXPECT_TRUE(autofill_manager_delegate_
      ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_SHIPPING,
                                         AUTOCHECKOUT_STEP_FAILED));
}

TEST_F(AutocheckoutManagerTest, OnClickFailedTestMissingClickBeforeFilling) {
  OpenRequestAutocompleteDialog();

  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(AutofillMetrics::
          AUTOCHECKOUT_BUY_FLOW_MISSING_CLICK_ELEMENT_BEFORE_FORM_FILLING))
      .Times(1);
  autocheckout_manager_->OnAutocheckoutPageCompleted(
      MISSING_CLICK_ELEMENT_BEFORE_FORM_FILLING);
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());
}

TEST_F(AutocheckoutManagerTest, OnClickFailedTestMissingClickAfterFilling) {
  OpenRequestAutocompleteDialog();

  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(AutofillMetrics::
          AUTOCHECKOUT_BUY_FLOW_MISSING_CLICK_ELEMENT_AFTER_FORM_FILLING))
      .Times(1);
  autocheckout_manager_->OnAutocheckoutPageCompleted(
      MISSING_CLICK_ELEMENT_AFTER_FORM_FILLING);
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());
}

TEST_F(AutocheckoutManagerTest, MaybeShowAutocheckoutBubbleTest) {
  GURL frame_url;
  gfx::RectF bounding_box;
  EXPECT_CALL(autocheckout_manager_->metric_logger(),
              LogAutocheckoutBubbleMetric(
                  AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
  // MaybeShowAutocheckoutBubble shows bubble if it has not been shown.
  autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url, bounding_box);
  EXPECT_FALSE(autocheckout_manager_->should_show_bubble());
  EXPECT_TRUE(autofill_manager_delegate_->autocheckout_bubble_shown());

  // Reset |autofill_manager_delegate_|.
  HideRequestAutocompleteDialog();
  autofill_manager_delegate_->set_autocheckout_bubble_shown(false);

  // MaybeShowAutocheckoutBubble does nothing if the bubble was already shown
  // for the current page.
  autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url, bounding_box);
  EXPECT_FALSE(autocheckout_manager_->should_show_bubble());
  EXPECT_FALSE(autofill_manager_delegate_->autocheckout_bubble_shown());
  EXPECT_FALSE(autofill_manager_delegate_->request_autocomplete_dialog_open());
}

TEST_F(AutocheckoutManagerTest, OnLoadedPageMetaDataMissingMetaData) {
  // Gettting no meta data after any autocheckout page is an error.
  OpenRequestAutocompleteDialog();
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(
          AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_MISSING_FIELDMAPPING))
      .Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(
      scoped_ptr<AutocheckoutPageMetaData>());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());
}

TEST_F(AutocheckoutManagerTest, OnLoadedPageMetaDataRepeatedStartPage) {
  // Getting start page twice in a row is an error.
  OpenRequestAutocompleteDialog();
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(
          AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_CANNOT_PROCEED)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());

  EXPECT_TRUE(autofill_manager_delegate_
      ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_SHIPPING,
                                         AUTOCHECKOUT_STEP_FAILED));
}

TEST_F(AutocheckoutManagerTest, OnLoadedPageMetaDataRepeatedPage) {
  // Repeating a page is an error.
  OpenRequestAutocompleteDialog();
  // Go to second page.
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
  CheckFillFormsAndClickIpc(HAS_PROCEED_ELEMENT);
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(
          AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_CANNOT_PROCEED)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());
  EXPECT_TRUE(autofill_manager_delegate_
      ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_DELIVERY,
                                         AUTOCHECKOUT_STEP_FAILED));
}

TEST_F(AutocheckoutManagerTest, OnLoadedPageMetaDataNotInFlow) {
  // Repeating a page is an error.
  OpenRequestAutocompleteDialog();
  // Go to second page.
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
  CheckFillFormsAndClickIpc(HAS_PROCEED_ELEMENT);
  EXPECT_CALL(*autofill_manager_delegate_, OnAutocheckoutError()).Times(1);
  EXPECT_CALL(
      autocheckout_manager_->metric_logger(),
      LogAutocheckoutBuyFlowMetric(
          AutofillMetrics::AUTOCHECKOUT_BUY_FLOW_MISSING_FIELDMAPPING))
      .Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateNotInFlowMetaData());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_EQ(0U, process()->sink().message_count());
  EXPECT_TRUE(
      autofill_manager_delegate_->request_autocomplete_dialog_open());
  EXPECT_TRUE(autofill_manager_delegate_
      ->AutocheckoutStepExistsWithStatus(AUTOCHECKOUT_STEP_DELIVERY,
                                         AUTOCHECKOUT_STEP_FAILED));
}

TEST_F(AutocheckoutManagerTest,
       OnLoadedPageMetaDataShouldNotFillFormsIfNotInFlow) {
  // If not in flow, OnLoadedPageMetaData does not fill forms.
  EXPECT_CALL(autocheckout_manager_->metric_logger(),
              LogAutocheckoutBubbleMetric(
                  AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());
  // Go to second page.
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_EQ(0U, process()->sink().message_count());
}

TEST_F(AutocheckoutManagerTest, FullAutocheckoutFlow) {
  TestFullAutocheckoutFlow(HAS_PROCEED_ELEMENT);
}

TEST_F(AutocheckoutManagerTest, FullAutocheckoutFlowNoClickOnLastPage) {
  TestFullAutocheckoutFlow(NO_PROCEED_ELEMENT);
}

TEST_F(AutocheckoutManagerTest, CancelledAutocheckoutFlow) {
  // Test for progression through last page.
  OpenRequestAutocompleteDialog();
  // Go to second page.
  autocheckout_manager_->OnLoadedPageMetaData(CreateInFlowMetaData());
  EXPECT_TRUE(autocheckout_manager_->in_autocheckout_flow());
  CheckFillFormsAndClickIpc(HAS_PROCEED_ELEMENT);

  // Cancel the flow.
  autocheckout_manager_->ReturnAutocheckoutData(NULL, std::string());
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());

  // Go to third page.
  EXPECT_CALL(autocheckout_manager_->metric_logger(),
              LogAutocheckoutBuyFlowMetric(testing::_)).Times(0);
  autocheckout_manager_->OnLoadedPageMetaData(
      CreateEndOfFlowMetaData(HAS_PROCEED_ELEMENT));
  EXPECT_EQ(0U, process()->sink().message_count());
}

TEST_F(AutocheckoutManagerTest, SinglePageFlow) {
  TestSinglePageFlow(HAS_PROCEED_ELEMENT);
}

TEST_F(AutocheckoutManagerTest, SinglePageFlowNoClickElement) {
  TestSinglePageFlow(NO_PROCEED_ELEMENT);
}

TEST_F(AutocheckoutManagerTest, CancelAutocheckoutBubble) {
  GURL frame_url;
  gfx::RectF bounding_box;
  autofill_manager_delegate_->set_should_autoclick_bubble(false);
  EXPECT_FALSE(autocheckout_manager_->in_autocheckout_flow());
  EXPECT_FALSE(autofill_manager_delegate_->request_autocomplete_dialog_open());
  EXPECT_FALSE(autocheckout_manager_->is_autocheckout_bubble_showing());
  EXPECT_CALL(autocheckout_manager_->metric_logger(),
              LogAutocheckoutBubbleMetric(
                  AutofillMetrics::BUBBLE_COULD_BE_DISPLAYED)).Times(1);
  autocheckout_manager_->OnLoadedPageMetaData(CreateStartOfFlowMetaData());

  autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url, bounding_box);
  EXPECT_TRUE(autocheckout_manager_->is_autocheckout_bubble_showing());
  autocheckout_manager_->MaybeShowAutocheckoutDialog(
      frame_url,
      AUTOCHECKOUT_BUBBLE_IGNORED);
  EXPECT_FALSE(autocheckout_manager_->is_autocheckout_bubble_showing());
  EXPECT_TRUE(autocheckout_manager_->should_show_bubble());

  autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url, bounding_box);
  EXPECT_TRUE(autocheckout_manager_->is_autocheckout_bubble_showing());
  autocheckout_manager_->MaybeShowAutocheckoutDialog(
      frame_url,
      AUTOCHECKOUT_BUBBLE_CANCELED);
  EXPECT_FALSE(autocheckout_manager_->is_autocheckout_bubble_showing());
  EXPECT_FALSE(autocheckout_manager_->should_show_bubble());

  autocheckout_manager_->MaybeShowAutocheckoutBubble(frame_url, bounding_box);
  EXPECT_FALSE(autocheckout_manager_->is_autocheckout_bubble_showing());
}

}  // namespace autofill
