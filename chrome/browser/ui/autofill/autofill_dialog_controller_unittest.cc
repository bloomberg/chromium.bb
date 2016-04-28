// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include <map>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/signin/account_tracker_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/mock_address_validator.h"
#include "chrome/browser/web_data_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/chromium/chrome_address_validator.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_problem.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

namespace {

using ::i18n::addressinput::FieldProblemMap;
using testing::AtLeast;
using testing::DoAll;
using testing::Return;
using testing::SetArgPointee;
using testing::_;

const char kSourceUrl[] = "http://localbike.shop";
const char kFakeEmail[] = "user@chromium.org";
const char* kFieldsFromPage[] =
    { "email",
      "cc-name",
      "cc-number",
      "cc-exp-month",
      "cc-exp-year",
      "cc-csc",
      "billing name",
      "billing address-line1",
      "billing address-level2",
      "billing address-level1",
      "billing postal-code",
      "billing country",
      "billing tel",
      "shipping name",
      "shipping address-line1",
      "shipping address-level2",
      "shipping address-level1",
      "shipping postal-code",
      "shipping country",
      "shipping tel",
    };
const char kTestCCNumberAmex[] = "376200000000002";
const char kTestCCNumberVisa[] = "4111111111111111";
const char kTestCCNumberMaster[] = "5555555555554444";
const char kTestCCNumberDiscover[] = "6011111111111117";
const char kTestCCNumberIncomplete[] = "4111111111";
// Credit card number fails Luhn check.
const char kTestCCNumberInvalid[] = "4111111111111112";

bool HasAnyError(const ValidityMessages& messages, ServerFieldType field) {
  return !messages.GetMessageOrDefault(field).text.empty();
}

bool HasUnsureError(const ValidityMessages& messages, ServerFieldType field) {
  const ValidityMessage& message = messages.GetMessageOrDefault(field);
  return !message.text.empty() && !message.sure;
}

class TestAutofillDialogView : public AutofillDialogView {
 public:
  TestAutofillDialogView()
      : updates_started_(0), save_details_locally_checked_(true) {}
  ~TestAutofillDialogView() override {}

  void Show() override {}
  void Hide() override {}

  void UpdatesStarted() override {
    updates_started_++;
  }

  void UpdatesFinished() override {
    updates_started_--;
    EXPECT_GE(updates_started_, 0);
  }

  void UpdateNotificationArea() override {
    EXPECT_GE(updates_started_, 1);
  }

  void UpdateButtonStrip() override {
    EXPECT_GE(updates_started_, 1);
  }

  void UpdateDetailArea() override {
    EXPECT_GE(updates_started_, 1);
  }

  void UpdateSection(DialogSection section) override {
    section_updates_[section]++;
    EXPECT_GE(updates_started_, 1);
  }

  void UpdateErrorBubble() override {
    EXPECT_GE(updates_started_, 1);
  }

  void FillSection(DialogSection section,
                   ServerFieldType originating_type) override {}
  void GetUserInput(DialogSection section, FieldValueMap* output) override {
    *output = outputs_[section];
  }

  base::string16 GetCvc() override { return base::string16(); }

  bool SaveDetailsLocally() override { return save_details_locally_checked_; }

  MOCK_METHOD0(ModelChanged, void());
  MOCK_METHOD0(UpdateForErrors, void());

  void ValidateSection(DialogSection) override {}

  void SetUserInput(DialogSection section, const FieldValueMap& map) {
    outputs_[section] = map;
  }

  void CheckSaveDetailsLocallyCheckbox(bool checked) {
    save_details_locally_checked_ = checked;
  }

  void ClearSectionUpdates() {
    section_updates_.clear();
  }

  std::map<DialogSection, size_t> section_updates() const {
    return section_updates_;
  }

 private:
  std::map<DialogSection, FieldValueMap> outputs_;
  std::map<DialogSection, size_t> section_updates_;

  int updates_started_;
  bool save_details_locally_checked_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogView);
};

class TestAutofillDialogController
    : public AutofillDialogControllerImpl,
      public base::SupportsWeakPtr<TestAutofillDialogController> {
 public:
  TestAutofillDialogController(
      content::WebContents* contents,
      const FormData& form_structure,
      const GURL& source_url,
      const AutofillClient::ResultCallback& callback)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     callback),
        submit_button_delay_count_(0) {}

  ~TestAutofillDialogController() override {}

  AutofillDialogView* CreateView() override {
    return new testing::NiceMock<TestAutofillDialogView>();
  }

  void Init(content::BrowserContext* browser_context) {
    Profile* profile = Profile::FromBrowserContext(browser_context);
    test_manager_.Init(WebDataServiceFactory::GetAutofillWebDataForProfile(
                           profile,
                           ServiceAccessType::EXPLICIT_ACCESS),
                       user_prefs::UserPrefs::Get(browser_context),
                       AccountTrackerServiceFactory::GetForProfile(profile),
                       SigninManagerFactory::GetForProfile(profile),
                       browser_context->IsOffTheRecord());
  }

  TestAutofillDialogView* GetView() {
    return static_cast<TestAutofillDialogView*>(view());
  }

  TestPersonalDataManager* GetTestingManager() {
    return &test_manager_;
  }

  MockAddressValidator* GetMockValidator() {
    return &mock_validator_;
  }

  const GURL& open_tab_url() { return open_tab_url_; }

  void SimulateSubmitButtonDelayBegin() {
    AutofillDialogControllerImpl::SubmitButtonDelayBegin();
  }

  void SimulateSubmitButtonDelayEnd() {
    AutofillDialogControllerImpl::SubmitButtonDelayEndForTesting();
  }

  // Returns the number of times that the submit button was delayed.
  int get_submit_button_delay_count() const {
    return submit_button_delay_count_;
  }

  MOCK_METHOD0(LoadRiskFingerprintData, void());
  using AutofillDialogControllerImpl::IsEditingExistingData;
  using AutofillDialogControllerImpl::IsManuallyEditingSection;
  using AutofillDialogControllerImpl::popup_input_type;

 protected:
  PersonalDataManager* GetManager() const override {
    return const_cast<TestAutofillDialogController*>(this)->
        GetTestingManager();
  }

  AddressValidator* GetValidator() override {
    return &mock_validator_;
  }

  void OpenTabWithUrl(const GURL& url) override {
    open_tab_url_ = url;
  }

  // AutofillDialogControllerImpl calls this method before showing the dialog
  // window.
  void SubmitButtonDelayBegin() override {
    // Do not delay enabling the submit button in testing.
    submit_button_delay_count_++;
  }

 private:
  TestPersonalDataManager test_manager_;

  // A mock validator object to prevent network requests and track when
  // validation rules are loaded or validation attempts occur.
  testing::NiceMock<MockAddressValidator> mock_validator_;

  GURL open_tab_url_;

  // The number of times that the submit button was delayed.
  int submit_button_delay_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogControllerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AutofillDialogControllerTest() : form_structure_(NULL) {
    CountryNames::SetLocaleString("en-US");
  }

  // testing::Test implementation:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    Reset();
  }

  void TearDown() override {
    if (controller_)
      controller_->ViewClosed();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Reset() {
    if (controller_)
      controller_->ViewClosed();

    profile()->GetPrefs()->ClearPref(::prefs::kAutofillDialogSaveData);

    // We have to clear the old local state before creating a new one.
    scoped_local_state_.reset();
    scoped_local_state_.reset(new ScopedTestingLocalState(
        TestingBrowserProcess::GetGlobal()));

    SetUpControllerWithFormData(DefaultFormData());
  }

  FormData DefaultFormData() {
    FormData form_data;
    for (size_t i = 0; i < arraysize(kFieldsFromPage); ++i) {
      FormFieldData field;
      field.autocomplete_attribute = kFieldsFromPage[i];
      form_data.fields.push_back(field);
    }
    return form_data;
  }

  // Creates a new controller for |form_data|.
  void ResetControllerWithFormData(const FormData& form_data) {
    if (controller_)
      controller_->ViewClosed();

    AutofillClient::ResultCallback callback =
        base::Bind(&AutofillDialogControllerTest::FinishedCallback,
                   base::Unretained(this));
    controller_ = (new testing::NiceMock<TestAutofillDialogController>(
        web_contents(),
        form_data,
        GURL(kSourceUrl),
        callback))->AsWeakPtr();
    controller_->Init(profile());
  }

  // Creates a new controller for |form_data|.
  void SetUpControllerWithFormData(const FormData& form_data) {
    ResetControllerWithFormData(form_data);
    controller()->Show();
  }

  // Fills the inputs in SECTION_CC with data.
  void FillCreditCardInputs() {
    FieldValueMap cc_outputs;
    const DetailInputs& cc_inputs =
        controller()->RequestedFieldsForSection(SECTION_CC);
    for (size_t i = 0; i < cc_inputs.size(); ++i) {
      cc_outputs[cc_inputs[i].type] = cc_inputs[i].type == CREDIT_CARD_NUMBER ?
          ASCIIToUTF16(kTestCCNumberVisa) : ASCIIToUTF16("11");
    }
    controller()->GetView()->SetUserInput(SECTION_CC, cc_outputs);
  }

  // Activates the 'Add new foo' option from the |section|'s suggestions
  // dropdown and fills the |section|'s inputs with the data from the
  // |data_model|.  If |section| is SECTION_CC, also fills in '123' for the CVC.
  void FillInputs(DialogSection section, const AutofillDataModel& data_model) {
    // Select the 'Add new foo' option.
    ui::MenuModel* model = GetMenuModelForSection(section);
    if (model)
      model->ActivatedAt(model->GetItemCount() - 2);

    // Fill the inputs.
    FieldValueMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    for (size_t i = 0; i < inputs.size(); ++i) {
      ServerFieldType type = inputs[i].type;
      base::string16 output;
      if (type == CREDIT_CARD_VERIFICATION_CODE)
        output = ASCIIToUTF16("123");
      else
        output = data_model.GetInfo(AutofillType(type), "en-US");
      outputs[inputs[i].type] = output;
    }
    controller()->GetView()->SetUserInput(section, outputs);
  }

  std::vector<DialogNotification> NotificationsOfType(
      DialogNotification::Type type) {
    std::vector<DialogNotification> right_type;
    const std::vector<DialogNotification>& notifications =
        controller()->CurrentNotifications();
    for (size_t i = 0; i < notifications.size(); ++i) {
      if (notifications[i].type() == type)
        right_type.push_back(notifications[i]);
    }
    return right_type;
  }

  void UseBillingForShipping() {
    controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(0);
  }

  base::string16 ValidateCCNumber(DialogSection section,
                                  const std::string& cc_number,
                                  bool should_pass) {
    FieldValueMap outputs;
    outputs[ADDRESS_BILLING_COUNTRY] = ASCIIToUTF16("United States");
    outputs[CREDIT_CARD_NUMBER] = UTF8ToUTF16(cc_number);
    ValidityMessages messages =
        controller()->InputsAreValid(section, outputs);
    EXPECT_EQ(should_pass, !messages.HasSureError(CREDIT_CARD_NUMBER));
    return messages.GetMessageOrDefault(CREDIT_CARD_NUMBER).text;
  }

  // Returns true if the given |section| contains a field of the given |type|.
  bool SectionContainsField(DialogSection section, ServerFieldType type) {
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    for (DetailInputs::const_iterator it = inputs.begin(); it != inputs.end();
         ++it) {
      if (it->type == type)
        return true;
    }
    return false;
  }

  SuggestionsMenuModel* GetMenuModelForSection(DialogSection section) {
    ui::MenuModel* model = controller()->MenuModelForSection(section);
    return static_cast<SuggestionsMenuModel*>(model);
  }

  void SubmitAndVerifyShippingAndBillingResults() {
    // Test after setting use billing for shipping.
    UseBillingForShipping();

    controller()->OnAccept();

    ASSERT_EQ(20U, form_structure()->field_count());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY,
              form_structure()->field(11)->Type().GetStorableType());
    EXPECT_EQ(ADDRESS_BILLING, form_structure()->field(11)->Type().group());
    EXPECT_EQ(ADDRESS_HOME_COUNTRY,
              form_structure()->field(18)->Type().GetStorableType());
    EXPECT_EQ(ADDRESS_HOME, form_structure()->field(18)->Type().group());
    base::string16 billing_country = form_structure()->field(11)->value;
    EXPECT_EQ(2U, billing_country.size());
    base::string16 shipping_country = form_structure()->field(18)->value;
    EXPECT_EQ(2U, shipping_country.size());
    EXPECT_FALSE(billing_country.empty());
    EXPECT_FALSE(shipping_country.empty());
    EXPECT_EQ(billing_country, shipping_country);

    EXPECT_EQ(CREDIT_CARD_NAME_FULL,
              form_structure()->field(1)->Type().GetStorableType());
    base::string16 cc_name = form_structure()->field(1)->value;
    EXPECT_EQ(NAME_FULL, form_structure()->field(6)->Type().GetStorableType());
    EXPECT_EQ(NAME_BILLING, form_structure()->field(6)->Type().group());
    base::string16 billing_name = form_structure()->field(6)->value;
    EXPECT_EQ(NAME_FULL, form_structure()->field(13)->Type().GetStorableType());
    EXPECT_EQ(NAME, form_structure()->field(13)->Type().group());
    base::string16 shipping_name = form_structure()->field(13)->value;

    EXPECT_FALSE(cc_name.empty());
    EXPECT_FALSE(billing_name.empty());
    EXPECT_FALSE(shipping_name.empty());
    EXPECT_EQ(cc_name, billing_name);
    EXPECT_EQ(cc_name, shipping_name);
  }

  TestAutofillDialogController* controller() { return controller_.get(); }

  const FormStructure* form_structure() { return form_structure_; }

 private:
  void FinishedCallback(AutofillClient::RequestAutocompleteResult result,
                        const base::string16& debug_message,
                        const FormStructure* form_structure) {
    form_structure_ = form_structure;
  }

#if defined(OS_WIN)
  // http://crbug.com/227221
  ui::ScopedOleInitializer ole_initializer_;
#endif

  // The controller owns itself.
  base::WeakPtr<TestAutofillDialogController> controller_;

  // Returned when the dialog closes successfully.
  const FormStructure* form_structure_;

  std::unique_ptr<ScopedTestingLocalState> scoped_local_state_;
};

}  // namespace

TEST_F(AutofillDialogControllerTest, RefuseToShowWithNoAutocompleteAttributes) {
  FormFieldData email_field;
  email_field.name = ASCIIToUTF16("email");
  FormFieldData cc_field;
  cc_field.name = ASCIIToUTF16("cc");
  FormFieldData billing_field;
  billing_field.name = ASCIIToUTF16("billing name");

  FormData form_data;
  form_data.fields.push_back(email_field);
  form_data.fields.push_back(cc_field);
  form_data.fields.push_back(billing_field);

  SetUpControllerWithFormData(form_data);
  EXPECT_FALSE(controller());
}

TEST_F(AutofillDialogControllerTest, RefuseToShowWithNoCcField) {
  FormFieldData shipping_tel;
  shipping_tel.autocomplete_attribute = "shipping tel";

  FormData form_data;
  form_data.fields.push_back(shipping_tel);

  SetUpControllerWithFormData(form_data);
  EXPECT_FALSE(controller());

  // Any cc- field will do.
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-csc";
  form_data.fields.push_back(cc_field);

  SetUpControllerWithFormData(form_data);
  EXPECT_TRUE(controller());
}

// Ensure the default ValidityMessage has the expected values.
TEST_F(AutofillDialogControllerTest, DefaultValidityMessage) {
  ValidityMessages messages;
  ValidityMessage message = messages.GetMessageOrDefault(UNKNOWN_TYPE);
  EXPECT_FALSE(message.sure);
  EXPECT_TRUE(message.text.empty());
}

// This test makes sure nothing falls over when fields are being validity-
// checked.
TEST_F(AutofillDialogControllerTest, ValidityCheck) {
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    const DetailInputs& shipping_inputs =
        controller()->RequestedFieldsForSection(section);
    for (DetailInputs::const_iterator iter = shipping_inputs.begin();
         iter != shipping_inputs.end(); ++iter) {
      controller()->InputValidityMessage(section, iter->type, base::string16());
    }
  }
}

// Test for phone number validation.
TEST_F(AutofillDialogControllerTest, PhoneNumberValidation) {
  for (size_t i = 0; i < 2; ++i) {
    ServerFieldType phone = i == 0 ? PHONE_HOME_WHOLE_NUMBER :
                                     PHONE_BILLING_WHOLE_NUMBER;
    ServerFieldType address = i == 0 ? ADDRESS_HOME_COUNTRY :
                                       ADDRESS_BILLING_COUNTRY;
    DialogSection section = i == 0 ? SECTION_SHIPPING : SECTION_BILLING;

    FieldValueMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    AutofillProfile full_profile(test::GetVerifiedProfile());
    for (size_t j = 0; j < inputs.size(); ++j) {
      const ServerFieldType type = inputs[j].type;
      outputs[type] = full_profile.GetInfo(AutofillType(type), "en-US");
    }

    // Make sure country is United States.
    outputs[address] = ASCIIToUTF16("United States");

    // Existing data should have no errors.
    ValidityMessages messages = controller()->InputsAreValid(section, outputs);
    EXPECT_FALSE(HasAnyError(messages, phone));

    // Input an empty phone number.
    outputs[phone] = base::string16();
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_TRUE(HasUnsureError(messages, phone));

    // Input an invalid phone number.
    outputs[phone] = ASCIIToUTF16("ABC");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_TRUE(messages.HasSureError(phone));

    // Input a local phone number.
    outputs[phone] = ASCIIToUTF16("2155546699");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_FALSE(HasAnyError(messages, phone));

    // Input an invalid local phone number.
    outputs[phone] = ASCIIToUTF16("215554669");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_TRUE(messages.HasSureError(phone));

    // Input an international phone number.
    outputs[phone] = ASCIIToUTF16("+33 892 70 12 39");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_FALSE(HasAnyError(messages, phone));

    // Input an invalid international phone number.
    outputs[phone] = ASCIIToUTF16("+112333 892 70 12 39");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_TRUE(messages.HasSureError(phone));

    // Input a valid Canadian number.
    outputs[phone] = ASCIIToUTF16("+1 506 887 1234");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_FALSE(HasAnyError(messages, phone));

    // Input a valid Canadian number without the country code.
    outputs[phone] = ASCIIToUTF16("506 887 1234");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_TRUE(HasAnyError(messages, phone));

    // Input a valid Canadian toll-free number.
    outputs[phone] = ASCIIToUTF16("310 1234");
    messages = controller()->InputsAreValid(section, outputs);
    EXPECT_TRUE(HasAnyError(messages, phone));
  }
}

TEST_F(AutofillDialogControllerTest, ExpirationDateValidity) {
  ui::ComboboxModel* exp_year_model =
      controller()->ComboboxModelForAutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR);
  ui::ComboboxModel* exp_month_model =
      controller()->ComboboxModelForAutofillType(CREDIT_CARD_EXP_MONTH);

  base::string16 default_year_value =
      exp_year_model->GetItemAt(exp_year_model->GetDefaultIndex());
  base::string16 default_month_value =
      exp_month_model->GetItemAt(exp_month_model->GetDefaultIndex());

  base::string16 other_year_value =
      exp_year_model->GetItemAt(exp_year_model->GetItemCount() - 1);
  base::string16 other_month_value =
      exp_month_model->GetItemAt(exp_month_model->GetItemCount() - 1);

  FieldValueMap outputs;
  outputs[CREDIT_CARD_EXP_MONTH] = default_month_value;
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = default_year_value;

  // Expiration default values generate unsure validation errors (but not sure).
  ValidityMessages messages = controller()->InputsAreValid(SECTION_CC, outputs);
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_MONTH));

  // Expiration date with default month fails.
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = other_year_value;
  messages = controller()->InputsAreValid(SECTION_CC, outputs);
  EXPECT_FALSE(HasUnsureError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_MONTH));

  // Expiration date with default year fails.
  outputs[CREDIT_CARD_EXP_MONTH] = other_month_value;
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = default_year_value;
  messages = controller()->InputsAreValid(SECTION_CC, outputs);
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_FALSE(HasUnsureError(messages, CREDIT_CARD_EXP_MONTH));
}

TEST_F(AutofillDialogControllerTest, BillingNameValidation) {
  FieldValueMap outputs;
  outputs[ADDRESS_BILLING_COUNTRY] = ASCIIToUTF16("United States");

  // Input an empty billing name.
  outputs[NAME_BILLING_FULL] = base::string16();
  ValidityMessages messages = controller()->InputsAreValid(SECTION_BILLING,
                                                           outputs);
  EXPECT_TRUE(HasUnsureError(messages, NAME_BILLING_FULL));

  // Input a non-empty billing name.
  outputs[NAME_BILLING_FULL] = ASCIIToUTF16("Bob");
  messages = controller()->InputsAreValid(SECTION_BILLING, outputs);
  EXPECT_FALSE(HasAnyError(messages, NAME_BILLING_FULL));
}

TEST_F(AutofillDialogControllerTest, CreditCardNumberValidation) {
  // Should accept AMEX, Visa, Master and Discover.
  ValidateCCNumber(SECTION_CC, kTestCCNumberVisa, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberDiscover, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberAmex, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberIncomplete, false);
  ValidateCCNumber(SECTION_CC, kTestCCNumberInvalid, false);
}

TEST_F(AutofillDialogControllerTest, AutofillProfiles) {
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  // Since the PersonalDataManager is empty, this should only have the
  // "use billing", "add new" and "manage" menu items.
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());
  // On the other hand, the other models should be NULL when there's no
  // suggestion.
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_BILLING));

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(3);

  // Empty profiles are ignored.
  AutofillProfile empty_profile(base::GenerateGUID(), kSettingsOrigin);
  empty_profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Doe"));
  controller()->GetTestingManager()->AddTestingProfile(&empty_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());

  // An otherwise full but unverified profile should be ignored.
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.set_origin("https://www.example.com");
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());

  // A full, verified profile should be picked up.
  AutofillProfile verified_profile(test::GetVerifiedProfile());
  verified_profile.SetRawInfo(ADDRESS_HOME_LINE2, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(4, shipping_model->GetItemCount());
}

// Checks that a valid profile is selected by default, but if invalid is
// popped into edit mode.
TEST_F(AutofillDialogControllerTest, AutofillProfilesPopInvalidIntoEdit) {
  SuggestionsMenuModel* shipping_model =
      GetMenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(3, shipping_model->GetItemCount());
  // "Same as billing" is selected.
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_BILLING));

  AutofillProfile verified_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));

  // Now make up a problem and make sure the profile isn't in the list.
  Reset();
  FieldProblemMap problems;
  problems.insert(std::make_pair(::i18n::addressinput::POSTAL_CODE,
                                 ::i18n::addressinput::MISMATCHING_VALUE));
  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("US"), _, _)).
      WillRepeatedly(DoAll(SetArgPointee<2>(problems),
                           Return(AddressValidator::SUCCESS)));

  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  shipping_model = GetMenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_BILLING));
}

// Makes sure suggestion profiles are re-validated when validation rules load.
TEST_F(AutofillDialogControllerTest, AutofillProfilesRevalidateAfterRulesLoad) {
  SuggestionsMenuModel* shipping_model =
      GetMenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(3, shipping_model->GetItemCount());
  // "Same as billing" is selected.
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_BILLING));
  AutofillProfile verified_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));
  EXPECT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));

  FieldProblemMap problems;
  problems.insert(std::make_pair(::i18n::addressinput::POSTAL_CODE,
                                 ::i18n::addressinput::MISMATCHING_VALUE));
  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("US"), _, _)).
      WillRepeatedly(DoAll(SetArgPointee<2>(problems),
                           Return(AddressValidator::SUCCESS)));

  controller()->OnAddressValidationRulesLoaded("US", true);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->IsManuallyEditingSection(SECTION_BILLING));
}

// Makes sure that the choice of which Autofill profile to use for each section
// is sticky.
TEST_F(AutofillDialogControllerTest, AutofillProfileDefaults) {
  AutofillProfile profile(test::GetVerifiedProfile());
  AutofillProfile profile2(test::GetVerifiedProfile2());
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  controller()->GetTestingManager()->AddTestingProfile(&profile2);

  // Until a selection has been made, the default shipping suggestion is the
  // first one (after "use billing").
  SuggestionsMenuModel* shipping_model =
      GetMenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(1, shipping_model->checked_item());

  for (int i = 2; i >= 0; --i) {
    shipping_model = GetMenuModelForSection(SECTION_SHIPPING);
    shipping_model->ExecuteCommand(i, 0);
    FillCreditCardInputs();
    controller()->OnAccept();

    Reset();
    controller()->GetTestingManager()->AddTestingProfile(&profile);
    controller()->GetTestingManager()->AddTestingProfile(&profile2);
    shipping_model = GetMenuModelForSection(SECTION_SHIPPING);
    EXPECT_EQ(i, shipping_model->checked_item());
  }

  // Try again, but don't add the default profile to the PDM. The dialog
  // should fall back to the first profile.
  shipping_model->ExecuteCommand(2, 0);
  FillCreditCardInputs();
  controller()->OnAccept();
  Reset();
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  shipping_model = GetMenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(1, shipping_model->checked_item());
}

// Makes sure that a newly added Autofill profile becomes set as the default
// choice for the next run.
TEST_F(AutofillDialogControllerTest, NewAutofillProfileIsDefault) {

  AutofillProfile profile(test::GetVerifiedProfile());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  // Until a selection has been made, the default suggestion is the first one.
  // For the shipping section, this follows the "use billing" suggestion.
  EXPECT_EQ(0, GetMenuModelForSection(SECTION_CC)->checked_item());
  EXPECT_EQ(1, GetMenuModelForSection(SECTION_SHIPPING)->checked_item());

  // Fill in the shipping and credit card sections with new data.
  AutofillProfile new_profile(test::GetVerifiedProfile2());
  CreditCard new_credit_card(test::GetVerifiedCreditCard2());
  FillInputs(SECTION_SHIPPING, new_profile);
  FillInputs(SECTION_CC, new_credit_card);
  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(true);
  controller()->OnAccept();

  // Update the |new_profile| and |new_credit_card|'s guids to the saved ones.
  new_profile.set_guid(
      controller()->GetTestingManager()->imported_profile().guid());
  new_credit_card.set_guid(
      controller()->GetTestingManager()->imported_credit_card().guid());

  // Reload the dialog. The newly added address should now be set as the
  // default.
  Reset();
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  controller()->GetTestingManager()->AddTestingProfile(&new_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  // Until a selection has been made, the default suggestion is the first one.
  // For the shipping section, this follows the "use billing" suggestion.
  EXPECT_EQ(2, GetMenuModelForSection(SECTION_SHIPPING)->checked_item());
}

TEST_F(AutofillDialogControllerTest, SuggestValidEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  const base::string16 kValidEmail = ASCIIToUTF16(kFakeEmail);
  profile.SetRawInfo(EMAIL_ADDRESS, kValidEmail);
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  // "add", "manage", and 1 suggestion.
  EXPECT_EQ(
      3, controller()->MenuModelForSection(SECTION_BILLING)->GetItemCount());
  // "add", "manage", 1 suggestion, and "same as billing".
  EXPECT_EQ(
      4, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestInvalidEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(".!#$%&'*+/=?^_`-@-.."));
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
  // "add", "manage", 1 suggestion, and "same as billing".
  EXPECT_EQ(
      4, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, SuggestValidAddress) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  // "add", "manage", and 1 suggestion.
  EXPECT_EQ(
      3, controller()->MenuModelForSection(SECTION_BILLING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestInvalidAddress) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("C"));
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestIncompleteAddress) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  // Same as shipping, manage, add new.
  EXPECT_EQ(3,
      controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
}

TEST_F(AutofillDialogControllerTest, DoSuggestShippingAddressWithoutEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  // Same as shipping, manage, add new, profile with missing email.
  EXPECT_EQ(4,
      controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
  // Billing addresses require email.
  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
}

TEST_F(AutofillDialogControllerTest, AutofillCreditCards) {
  // Since the PersonalDataManager is empty, this should only have the
  // default menu items.
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(3);

  // Empty cards are ignored.
  CreditCard empty_card(base::GenerateGUID(), kSettingsOrigin);
  empty_card.SetRawInfo(CREDIT_CARD_NAME_FULL, ASCIIToUTF16("John Doe"));
  controller()->GetTestingManager()->AddTestingCreditCard(&empty_card);
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));

  // An otherwise full but unverified card should be ignored.
  CreditCard full_card(test::GetCreditCard());
  full_card.set_origin("https://www.example.com");
  controller()->GetTestingManager()->AddTestingCreditCard(&full_card);
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));

  // A full, verified card should be picked up.
  CreditCard verified_card(test::GetCreditCard());
  verified_card.set_origin(kSettingsOrigin);
  controller()->GetTestingManager()->AddTestingCreditCard(&verified_card);
  ui::MenuModel* credit_card_model =
      controller()->MenuModelForSection(SECTION_CC);
  ASSERT_TRUE(credit_card_model);
  EXPECT_EQ(3, credit_card_model->GetItemCount());
}

// Test selecting a shipping address different from billing as address.
TEST_F(AutofillDialogControllerTest, DontUseBillingAsShipping) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  AutofillProfile full_profile2(test::GetVerifiedProfile2());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile2);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  shipping_model->ActivatedAt(2);

  controller()->OnAccept();
  ASSERT_EQ(20U, form_structure()->field_count());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure()->field(9)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_BILLING, form_structure()->field(9)->Type().group());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure()->field(16)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME, form_structure()->field(16)->Type().group());
  base::string16 billing_state = form_structure()->field(9)->value;
  base::string16 shipping_state = form_structure()->field(16)->value;
  EXPECT_FALSE(billing_state.empty());
  EXPECT_FALSE(shipping_state.empty());
  EXPECT_NE(billing_state, shipping_state);

  EXPECT_EQ(CREDIT_CARD_NAME_FULL,
            form_structure()->field(1)->Type().GetStorableType());
  base::string16 cc_name = form_structure()->field(1)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(6)->Type().GetStorableType());
  EXPECT_EQ(NAME_BILLING, form_structure()->field(6)->Type().group());
  base::string16 billing_name = form_structure()->field(6)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(13)->Type().GetStorableType());
  EXPECT_EQ(NAME, form_structure()->field(13)->Type().group());
  base::string16 shipping_name = form_structure()->field(13)->value;

  EXPECT_FALSE(cc_name.empty());
  EXPECT_FALSE(billing_name.empty());
  EXPECT_FALSE(shipping_name.empty());
  // Billing name should always be the same as cardholder name.
  EXPECT_EQ(cc_name, billing_name);
  EXPECT_NE(cc_name, shipping_name);
}

// Test selecting UseBillingForShipping.
TEST_F(AutofillDialogControllerTest, UseBillingAsShipping) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  AutofillProfile full_profile2(test::GetVerifiedProfile2());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile2);

  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  ASSERT_FALSE(controller()->IsManuallyEditingSection(SECTION_CC));
  ASSERT_FALSE(controller()->IsManuallyEditingSection(SECTION_BILLING));

  SubmitAndVerifyShippingAndBillingResults();
}

TEST_F(AutofillDialogControllerTest, UseBillingAsShippingManualInput) {
  ASSERT_TRUE(controller()->IsManuallyEditingSection(SECTION_CC));
  ASSERT_TRUE(controller()->IsManuallyEditingSection(SECTION_BILLING));

  CreditCard credit_card(test::GetVerifiedCreditCard());
  FillInputs(SECTION_CC, credit_card);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  FillInputs(SECTION_BILLING, full_profile);

  SubmitAndVerifyShippingAndBillingResults();
}

// Tests that shipping and billing telephone fields are supported, and filled
// in by their respective profiles. http://crbug.com/244515
TEST_F(AutofillDialogControllerTest, BillingVsShippingPhoneNumber) {
  FormFieldData shipping_tel;
  shipping_tel.autocomplete_attribute = "shipping tel";
  FormFieldData billing_tel;
  billing_tel.autocomplete_attribute = "billing tel";
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-csc";

  FormData form_data;
  form_data.fields.push_back(shipping_tel);
  form_data.fields.push_back(billing_tel);
  form_data.fields.push_back(cc_field);
  SetUpControllerWithFormData(form_data);

  // The profile that will be chosen for the shipping section.
  AutofillProfile shipping_profile(test::GetVerifiedProfile());
  // The profile that will be chosen for the billing section.
  AutofillProfile billing_profile(test::GetVerifiedProfile2());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&shipping_profile);
  controller()->GetTestingManager()->AddTestingProfile(&billing_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  ui::MenuModel* billing_model =
      controller()->MenuModelForSection(SECTION_BILLING);
  billing_model->ActivatedAt(1);

  controller()->OnAccept();
  ASSERT_EQ(3U, form_structure()->field_count());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure()->field(0)->Type().GetStorableType());
  EXPECT_EQ(PHONE_HOME, form_structure()->field(0)->Type().group());
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER,
            form_structure()->field(1)->Type().GetStorableType());
  EXPECT_EQ(PHONE_BILLING, form_structure()->field(1)->Type().group());
  EXPECT_EQ(shipping_profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            form_structure()->field(0)->value);
  EXPECT_EQ(billing_profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            form_structure()->field(1)->value);
  EXPECT_NE(form_structure()->field(1)->value,
            form_structure()->field(0)->value);
}

// Similar to the above, but tests that street-address (i.e. all lines of the
// street address) is successfully filled for both shipping and billing
// sections.
TEST_F(AutofillDialogControllerTest, BillingVsShippingStreetAddress) {
  FormFieldData shipping_address;
  shipping_address.autocomplete_attribute = "shipping street-address";
  FormFieldData billing_address;
  billing_address.autocomplete_attribute = "billing street-address";
  FormFieldData shipping_address_textarea;
  shipping_address_textarea.autocomplete_attribute = "shipping street-address";
  shipping_address_textarea.form_control_type = "textarea";
  FormFieldData billing_address_textarea;
  billing_address_textarea.autocomplete_attribute = "billing street-address";
  billing_address_textarea.form_control_type = "textarea";
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-csc";

  FormData form_data;
  form_data.fields.push_back(shipping_address);
  form_data.fields.push_back(billing_address);
  form_data.fields.push_back(shipping_address_textarea);
  form_data.fields.push_back(billing_address_textarea);
  form_data.fields.push_back(cc_field);
  SetUpControllerWithFormData(form_data);

  // The profile that will be chosen for the shipping section.
  AutofillProfile shipping_profile(test::GetVerifiedProfile());
  // The profile that will be chosen for the billing section.
  AutofillProfile billing_profile(test::GetVerifiedProfile2());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&shipping_profile);
  controller()->GetTestingManager()->AddTestingProfile(&billing_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  ui::MenuModel* billing_model =
      controller()->MenuModelForSection(SECTION_BILLING);
  billing_model->ActivatedAt(1);

  controller()->OnAccept();
  ASSERT_EQ(5U, form_structure()->field_count());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            form_structure()->field(0)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME, form_structure()->field(0)->Type().group());
  EXPECT_EQ(ADDRESS_HOME_STREET_ADDRESS,
            form_structure()->field(1)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_BILLING, form_structure()->field(1)->Type().group());
  // Inexact matching; single-line inputs get the address data concatenated but
  // separated by commas.
  EXPECT_TRUE(base::StartsWith(form_structure()->field(0)->value,
                               shipping_profile.GetRawInfo(ADDRESS_HOME_LINE1),
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::EndsWith(form_structure()->field(0)->value,
                             shipping_profile.GetRawInfo(ADDRESS_HOME_LINE2),
                             base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::StartsWith(form_structure()->field(1)->value,
                               billing_profile.GetRawInfo(ADDRESS_HOME_LINE1),
                               base::CompareCase::SENSITIVE));
  EXPECT_TRUE(base::EndsWith(form_structure()->field(1)->value,
                             billing_profile.GetRawInfo(ADDRESS_HOME_LINE2),
                             base::CompareCase::SENSITIVE));
  // The textareas should be an exact match.
  EXPECT_EQ(shipping_profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            form_structure()->field(2)->value);
  EXPECT_EQ(billing_profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS),
            form_structure()->field(3)->value);

  EXPECT_NE(form_structure()->field(1)->value,
            form_structure()->field(0)->value);
  EXPECT_NE(form_structure()->field(3)->value,
            form_structure()->field(2)->value);
}

// Test asking for different pieces of the name.
TEST_F(AutofillDialogControllerTest, NamePieces) {
  const char* const attributes[] = {
      "shipping name",
      "billing name",
      "billing given-name",
      "billing family-name",
      "billing additional-name",
      "cc-csc"
  };

  FormData form_data;
  for (size_t i = 0; i < arraysize(attributes); ++i) {
    FormFieldData field;
    field.autocomplete_attribute.assign(attributes[i]);
    form_data.fields.push_back(field);
  }

  SetUpControllerWithFormData(form_data);

  // Billing.
  AutofillProfile test_profile(test::GetVerifiedProfile());
  test_profile.SetInfo(AutofillType(NAME_FULL),
                       ASCIIToUTF16("Fabian Jackson von Nacho"),
                       "en-US");
  controller()->GetTestingManager()->AddTestingProfile(&test_profile);

  // Credit card.
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  // Make shipping name different from billing.
  AutofillProfile test_profile2(test::GetVerifiedProfile2());
  test_profile2.SetInfo(AutofillType(NAME_FULL),
                        ASCIIToUTF16("Don Ford"),
                        "en-US");
  controller()->GetTestingManager()->AddTestingProfile(&test_profile2);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  shipping_model->ActivatedAt(2);

  controller()->OnAccept();

  EXPECT_EQ(NAME_FULL, form_structure()->field(0)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("Don Ford"),
            form_structure()->field(0)->value);

  EXPECT_EQ(NAME_FULL, form_structure()->field(1)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("Fabian Jackson von Nacho"),
            form_structure()->field(1)->value);

  EXPECT_EQ(NAME_FIRST, form_structure()->field(2)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("Fabian"),
            form_structure()->field(2)->value);

  EXPECT_EQ(NAME_LAST, form_structure()->field(3)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("von Nacho"),
            form_structure()->field(3)->value);

  EXPECT_EQ(NAME_MIDDLE, form_structure()->field(4)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("Jackson"),
            form_structure()->field(4)->value);
}

// Tests that adding an autofill profile and then submitting works.
TEST_F(AutofillDialogControllerTest, AddAutofillProfile) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_BILLING);
  // Activate the "Add billing address" menu item.
  model->ActivatedAt(model->GetItemCount() - 2);

  // Fill in the inputs from the profile.
  FieldValueMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  AutofillProfile full_profile2(test::GetVerifiedProfile2());
  for (size_t i = 0; i < inputs.size(); ++i) {
    const ServerFieldType type = inputs[i].type;
    outputs[type] = full_profile2.GetInfo(AutofillType(type), "en-US");
  }
  controller()->GetView()->SetUserInput(SECTION_BILLING, outputs);

  controller()->OnAccept();
  const AutofillProfile& added_profile =
      controller()->GetTestingManager()->imported_profile();

  const DetailInputs& shipping_inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  for (size_t i = 0; i < shipping_inputs.size(); ++i) {
    const ServerFieldType type = shipping_inputs[i].type;
    EXPECT_EQ(full_profile2.GetInfo(AutofillType(type), "en-US"),
              added_profile.GetInfo(AutofillType(type), "en-US"));
  }
}

TEST_F(AutofillDialogControllerTest, SaveDetailsInChrome) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(4);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(0);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(1);
  EXPECT_TRUE(controller()->ShouldOfferToSaveInChrome());

  profile()->GetPrefs()->SetBoolean(prefs::kAutofillEnabled, false);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  profile()->GetPrefs()->SetBoolean(prefs::kAutofillEnabled, true);
  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(1);
  EXPECT_TRUE(controller()->ShouldOfferToSaveInChrome());

  profile()->ForceIncognito(true);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());
}

TEST_F(AutofillDialogControllerTest, DisabledAutofill) {
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(prefs::kAutofillEnabled));

  AutofillProfile verified_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);

  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  // Verify suggestions menus should be showing when Autofill is enabled.
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC));
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_BILLING));
  EXPECT_EQ(
      4, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());

  EXPECT_CALL(*controller()->GetView(), ModelChanged());
  profile()->GetPrefs()->SetBoolean(prefs::kAutofillEnabled, false);

  // Verify billing and credit card suggestions menus are hidden when Autofill
  // is disabled.
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_BILLING));
  // And that the shipping suggestions menu has less selections.
  EXPECT_EQ(
      2, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());

  // Additionally, editing fields should not show Autofill popups.
  ASSERT_NO_FATAL_FAILURE(controller()->UserEditedOrActivatedInput(
      SECTION_BILLING, NAME_BILLING_FULL, gfx::NativeView(), gfx::Rect(),
      verified_profile.GetInfo(AutofillType(NAME_FULL), "en-US").substr(0, 1),
      true));
  EXPECT_EQ(UNKNOWN_TYPE, controller()->popup_input_type());
}

TEST_F(AutofillDialogControllerTest, ShippingSectionCanBeHidden) {
  FormFieldData email_field;
  email_field.autocomplete_attribute = "email";
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-number";
  FormFieldData billing_field;
  billing_field.autocomplete_attribute = "billing address-level1";

  FormData form_data;
  form_data.fields.push_back(email_field);
  form_data.fields.push_back(cc_field);
  form_data.fields.push_back(billing_field);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  SetUpControllerWithFormData(form_data);

  EXPECT_FALSE(controller()->SectionIsActive(SECTION_SHIPPING));

  FillCreditCardInputs();
  controller()->OnAccept();
  EXPECT_TRUE(form_structure());
}

TEST_F(AutofillDialogControllerTest, SaveInChromeByDefault) {
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
  FillCreditCardInputs();
  controller()->OnAccept();
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
}

TEST_F(AutofillDialogControllerTest,
       SaveInChromePreferenceNotRememberedOnCancel) {
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(false);
  controller()->OnCancel();
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
}

TEST_F(AutofillDialogControllerTest,
       SaveInChromePreferenceRememberedOnSuccess) {
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
  FillCreditCardInputs();
  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(false);
  controller()->OnAccept();
  EXPECT_FALSE(controller()->ShouldSaveInChrome());
}

TEST_F(AutofillDialogControllerTest, SubmitButtonIsDisabled) {
  EXPECT_EQ(1, controller()->get_submit_button_delay_count());

  // Begin the submit button delay.
  controller()->SimulateSubmitButtonDelayBegin();
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // End the submit button delay.
  controller()->SimulateSubmitButtonDelayEnd();
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

TEST_F(AutofillDialogControllerTest, IconsForFields_NoCreditCard) {
  FieldValueMap values;
  values[EMAIL_ADDRESS] = ASCIIToUTF16(kFakeEmail);
  FieldIconMap icons = controller()->IconsForFields(values);
  EXPECT_TRUE(icons.empty());
}

TEST_F(AutofillDialogControllerTest, IconsForFields_CreditCardNumberOnly) {
  FieldValueMap values;
  values[EMAIL_ADDRESS] = ASCIIToUTF16(kFakeEmail);
  values[CREDIT_CARD_NUMBER] = ASCIIToUTF16(kTestCCNumberVisa);
  FieldIconMap icons = controller()->IconsForFields(values);
  EXPECT_EQ(1UL, icons.size());
  EXPECT_EQ(1UL, icons.count(CREDIT_CARD_NUMBER));
}

TEST_F(AutofillDialogControllerTest, IconsForFields_CvcOnly) {
  FieldValueMap values;
  values[EMAIL_ADDRESS] = ASCIIToUTF16(kFakeEmail);
  values[CREDIT_CARD_VERIFICATION_CODE] = ASCIIToUTF16("123");
  FieldIconMap icons = controller()->IconsForFields(values);
  EXPECT_EQ(1UL, icons.size());
  EXPECT_EQ(1UL, icons.count(CREDIT_CARD_VERIFICATION_CODE));
}

TEST_F(AutofillDialogControllerTest, IconsForFields_BothCreditCardAndCvc) {
  FieldValueMap values;
  values[EMAIL_ADDRESS] = ASCIIToUTF16(kFakeEmail);
  values[CREDIT_CARD_NUMBER] = ASCIIToUTF16(kTestCCNumberVisa);
  values[CREDIT_CARD_VERIFICATION_CODE] = ASCIIToUTF16("123");
  FieldIconMap icons = controller()->IconsForFields(values);
  EXPECT_EQ(2UL, icons.size());
  EXPECT_EQ(1UL, icons.count(CREDIT_CARD_VERIFICATION_CODE));
  EXPECT_EQ(1UL, icons.count(CREDIT_CARD_NUMBER));
}

TEST_F(AutofillDialogControllerTest, FieldControlsIcons) {
  EXPECT_TRUE(controller()->FieldControlsIcons(CREDIT_CARD_NUMBER));
  EXPECT_FALSE(controller()->FieldControlsIcons(CREDIT_CARD_VERIFICATION_CODE));
  EXPECT_FALSE(controller()->FieldControlsIcons(EMAIL_ADDRESS));
}

// Verifies that a call to the IconsForFields() method before the card type is
// known returns a placeholder image that is at least as large as the icons for
// all of the supported major credit card issuers.
TEST_F(AutofillDialogControllerTest, IconReservedForCreditCardField) {
  FieldValueMap inputs;
  inputs[CREDIT_CARD_NUMBER] = base::string16();

  FieldIconMap icons = controller()->IconsForFields(inputs);
  EXPECT_EQ(1U, icons.size());

  ASSERT_EQ(1U, icons.count(CREDIT_CARD_NUMBER));
  gfx::Image placeholder_icon = icons[CREDIT_CARD_NUMBER];

  // Verify that the placeholder icon is at least as large as the icons for the
  // supported credit card issuers.
  const int kSupportedCardIdrs[] = {
    IDR_AUTOFILL_CC_AMEX,
    IDR_AUTOFILL_CC_DISCOVER,
    IDR_AUTOFILL_CC_GENERIC,
    IDR_AUTOFILL_CC_MASTERCARD,
    IDR_AUTOFILL_CC_VISA,
  };
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (size_t i = 0; i < arraysize(kSupportedCardIdrs); ++i) {
    SCOPED_TRACE(base::SizeTToString(i));
    gfx::Image supported_card_icon = rb.GetImageNamed(kSupportedCardIdrs[i]);
    EXPECT_GE(placeholder_icon.Width(), supported_card_icon.Width());
    EXPECT_GE(placeholder_icon.Height(), supported_card_icon.Height());
  }
}

TEST_F(AutofillDialogControllerTest, CountryChangeUpdatesSection) {
  TestAutofillDialogView* view = controller()->GetView();
  view->ClearSectionUpdates();

  controller()->UserEditedOrActivatedInput(SECTION_SHIPPING,
                                           ADDRESS_HOME_COUNTRY,
                                           gfx::NativeView(),
                                           gfx::Rect(),
                                           ASCIIToUTF16("Belarus"),
                                           true);
  std::map<DialogSection, size_t> updates = view->section_updates();
  EXPECT_EQ(1U, updates[SECTION_SHIPPING]);
  EXPECT_EQ(1U, updates.size());

  view->ClearSectionUpdates();

  controller()->UserEditedOrActivatedInput(SECTION_BILLING,
                                           ADDRESS_BILLING_COUNTRY,
                                           gfx::NativeView(),
                                           gfx::Rect(),
                                           ASCIIToUTF16("Italy"),
                                           true);
  updates = view->section_updates();
  EXPECT_EQ(1U, updates[SECTION_BILLING]);
  EXPECT_EQ(1U, updates.size());
}

TEST_F(AutofillDialogControllerTest, CorrectCountryFromInputs) {
  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("DE"), _, _));

  FieldValueMap billing_inputs;
  billing_inputs[ADDRESS_BILLING_COUNTRY] = ASCIIToUTF16("Germany");
  controller()->InputsAreValid(SECTION_BILLING, billing_inputs);

  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("FR"), _, _));

  FieldValueMap shipping_inputs;
  shipping_inputs[ADDRESS_HOME_COUNTRY] = ASCIIToUTF16("France");
  controller()->InputsAreValid(SECTION_SHIPPING, shipping_inputs);
}

TEST_F(AutofillDialogControllerTest, ValidationRulesLoadedOnCountryChange) {
  ResetControllerWithFormData(DefaultFormData());
  EXPECT_CALL(*controller()->GetMockValidator(),
              LoadRules("US")).Times(AtLeast(1));
  controller()->Show();

  EXPECT_CALL(*controller()->GetMockValidator(), LoadRules("FR"));
  controller()->UserEditedOrActivatedInput(SECTION_BILLING,
                                           ADDRESS_BILLING_COUNTRY,
                                           gfx::NativeView(),
                                           gfx::Rect(),
                                           ASCIIToUTF16("France"),
                                           true);
}

TEST_F(AutofillDialogControllerTest, UsValidationRulesLoadedForJpOnlyProfile) {
  ResetControllerWithFormData(DefaultFormData());
  AutofillProfile jp_profile(base::GenerateGUID(), kSettingsOrigin);
  jp_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("JP"));
  controller()->GetTestingManager()->AddTestingProfile(&jp_profile);
  EXPECT_CALL(*controller()->GetMockValidator(), LoadRules("US")).Times(0);
  EXPECT_CALL(*controller()->GetMockValidator(),
              LoadRules("JP")).Times(AtLeast(1));
  controller()->Show();
}

TEST_F(AutofillDialogControllerTest, InvalidWhenRulesNotReady) {
  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);

  // If the rules haven't loaded yet, validation errors should show on submit.
  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("US"), _, _)).
              WillRepeatedly(Return(AddressValidator::RULES_NOT_READY));

  FieldValueMap inputs;
  inputs[ADDRESS_HOME_ZIP] = ASCIIToUTF16("1234");
  inputs[ADDRESS_HOME_COUNTRY] = ASCIIToUTF16("United States");

  ValidityMessages messages =
      controller()->InputsAreValid(SECTION_SHIPPING, inputs);
  EXPECT_FALSE(messages.GetMessageOrDefault(ADDRESS_HOME_ZIP).text.empty());
  EXPECT_FALSE(messages.HasSureError(ADDRESS_HOME_ZIP));
  // Country should never show an error message as it's always valid.
  EXPECT_TRUE(messages.GetMessageOrDefault(ADDRESS_HOME_COUNTRY).text.empty());
}

TEST_F(AutofillDialogControllerTest, ValidButUnverifiedWhenRulesFail) {
  // Add suggestions so the credit card and billing sections aren't showing
  // their manual inputs (to isolate to just shipping).
  AutofillProfile verified_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  CreditCard verified_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&verified_card);

  // Select "Add new shipping address...".
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(2);

  // If the rules are unavailable, validation errors should not show.
  EXPECT_CALL(*controller()->GetMockValidator(),
              ValidateAddress(CountryCodeMatcher("US"), _, _)).
              WillRepeatedly(Return(AddressValidator::RULES_UNAVAILABLE));

  FieldValueMap outputs;
  AutofillProfile full_profile(test::GetFullProfile());
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  for (size_t i = 0; i < inputs.size(); ++i) {
    const ServerFieldType type = inputs[i].type;
    outputs[type] = full_profile.GetInfo(AutofillType(type), "en-US");
  }
  controller()->GetView()->SetUserInput(SECTION_SHIPPING, outputs);
  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(true);
  controller()->OnAccept();

  // Profiles saved while rules are unavailable shouldn't be verified.
  const AutofillProfile& imported_profile =
      controller()->GetTestingManager()->imported_profile();
  ASSERT_EQ(imported_profile.GetInfo(AutofillType(NAME_FULL), "en-US"),
            full_profile.GetInfo(AutofillType(NAME_FULL), "en-US"));
  EXPECT_EQ(imported_profile.origin(), GURL(kSourceUrl).GetOrigin().spec());
  EXPECT_FALSE(imported_profile.IsVerified());
}

TEST_F(AutofillDialogControllerTest, LimitedCountryChoices) {
  ui::ComboboxModel* shipping_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_HOME_COUNTRY);
  const int default_number_of_countries =
      shipping_country_model->GetItemCount();
  // We show a lot of countries by default, but the exact number doesn't matter.
  EXPECT_GT(default_number_of_countries, 50);

  // Create a form data that simulates:
  //   <select autocomplete="billing country">
  //     <option value="AU">Down Under</option>
  //     <option value="">fR</option>  <!-- Case doesn't matter -->
  //     <option value="GRMNY">Germany</option>
  //   </select>
  // Only country codes are respected, whether they're in value or the option's
  // text content. Thus the first two options should be recognized.
  FormData form_data;
  FormFieldData field;
  field.autocomplete_attribute = "billing country";
  field.option_contents.push_back(ASCIIToUTF16("Down Under"));
  field.option_values.push_back(ASCIIToUTF16("AU"));
  field.option_contents.push_back(ASCIIToUTF16("Fr"));
  field.option_values.push_back(ASCIIToUTF16(""));
  field.option_contents.push_back(ASCIIToUTF16("Germany"));
  field.option_values.push_back(ASCIIToUTF16("GRMNY"));

  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-csc";

  form_data.fields.push_back(field);
  form_data.fields.push_back(cc_field);
  ResetControllerWithFormData(form_data);
  controller()->Show();

  // Shipping model shouldn't have changed.
  shipping_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_HOME_COUNTRY);
  EXPECT_EQ(default_number_of_countries,
            shipping_country_model->GetItemCount());
  // Billing model now only has two items.
  ui::ComboboxModel* billing_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_BILLING_COUNTRY);
  ASSERT_EQ(2, billing_country_model->GetItemCount());
  EXPECT_EQ(billing_country_model->GetItemAt(0), ASCIIToUTF16("Australia"));
  EXPECT_EQ(billing_country_model->GetItemAt(1), ASCIIToUTF16("France"));

  // Make sure it also applies to profile suggestions.
  AutofillProfile us_profile(test::GetVerifiedProfile());
  us_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  controller()->GetTestingManager()->AddTestingProfile(&us_profile);
  // Don't show a suggestion if the only one that exists is disabled.
  EXPECT_FALSE(
      controller()->SuggestionStateForSection(SECTION_BILLING).visible);

  // Add a profile with an acceptable country; suggestion should be shown.
  ResetControllerWithFormData(form_data);
  controller()->Show();
  AutofillProfile au_profile(test::GetVerifiedProfile2());
  au_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("AU"));
  controller()->GetTestingManager()->AddTestingProfile(&us_profile);
  controller()->GetTestingManager()->AddTestingProfile(&au_profile);
  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_BILLING);
  ASSERT_TRUE(model);
  EXPECT_EQ(4, model->GetItemCount());
  EXPECT_FALSE(model->IsEnabledAt(0));
  EXPECT_TRUE(model->IsEnabledAt(1));

  // Add <input type="text" autocomplete="billing country"></input>
  // This should open up selection of all countries again.
  FormFieldData field2;
  field2.autocomplete_attribute = "billing country";
  form_data.fields.push_back(field2);
  ResetControllerWithFormData(form_data);
  controller()->Show();

  billing_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_BILLING_COUNTRY);
  EXPECT_EQ(default_number_of_countries,
            billing_country_model->GetItemCount());
}

// http://crbug.com/388018
TEST_F(AutofillDialogControllerTest, NoCountryChoices) {
  // Create a form data that simulates:
  //   <select autocomplete="billing country">
  //     <option value="ATL">Atlantis</option>
  //     <option value="ELD">Eldorado</option>
  //   </select>
  // i.e. contains a list of no valid countries.
  FormData form_data;
  FormFieldData field;
  field.autocomplete_attribute = "billing country";
  field.option_contents.push_back(ASCIIToUTF16("Atlantis"));
  field.option_values.push_back(ASCIIToUTF16("ATL"));
  field.option_contents.push_back(ASCIIToUTF16("Eldorado"));
  field.option_values.push_back(ASCIIToUTF16("ELD"));

  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-csc";

  form_data.fields.push_back(field);
  form_data.fields.push_back(cc_field);
  ResetControllerWithFormData(form_data);
  controller()->Show();

  // Controller aborts and self destructs.
  EXPECT_EQ(0, controller());
}

TEST_F(AutofillDialogControllerTest, LimitedCcChoices) {
  // Typically, MC and Visa are both valid.
  ValidateCCNumber(SECTION_CC, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberVisa, true);

  FormData form_data;
  FormFieldData field;
  field.autocomplete_attribute = "billing cc-type";
  field.option_contents.push_back(ASCIIToUTF16("Visa"));
  field.option_values.push_back(ASCIIToUTF16("V"));
  field.option_contents.push_back(ASCIIToUTF16("Amex"));
  field.option_values.push_back(ASCIIToUTF16("AX"));
  form_data.fields.push_back(field);
  ResetControllerWithFormData(form_data);
  controller()->Show();

  // MC is not valid because it's missing from FormData.
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DIALOG_VALIDATION_UNACCEPTED_MASTERCARD),
            ValidateCCNumber(SECTION_CC, kTestCCNumberMaster, false));
  ValidateCCNumber(SECTION_CC, kTestCCNumberVisa, true);

  CreditCard visa_card(test::GetVerifiedCreditCard());
  CreditCard amex_card(test::GetVerifiedCreditCard2());

  CreditCard master_card(base::GenerateGUID(), kSettingsOrigin);
  test::SetCreditCardInfo(
      &master_card, "Mr Foo", "5105105105105100", "07", "2099");

  controller()->GetTestingManager()->AddTestingCreditCard(&visa_card);
  controller()->GetTestingManager()->AddTestingCreditCard(&amex_card);
  controller()->GetTestingManager()->AddTestingCreditCard(&master_card);

  // The stored MC is disabled in the dropdown.
  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_CC);
  ASSERT_TRUE(model);
  ASSERT_EQ(5, model->GetItemCount());
  EXPECT_TRUE(model->IsEnabledAt(0));
  EXPECT_TRUE(model->IsEnabledAt(1));
  EXPECT_FALSE(model->IsEnabledAt(2));
  EXPECT_TRUE(model->IsEnabledAt(3));
  EXPECT_TRUE(model->IsEnabledAt(4));

  SetUpControllerWithFormData(form_data);

  // Discover is disallowed because it's not in FormData.
  ValidateCCNumber(SECTION_CC, kTestCCNumberDiscover, false);
}

TEST_F(AutofillDialogControllerTest, SuggestCountrylessProfiles) {
  FieldValueMap outputs;
  outputs[ADDRESS_HOME_COUNTRY] = ASCIIToUTF16("US");
  controller()->GetView()->SetUserInput(SECTION_SHIPPING, outputs);

  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("The Man Without a Country"));
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  controller()->UserEditedOrActivatedInput(
      SECTION_SHIPPING,
      NAME_FULL,
      gfx::NativeView(),
      gfx::Rect(),
      profile.GetRawInfo(NAME_FULL).substr(0, 1),
      true);
  EXPECT_EQ(NAME_FULL, controller()->popup_input_type());
}

}  // namespace autofill
