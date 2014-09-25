// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <utility>

#include "base/basictypes.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "base/tuple.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/mock_address_validator.h"
#include "chrome/browser/ui/autofill/mock_new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_controller.h"
#include "chrome/browser/webdata/web_data_service_factory.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/gaia_account.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/mock_wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
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
const char kFakeFingerprintEncoded[] = "CgVaAwiACA==";
const char kEditedBillingAddress[] = "123 edited billing address";
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
const char kSettingsOrigin[] = "Chrome settings";
const char kTestCCNumberAmex[] = "376200000000002";
const char kTestCCNumberVisa[] = "4111111111111111";
const char kTestCCNumberMaster[] = "5555555555554444";
const char kTestCCNumberDiscover[] = "6011111111111117";
const char kTestCCNumberIncomplete[] = "4111111111";
// Credit card number fails Luhn check.
const char kTestCCNumberInvalid[] = "4111111111111112";

// Copies the initial values from |inputs| into |outputs|.
void CopyInitialValues(const DetailInputs& inputs, FieldValueMap* outputs) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    (*outputs)[input.type] = input.initial_value;
  }
}

scoped_ptr<wallet::WalletItems> CompleteAndValidWalletItems() {
  scoped_ptr<wallet::WalletItems> items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  items->AddAccount(wallet::GetTestGaiaAccount());
  items->AddInstrument(wallet::GetTestMaskedInstrument());
  items->AddAddress(wallet::GetTestShippingAddress());
  return items.Pass();
}

scoped_ptr<risk::Fingerprint> GetFakeFingerprint() {
  scoped_ptr<risk::Fingerprint> fingerprint(new risk::Fingerprint());
  // Add some data to the proto, else the encoded content is empty.
  fingerprint->mutable_machine_characteristics()->mutable_screen_size()->
      set_width(1024);
  return fingerprint.Pass();
}

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
  virtual ~TestAutofillDialogView() {}

  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}

  virtual void UpdatesStarted() OVERRIDE {
    updates_started_++;
  }

  virtual void UpdatesFinished() OVERRIDE {
    updates_started_--;
    EXPECT_GE(updates_started_, 0);
  }

  virtual void UpdateNotificationArea() OVERRIDE {
    EXPECT_GE(updates_started_, 1);
  }

  virtual void UpdateAccountChooser() OVERRIDE {
    EXPECT_GE(updates_started_, 1);
  }

  virtual void UpdateButtonStrip() OVERRIDE {
    EXPECT_GE(updates_started_, 1);
  }

  virtual void UpdateOverlay() OVERRIDE {
    EXPECT_GE(updates_started_, 1);
  }

  virtual void UpdateDetailArea() OVERRIDE {
    EXPECT_GE(updates_started_, 1);
  }

  virtual void UpdateSection(DialogSection section) OVERRIDE {
    section_updates_[section]++;
    EXPECT_GE(updates_started_, 1);
  }

  virtual void UpdateErrorBubble() OVERRIDE {
    EXPECT_GE(updates_started_, 1);
  }

  virtual void FillSection(DialogSection section,
                           ServerFieldType originating_type) OVERRIDE {}
  virtual void GetUserInput(DialogSection section, FieldValueMap* output)
      OVERRIDE {
    *output = outputs_[section];
  }

  virtual base::string16 GetCvc() OVERRIDE { return base::string16(); }

  virtual bool SaveDetailsLocally() OVERRIDE {
    return save_details_locally_checked_;
  }

  virtual const content::NavigationController* ShowSignIn() OVERRIDE {
    return NULL;
  }
  virtual void HideSignIn() OVERRIDE {}

  MOCK_METHOD0(ModelChanged, void());
  MOCK_METHOD0(UpdateForErrors, void());

  virtual void OnSignInResize(const gfx::Size& pref_size) OVERRIDE {}
  virtual void ValidateSection(DialogSection) OVERRIDE {}

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
      const AutofillMetrics& metric_logger,
      const AutofillClient::ResultCallback& callback,
      MockNewCreditCardBubbleController* mock_new_card_bubble_controller)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     callback),
        metric_logger_(metric_logger),
        mock_wallet_client_(
            Profile::FromBrowserContext(contents->GetBrowserContext())
                ->GetRequestContext(),
            this,
            source_url),
        mock_new_card_bubble_controller_(mock_new_card_bubble_controller),
        submit_button_delay_count_(0) {}

  virtual ~TestAutofillDialogController() {}

  virtual AutofillDialogView* CreateView() OVERRIDE {
    return new testing::NiceMock<TestAutofillDialogView>();
  }

  void Init(content::BrowserContext* browser_context) {
    test_manager_.Init(
        WebDataServiceFactory::GetAutofillWebDataForProfile(
            Profile::FromBrowserContext(browser_context),
            Profile::EXPLICIT_ACCESS),
        user_prefs::UserPrefs::Get(browser_context),
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

  wallet::MockWalletClient* GetTestingWalletClient() {
    return &mock_wallet_client_;
  }

  const GURL& open_tab_url() { return open_tab_url_; }

  void SimulateSigninError() {
    OnWalletSigninError();
  }

  // Skips past the 2 second wait between FinishSubmit and DoFinishSubmit.
  void ForceFinishSubmit() {
    DoFinishSubmit();
  }

  void SimulateSubmitButtonDelayBegin() {
    AutofillDialogControllerImpl::SubmitButtonDelayBegin();
  }

  void SimulateSubmitButtonDelayEnd() {
    AutofillDialogControllerImpl::SubmitButtonDelayEndForTesting();
  }

  using AutofillDialogControllerImpl::
      ClearLastWalletItemsFetchTimestampForTesting;

  // Returns the number of times that the submit button was delayed.
  int get_submit_button_delay_count() const {
    return submit_button_delay_count_;
  }

  MOCK_METHOD0(LoadRiskFingerprintData, void());
  using AutofillDialogControllerImpl::AccountChooserModelForTesting;
  using AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData;
  using AutofillDialogControllerImpl::IsEditingExistingData;
  using AutofillDialogControllerImpl::IsManuallyEditingSection;
  using AutofillDialogControllerImpl::IsPayingWithWallet;
  using AutofillDialogControllerImpl::IsSubmitPausedOn;
  using AutofillDialogControllerImpl::NOT_CHECKED;
  using AutofillDialogControllerImpl::popup_input_type;
  using AutofillDialogControllerImpl::SignedInState;

 protected:
  virtual PersonalDataManager* GetManager() const OVERRIDE {
    return const_cast<TestAutofillDialogController*>(this)->
        GetTestingManager();
  }

  virtual AddressValidator* GetValidator() OVERRIDE {
    return &mock_validator_;
  }

  virtual wallet::WalletClient* GetWalletClient() OVERRIDE {
    return &mock_wallet_client_;
  }

  virtual void OpenTabWithUrl(const GURL& url) OVERRIDE {
    open_tab_url_ = url;
  }

  virtual void ShowNewCreditCardBubble(
      scoped_ptr<CreditCard> new_card,
      scoped_ptr<AutofillProfile> billing_profile) OVERRIDE {
    mock_new_card_bubble_controller_->Show(new_card.Pass(),
                                           billing_profile.Pass());
  }

  // AutofillDialogControllerImpl calls this method before showing the dialog
  // window.
  virtual void SubmitButtonDelayBegin() OVERRIDE {
    // Do not delay enabling the submit button in testing.
    submit_button_delay_count_++;
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  TestPersonalDataManager test_manager_;
  testing::NiceMock<wallet::MockWalletClient> mock_wallet_client_;

  // A mock validator object to prevent network requests and track when
  // validation rules are loaded or validation attempts occur.
  testing::NiceMock<MockAddressValidator> mock_validator_;

  GURL open_tab_url_;
  MockNewCreditCardBubbleController* mock_new_card_bubble_controller_;

  // The number of times that the submit button was delayed.
  int submit_button_delay_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogControllerTest : public ChromeRenderViewHostTestHarness {
 protected:
  AutofillDialogControllerTest(): form_structure_(NULL) {}

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();
    Reset();
  }

  virtual void TearDown() OVERRIDE {
    if (controller_)
      controller_->ViewClosed();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void Reset() {
    if (controller_)
      controller_->ViewClosed();

    test_generated_bubble_controller_ =
        new testing::NiceMock<TestGeneratedCreditCardBubbleController>(
            web_contents());
    ASSERT_TRUE(test_generated_bubble_controller_->IsInstalled());

    mock_new_card_bubble_controller_.reset(
        new MockNewCreditCardBubbleController);

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
        metric_logger_,
        callback,
        mock_new_card_bubble_controller_.get()))->AsWeakPtr();
    controller_->Init(profile());
  }

  // Creates a new controller for |form_data| and sets up some initial wallet
  // data for it.
  void SetUpControllerWithFormData(const FormData& form_data) {
    ResetControllerWithFormData(form_data);
    controller()->Show();
    if (controller() &&
        !profile()->GetPrefs()->GetBoolean(
            ::prefs::kAutofillDialogPayWithoutWallet)) {
      EXPECT_CALL(*controller()->GetTestingWalletClient(),
                  GetWalletItems(_, _));
      controller()->OnDidFetchWalletCookieValue(std::string());
      controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
    }
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

  // Fills the inputs in SECTION_CC_BILLING with valid data.
  void FillCCBillingInputs() {
    FieldValueMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
    AutofillProfile full_profile(test::GetVerifiedProfile());
    CreditCard full_card(test::GetCreditCard());
    for (size_t i = 0; i < inputs.size(); ++i) {
      const ServerFieldType type = inputs[i].type;
      outputs[type] = full_profile.GetInfo(AutofillType(type), "en-US");

      if (outputs[type].empty())
        outputs[type] = full_card.GetInfo(AutofillType(type), "en-US");
    }
    controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);
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

  void SwitchToAutofill() {
    ui::MenuModel* model = controller_->MenuModelForAccountChooser();
    model->ActivatedAt(model->GetItemCount() - 1);
  }

  void SwitchToWallet() {
    controller_->MenuModelForAccountChooser()->ActivatedAt(0);
  }

  void SimulateSigninError() {
    controller_->SimulateSigninError();
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

  void SubmitWithWalletItems(scoped_ptr<wallet::WalletItems> wallet_items) {
    controller()->OnDidGetWalletItems(wallet_items.Pass());
    AcceptAndLoadFakeFingerprint();
  }

  void AcceptAndLoadFakeFingerprint() {
    controller()->OnAccept();
    controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());
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

    EXPECT_EQ(CREDIT_CARD_NAME,
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

  TestGeneratedCreditCardBubbleController* test_generated_bubble_controller() {
    return test_generated_bubble_controller_;
  }

  const MockNewCreditCardBubbleController* mock_new_card_bubble_controller() {
    return mock_new_card_bubble_controller_.get();
  }

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

  // Must outlive the controller.
  AutofillMetrics metric_logger_;

  // Returned when the dialog closes successfully.
  const FormStructure* form_structure_;

  // Used to monitor if the Autofill credit card bubble is shown. Owned by
  // |web_contents()|.
  TestGeneratedCreditCardBubbleController* test_generated_bubble_controller_;

  // Used to record when new card bubbles would show. Created in |Reset()|.
  scoped_ptr<MockNewCreditCardBubbleController>
      mock_new_card_bubble_controller_;

  scoped_ptr<ScopedTestingLocalState> scoped_local_state_;
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
  // Construct FieldValueMap from existing data.
  SwitchToAutofill();

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
    for (size_t i = 0; i < inputs.size(); ++i) {
      const ServerFieldType type = inputs[i].type;
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
  outputs[ADDRESS_BILLING_COUNTRY] = ASCIIToUTF16("United States");
  outputs[CREDIT_CARD_EXP_MONTH] = default_month_value;
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = default_year_value;

  // Expiration default values generate unsure validation errors (but not sure).
  ValidityMessages messages = controller()->InputsAreValid(SECTION_CC_BILLING,
                                                           outputs);
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_MONTH));

  // Expiration date with default month fails.
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = other_year_value;
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_FALSE(HasUnsureError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_MONTH));

  // Expiration date with default year fails.
  outputs[CREDIT_CARD_EXP_MONTH] = other_month_value;
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = default_year_value;
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_TRUE(HasUnsureError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_FALSE(HasUnsureError(messages, CREDIT_CARD_EXP_MONTH));
}

TEST_F(AutofillDialogControllerTest, BillingNameValidation) {
  // Construct FieldValueMap from AutofillProfile data.
  SwitchToAutofill();

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

  // Switch to Wallet which only considers names with with at least two names to
  // be valid.
  SwitchToWallet();

  // Setup some wallet state.
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Input an empty billing name. Data source should not change this behavior.
  outputs[NAME_BILLING_FULL] = base::string16();
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_TRUE(HasUnsureError(messages, NAME_BILLING_FULL));

  // Input a one name billing name. Wallet does not currently support this.
  outputs[NAME_BILLING_FULL] = ASCIIToUTF16("Bob");
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_TRUE(messages.HasSureError(NAME_BILLING_FULL));

  // Input a two name billing name.
  outputs[NAME_BILLING_FULL] = ASCIIToUTF16("Bob Barker");
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_FALSE(HasAnyError(messages, NAME_BILLING_FULL));

  // Input a more than two name billing name.
  outputs[NAME_BILLING_FULL] = ASCIIToUTF16("John Jacob Jingleheimer Schmidt"),
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_FALSE(HasAnyError(messages, NAME_BILLING_FULL));

  // Input a billing name with lots of crazy whitespace.
  outputs[NAME_BILLING_FULL] =
      ASCIIToUTF16("     \\n\\r John \\n  Jacob Jingleheimer \\t Schmidt  "),
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_FALSE(HasAnyError(messages, NAME_BILLING_FULL));
}

TEST_F(AutofillDialogControllerTest, CreditCardNumberValidation) {
  // Construct FieldValueMap from AutofillProfile data.
  SwitchToAutofill();

  // Should accept AMEX, Visa, Master and Discover.
  ValidateCCNumber(SECTION_CC, kTestCCNumberVisa, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberDiscover, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberAmex, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberIncomplete, false);
  ValidateCCNumber(SECTION_CC, kTestCCNumberInvalid, false);

  // Switch to Wallet which will not accept AMEX.
  SwitchToWallet();

  // Setup some wallet state on a merchant for which Wallet doesn't
  // support AMEX.
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));

  // Should accept Visa, Master and Discover, but not AMEX.
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberVisa, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberDiscover, true);
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_AUTOFILL_CREDIT_CARD_NOT_SUPPORTED_BY_WALLET_FOR_MERCHANT),
            ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberAmex, false));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_NUMBER),
      ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberIncomplete, false));
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_NUMBER),
      ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberInvalid, false));

  // Setup some wallet state on a merchant for which Wallet supports AMEX.
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_ALLOWED));

  // Should accept Visa, Master, Discover, and AMEX.
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberVisa, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberDiscover, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberAmex, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberIncomplete, false);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberInvalid, false);
}

TEST_F(AutofillDialogControllerTest, AutofillProfiles) {
  SwitchToAutofill();
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
  SwitchToAutofill();
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
  SwitchToAutofill();
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
  SwitchToAutofill();
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
  SwitchToAutofill();
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
  SwitchToAutofill();

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

  // Reload the dialog. The newly added address and credit card should now be
  // set as the defaults.
  Reset();
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  controller()->GetTestingManager()->AddTestingProfile(&new_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  controller()->GetTestingManager()->AddTestingCreditCard(&new_credit_card);

  // Until a selection has been made, the default suggestion is the first one.
  // For the shipping section, this follows the "use billing" suggestion.
  EXPECT_EQ(1, GetMenuModelForSection(SECTION_CC)->checked_item());
  EXPECT_EQ(2, GetMenuModelForSection(SECTION_SHIPPING)->checked_item());
}

TEST_F(AutofillDialogControllerTest, AutofillProfileVariants) {
  SwitchToAutofill();
  EXPECT_CALL(*controller()->GetView(), ModelChanged());
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(!!shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());

  // Set up some variant data.
  AutofillProfile full_profile(test::GetVerifiedProfile());
  std::vector<base::string16> names;
  names.push_back(ASCIIToUTF16("John Doe"));
  names.push_back(ASCIIToUTF16("Jane Doe"));
  full_profile.SetRawMultiInfo(NAME_FULL, names);
  std::vector<base::string16> emails;
  emails.push_back(ASCIIToUTF16(kFakeEmail));
  emails.push_back(ASCIIToUTF16("admin@example.com"));
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, emails);

  // Non-default variants are ignored by the dialog.
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  EXPECT_EQ(4, shipping_model->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, SuggestValidEmail) {
  SwitchToAutofill();
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
  SwitchToAutofill();
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(".!#$%&'*+/=?^_`-@-.."));
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
  // "add", "manage", 1 suggestion, and "same as billing".
  EXPECT_EQ(
      4, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, SuggestValidAddress) {
  SwitchToAutofill();
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  // "add", "manage", and 1 suggestion.
  EXPECT_EQ(
      3, controller()->MenuModelForSection(SECTION_BILLING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestInvalidAddress) {
  SwitchToAutofill();
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("C"));
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestIncompleteAddress) {
  SwitchToAutofill();
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  // Same as shipping, manage, add new.
  EXPECT_EQ(3,
      controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
}

TEST_F(AutofillDialogControllerTest, DoSuggestShippingAddressWithoutEmail) {
  SwitchToAutofill();
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
  SwitchToAutofill();
  // Since the PersonalDataManager is empty, this should only have the
  // default menu items.
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(3);

  // Empty cards are ignored.
  CreditCard empty_card(base::GenerateGUID(), kSettingsOrigin);
  empty_card.SetRawInfo(CREDIT_CARD_NAME, ASCIIToUTF16("John Doe"));
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
  SwitchToAutofill();
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

  EXPECT_EQ(CREDIT_CARD_NAME,
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
  SwitchToAutofill();

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
  SwitchToAutofill();

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

  SwitchToAutofill();

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

  SwitchToAutofill();

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
  EXPECT_TRUE(StartsWith(form_structure()->field(0)->value,
                         shipping_profile.GetRawInfo(ADDRESS_HOME_LINE1),
                         true));
  EXPECT_TRUE(EndsWith(form_structure()->field(0)->value,
                       shipping_profile.GetRawInfo(ADDRESS_HOME_LINE2),
                       true));
  EXPECT_TRUE(StartsWith(form_structure()->field(1)->value,
                         billing_profile.GetRawInfo(ADDRESS_HOME_LINE1),
                         true));
  EXPECT_TRUE(EndsWith(form_structure()->field(1)->value,
                       billing_profile.GetRawInfo(ADDRESS_HOME_LINE2),
                       true));
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
  SwitchToAutofill();

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

TEST_F(AutofillDialogControllerTest, AcceptLegalDocuments) {
  for (size_t i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "Case " << i);

    EXPECT_CALL(*controller()->GetTestingWalletClient(),
                AcceptLegalDocuments(_, _));
    EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));
    EXPECT_CALL(*controller(), LoadRiskFingerprintData());

    EXPECT_TRUE(controller()->LegalDocumentLinks().empty());
    controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
    EXPECT_TRUE(controller()->LegalDocumentLinks().empty());

    scoped_ptr<wallet::WalletItems> wallet_items =
        CompleteAndValidWalletItems();
    wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
    wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
    controller()->OnDidGetWalletItems(wallet_items.Pass());
    EXPECT_FALSE(controller()->LegalDocumentLinks().empty());

    controller()->OnAccept();
    controller()->OnDidAcceptLegalDocuments();
    controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());

    // Now try it all over again with the location disclosure already accepted.
    // Nothing should change.
    Reset();
    base::ListValue preexisting_list;
    preexisting_list.AppendString(kFakeEmail);
    g_browser_process->local_state()->Set(
        ::prefs::kAutofillDialogWalletLocationAcceptance,
        preexisting_list);
  }
}

TEST_F(AutofillDialogControllerTest, RejectLegalDocuments) {
  for (size_t i = 0; i < 2; ++i) {
    SCOPED_TRACE(testing::Message() << "Case " << i);

    EXPECT_CALL(*controller()->GetTestingWalletClient(),
                AcceptLegalDocuments(_, _)).Times(0);

    scoped_ptr<wallet::WalletItems> wallet_items =
        CompleteAndValidWalletItems();
    wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
    wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
    controller()->OnDidGetWalletItems(wallet_items.Pass());
    EXPECT_FALSE(controller()->LegalDocumentLinks().empty());

    controller()->OnCancel();

    // Now try it all over again with the location disclosure already accepted.
    // Nothing should change.
    Reset();
    base::ListValue preexisting_list;
    preexisting_list.AppendString(kFakeEmail);
    g_browser_process->local_state()->Set(
        ::prefs::kAutofillDialogWalletLocationAcceptance,
        preexisting_list);
  }
}

TEST_F(AutofillDialogControllerTest, AcceptLocationDisclosure) {
  // Check that accepting the dialog registers the user's name in the list
  // of users who have accepted the geolocation terms.
  EXPECT_TRUE(g_browser_process->local_state()->GetList(
      ::prefs::kAutofillDialogWalletLocationAcceptance)->empty());

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_FALSE(controller()->LegalDocumentsText().empty());
  EXPECT_TRUE(controller()->LegalDocumentLinks().empty());
  controller()->OnAccept();

  const base::ListValue* list = g_browser_process->local_state()->GetList(
      ::prefs::kAutofillDialogWalletLocationAcceptance);
  ASSERT_EQ(1U, list->GetSize());
  std::string accepted_username;
  EXPECT_TRUE(list->GetString(0, &accepted_username));
  EXPECT_EQ(kFakeEmail, accepted_username);

  // Now check it still works if that list starts off with some other username
  // in it.
  Reset();
  list = g_browser_process->local_state()->GetList(
      ::prefs::kAutofillDialogWalletLocationAcceptance);
  ASSERT_TRUE(list->empty());

  std::string kOtherUsername("spouse@example.com");
  base::ListValue preexisting_list;
  preexisting_list.AppendString(kOtherUsername);
  g_browser_process->local_state()->Set(
      ::prefs::kAutofillDialogWalletLocationAcceptance,
      preexisting_list);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_FALSE(controller()->LegalDocumentsText().empty());
  EXPECT_TRUE(controller()->LegalDocumentLinks().empty());
  controller()->OnAccept();

  list = g_browser_process->local_state()->GetList(
      ::prefs::kAutofillDialogWalletLocationAcceptance);
  ASSERT_EQ(2U, list->GetSize());
  EXPECT_NE(list->end(), list->Find(base::StringValue(kFakeEmail)));
  EXPECT_NE(list->end(), list->Find(base::StringValue(kOtherUsername)));

  // Now check the list doesn't change if the user cancels out of the dialog.
  Reset();
  list = g_browser_process->local_state()->GetList(
      ::prefs::kAutofillDialogWalletLocationAcceptance);
  ASSERT_TRUE(list->empty());

  g_browser_process->local_state()->Set(
      ::prefs::kAutofillDialogWalletLocationAcceptance,
      preexisting_list);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_FALSE(controller()->LegalDocumentsText().empty());
  EXPECT_TRUE(controller()->LegalDocumentLinks().empty());
  controller()->OnCancel();

  list = g_browser_process->local_state()->GetList(
      ::prefs::kAutofillDialogWalletLocationAcceptance);
  ASSERT_EQ(1U, list->GetSize());
  EXPECT_NE(list->end(), list->Find(base::StringValue(kOtherUsername)));
  EXPECT_EQ(list->end(), list->Find(base::StringValue(kFakeEmail)));
}

TEST_F(AutofillDialogControllerTest, LegalDocumentOverflow) {
  for (size_t number_of_docs = 2; number_of_docs < 11; ++number_of_docs) {
    scoped_ptr<wallet::WalletItems> wallet_items =
        CompleteAndValidWalletItems();
    for (size_t i = 0; i < number_of_docs; ++i)
      wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());

    Reset();
    controller()->OnDidGetWalletItems(wallet_items.Pass());

    // The dialog is only equipped to handle 2-6 legal documents. More than
    // 6 errors out.
    if (number_of_docs <= 6U) {
      EXPECT_FALSE(controller()->LegalDocumentsText().empty());
    } else {
      EXPECT_TRUE(controller()->LegalDocumentsText().empty());
      EXPECT_EQ(1U, NotificationsOfType(
          DialogNotification::WALLET_ERROR).size());
    }
  }

  controller()->OnCancel();
}

// Makes sure the default object IDs are respected.
TEST_F(AutofillDialogControllerTest, WalletDefaultItems) {
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());

  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // "add", "manage", and 4 suggestions.
  EXPECT_EQ(6,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(2));
  ASSERT_FALSE(controller()->IsEditingExistingData(SECTION_CC_BILLING));
  // "use billing", "add", "manage", and 5 suggestions.
  EXPECT_EQ(8,
      controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_SHIPPING)->
      IsItemCheckedAt(4));
  ASSERT_FALSE(controller()->IsEditingExistingData(SECTION_SHIPPING));
}

// Tests that invalid and AMEX default instruments are ignored.
TEST_F(AutofillDialogControllerTest, SelectInstrument) {
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  // Tests if default instrument is invalid, then, the first valid instrument is
  // selected instead of the default instrument.
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentInvalid());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // 4 suggestions and "add", "manage".
  EXPECT_EQ(6,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(0));

  // Tests if default instrument is AMEX but Wallet doesn't support
  // AMEX on this merchant, then the first valid instrument is
  // selected instead of the default instrument.
  wallet_items = wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(
      wallet::GetTestMaskedInstrumentAmex(wallet::AMEX_DISALLOWED));
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // 4 suggestions and "add", "manage".
  EXPECT_EQ(6,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(0));

  // Tests if default instrument is AMEX and it is allowed on this merchant,
  // then it is selected.
  wallet_items = wallet::GetTestWalletItems(wallet::AMEX_ALLOWED);
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(
      wallet::GetTestMaskedInstrumentAmex(wallet::AMEX_ALLOWED));
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // 4 suggestions and "add", "manage".
  EXPECT_EQ(6,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(2));

  // Tests if only have AMEX and invalid instrument, then "add" is selected.
  wallet_items = wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentInvalid());
  wallet_items->AddInstrument(
      wallet::GetTestMaskedInstrumentAmex(wallet::AMEX_DISALLOWED));

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // 2 suggestions and "add", "manage".
  EXPECT_EQ(4,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  // "add"
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(2));
}

TEST_F(AutofillDialogControllerTest, SaveAddress) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged());
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::IsNull(),
                               testing::NotNull(),
                               testing::IsNull(),
                               testing::IsNull()));

  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // If there is no shipping address in wallet, it will default to
  // "same-as-billing" instead of "add-new-item". "same-as-billing" is covered
  // by the following tests. The penultimate item in the menu is "add-new-item".
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  shipping_model->ActivatedAt(shipping_model->GetItemCount() - 2);

  AutofillProfile test_profile(test::GetVerifiedProfile());
  FillInputs(SECTION_SHIPPING, test_profile);

  AcceptAndLoadFakeFingerprint();
}

TEST_F(AutofillDialogControllerTest, SaveInstrument) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged());
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               testing::IsNull(),
                               testing::IsNull(),
                               testing::IsNull()));

  FillCCBillingInputs();
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  SubmitWithWalletItems(wallet_items.Pass());
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentWithInvalidInstruments) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged());
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               testing::IsNull(),
                               testing::IsNull(),
                               testing::IsNull()));

  FillCCBillingInputs();
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentInvalid());
  SubmitWithWalletItems(wallet_items.Pass());
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentAndAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               testing::NotNull(),
                               testing::IsNull(),
                               testing::IsNull()));

  FillCCBillingInputs();
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  SubmitWithWalletItems(wallet_items.Pass());
}

MATCHER(IsUpdatingExistingData, "updating existing Wallet data") {
  return !arg->object_id().empty();
}

MATCHER(UsesLocalBillingAddress, "uses the local billing address") {
  return arg->street_address()[0] == ASCIIToUTF16(kEditedBillingAddress);
}

// Tests that when using billing address for shipping, and there is no exact
// matched shipping address, then a shipping address should be added.
TEST_F(AutofillDialogControllerTest, BillingForShipping) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::IsNull(),
                               testing::NotNull(),
                               testing::IsNull(),
                               testing::IsNull()));

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  // Select "Same as billing" in the address menu.
  UseBillingForShipping();

  AcceptAndLoadFakeFingerprint();
}

// Tests that when using billing address for shipping, and there is an exact
// matched shipping address, then a shipping address should not be added.
TEST_F(AutofillDialogControllerTest, BillingForShippingHasMatch) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(_, _, _, _)).Times(0);

  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  scoped_ptr<wallet::WalletItems::MaskedInstrument> instrument =
      wallet::GetTestMaskedInstrument();
  // Copy billing address as shipping address, and assign an id to it.
  scoped_ptr<wallet::Address> shipping_address(
      new wallet::Address(instrument->address()));
  shipping_address->set_object_id("shipping_address_id");
  wallet_items->AddAddress(shipping_address.Pass());
  wallet_items->AddInstrument(instrument.Pass());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // Select "Same as billing" in the address menu.
  UseBillingForShipping();

  AcceptAndLoadFakeFingerprint();
}

// Test that the local view contents is used when saving a new instrument and
// the user has selected "Same as billing".
TEST_F(AutofillDialogControllerTest, SaveInstrumentSameAsBilling) {
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_CC_BILLING);
  model->ActivatedAt(model->GetItemCount() - 2);

  FieldValueMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  AutofillProfile full_profile(test::GetVerifiedProfile());
  CreditCard full_card(test::GetCreditCard());
  for (size_t i = 0; i < inputs.size(); ++i) {
    const ServerFieldType type = inputs[i].type;
    if (type == ADDRESS_BILLING_STREET_ADDRESS)
      outputs[type] = ASCIIToUTF16(kEditedBillingAddress);
    else
      outputs[type] = full_profile.GetInfo(AutofillType(type), "en-US");

    if (outputs[type].empty())
      outputs[type] = full_card.GetInfo(AutofillType(type), "en-US");
  }
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);

  controller()->OnAccept();

  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               UsesLocalBillingAddress(),
                               testing::IsNull(),
                               testing::IsNull()));
  AcceptAndLoadFakeFingerprint();
}

TEST_F(AutofillDialogControllerTest, CancelNoSave) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(_, _, _, _)).Times(0);

  EXPECT_CALL(*controller()->GetView(), ModelChanged());

  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));
  controller()->OnCancel();
}

// Checks that clicking the Manage menu item opens a new tab with a different
// URL for Wallet and Autofill.
TEST_F(AutofillDialogControllerTest, ManageItem) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  SwitchToAutofill();

  SuggestionsMenuModel* shipping = GetMenuModelForSection(SECTION_SHIPPING);
  shipping->ExecuteCommand(shipping->GetItemCount() - 1, 0);
  GURL autofill_manage_url = controller()->open_tab_url();
  EXPECT_EQ("chrome", autofill_manage_url.scheme());

  SwitchToWallet();
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  controller()->SuggestionItemSelected(shipping, shipping->GetItemCount() - 1);
  GURL wallet_manage_addresses_url = controller()->open_tab_url();
  EXPECT_EQ("https", wallet_manage_addresses_url.scheme());

  SuggestionsMenuModel* billing = GetMenuModelForSection(SECTION_CC_BILLING);
  controller()->SuggestionItemSelected(billing, billing->GetItemCount() - 1);
  GURL wallet_manage_instruments_url = controller()->open_tab_url();
  EXPECT_EQ("https", wallet_manage_instruments_url.scheme());

  EXPECT_NE(autofill_manage_url, wallet_manage_instruments_url);
  EXPECT_NE(wallet_manage_instruments_url, wallet_manage_addresses_url);
}

// Tests that adding an autofill profile and then submitting works.
TEST_F(AutofillDialogControllerTest, AddAutofillProfile) {
  SwitchToAutofill();
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

TEST_F(AutofillDialogControllerTest, VerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AuthenticateInstrument(_, _));

  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_TRUE(NotificationsOfType(DialogNotification::REQUIRED_ACTION).empty());
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SuggestionState suggestion_state =
      controller()->SuggestionStateForSection(SECTION_CC_BILLING);
  EXPECT_TRUE(suggestion_state.extra_text.empty());

  controller()->OnDidGetFullWallet(
      wallet::GetTestFullWalletWithRequiredActions(
          std::vector<wallet::RequiredAction>(1, wallet::VERIFY_CVV)));
  ASSERT_TRUE(controller()->IsSubmitPausedOn(wallet::VERIFY_CVV));

  EXPECT_FALSE(
      NotificationsOfType(DialogNotification::REQUIRED_ACTION).empty());
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC_BILLING));

  suggestion_state =
      controller()->SuggestionStateForSection(SECTION_CC_BILLING);
  EXPECT_FALSE(suggestion_state.extra_text.empty());
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC_BILLING));

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  controller()->OnAccept();

  EXPECT_FALSE(controller()->GetDialogOverlay().image.IsEmpty());
}

TEST_F(AutofillDialogControllerTest, ErrorDuringSubmit) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));

  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(AutofillDialogControllerTest, ErrorDuringVerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));

  SubmitWithWalletItems(CompleteAndValidWalletItems());
  controller()->OnDidGetFullWallet(
      wallet::GetTestFullWalletWithRequiredActions(
          std::vector<wallet::RequiredAction>(1, wallet::VERIFY_CVV)));

  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// Simulates receiving an INVALID_FORM_FIELD required action while processing a
// |WalletClientDelegate::OnDid{Save,Update}*()| call. This can happen if Online
// Wallet's server validation differs from Chrome's local validation.
TEST_F(AutofillDialogControllerTest, WalletServerSideValidation) {
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();

  std::vector<wallet::RequiredAction> required_actions;
  required_actions.push_back(wallet::INVALID_FORM_FIELD);

  std::vector<wallet::FormFieldError> form_errors;
  form_errors.push_back(
      wallet::FormFieldError(wallet::FormFieldError::INVALID_POSTAL_CODE,
                             wallet::FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(*controller()->GetView(), UpdateForErrors());
  controller()->OnDidSaveToWallet(std::string(),
                                  std::string(),
                                  required_actions,
                                  form_errors);
}

// Simulates receiving unrecoverable Wallet server validation errors.
TEST_F(AutofillDialogControllerTest, WalletServerSideValidationUnrecoverable) {
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();

  std::vector<wallet::RequiredAction> required_actions;
  required_actions.push_back(wallet::INVALID_FORM_FIELD);

  std::vector<wallet::FormFieldError> form_errors;
  form_errors.push_back(
      wallet::FormFieldError(wallet::FormFieldError::UNKNOWN_ERROR,
                             wallet::FormFieldError::UNKNOWN_LOCATION));

  controller()->OnDidSaveToWallet(std::string(),
                                  std::string(),
                                  required_actions,
                                  form_errors);

  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
}

// Test Wallet banners are show in the right situations. These banners promote
// saving details into Wallet (i.e. "[x] Save details to Wallet").
TEST_F(AutofillDialogControllerTest, WalletBanners) {
  // Simulate non-signed-in case.
  SetUpControllerWithFormData(DefaultFormData());
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  controller()->OnPassiveSigninFailure(error);
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // Sign in a user with a completed account.
  SetUpControllerWithFormData(DefaultFormData());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  // Full account; should show "Details from Wallet" message.
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());
  SwitchToAutofill();
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // Start over and sign in a user with an incomplete account.
  SetUpControllerWithFormData(DefaultFormData());
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Partial account.
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  SwitchToAutofill();
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // A Wallet error should kill any Wallet promos.
  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_ERROR).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());
}

TEST_F(AutofillDialogControllerTest, ViewCancelDoesntSetPref) {
  ASSERT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));

  SwitchToAutofill();

  controller()->OnCancel();
  controller()->ViewClosed();

  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
}

TEST_F(AutofillDialogControllerTest, SubmitWithSigninErrorDoesntSetPref) {
  ASSERT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));

  SimulateSigninError();
  FillCreditCardInputs();
  controller()->OnAccept();

  EXPECT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
}

// Tests that there's an overlay shown while waiting for full wallet items.
TEST_F(AutofillDialogControllerTest, WalletFirstRun) {
  EXPECT_TRUE(controller()->GetDialogOverlay().image.IsEmpty());

  SubmitWithWalletItems(CompleteAndValidWalletItems());
  EXPECT_FALSE(controller()->GetDialogOverlay().image.IsEmpty());

  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  EXPECT_FALSE(controller()->GetDialogOverlay().image.IsEmpty());
  EXPECT_FALSE(form_structure());

  // Don't make the test wait for 2 seconds.
  controller()->ForceFinishSubmit();
  EXPECT_TRUE(form_structure());
}

TEST_F(AutofillDialogControllerTest, ViewSubmitSetsPref) {
  ASSERT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));

  SwitchToAutofill();
  FillCreditCardInputs();
  controller()->OnAccept();

  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet));

  // Try again with a signin error (just leaves the pref alone).
  SetUpControllerWithFormData(DefaultFormData());

  // Setting up the controller again should not change the pref.
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet));

  SimulateSigninError();
  FillCreditCardInputs();
  controller()->OnAccept();
  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet));

  // Successfully choosing wallet does set the pref.
  // Note that OnDidGetWalletItems sets the account chooser to wallet mode.
  SetUpControllerWithFormData(DefaultFormData());

  controller()->OnDidFetchWalletCookieValue(std::string());
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();

  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet));
}

TEST_F(AutofillDialogControllerTest, HideWalletEmail) {
  SwitchToAutofill();

  // Email field should be showing when using Autofill.
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_BILLING));
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_TRUE(SectionContainsField(SECTION_BILLING, EMAIL_ADDRESS));

  SwitchToWallet();

  // Reset the wallet state.
  controller()->OnDidGetWalletItems(scoped_ptr<wallet::WalletItems>());

  // Setup some wallet state, submit, and get a full wallet to end the flow.
  scoped_ptr<wallet::WalletItems> wallet_items = CompleteAndValidWalletItems();

  // Filling |form_structure()| depends on the current username and wallet items
  // being fetched. Until both of these have occurred, the user should not be
  // able to click Submit if using Wallet. The username fetch happened earlier.
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Email field should be absent when using Wallet.
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_BILLING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_FALSE(SectionContainsField(SECTION_CC_BILLING, EMAIL_ADDRESS));

  controller()->OnAccept();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();

  ASSERT_TRUE(form_structure());
  size_t i = 0;
  for (; i < form_structure()->field_count(); ++i) {
    if (form_structure()->field(i)->Type().GetStorableType() == EMAIL_ADDRESS) {
      EXPECT_EQ(ASCIIToUTF16(kFakeEmail), form_structure()->field(i)->value);
      break;
    }
  }
  EXPECT_LT(i, form_structure()->field_count());
}

// Test if autofill types of returned form structure are correct for billing
// entries.
TEST_F(AutofillDialogControllerTest, AutofillTypes) {
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  controller()->OnAccept();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();
  ASSERT_TRUE(form_structure());
  ASSERT_EQ(20U, form_structure()->field_count());
  EXPECT_EQ(EMAIL_ADDRESS,
            form_structure()->field(0)->Type().GetStorableType());
  EXPECT_EQ(CREDIT_CARD_NUMBER,
            form_structure()->field(2)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure()->field(9)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_BILLING, form_structure()->field(9)->Type().group());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure()->field(16)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME, form_structure()->field(16)->Type().group());
}

TEST_F(AutofillDialogControllerTest, SaveDetailsInChrome) {
  SwitchToAutofill();
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
  SwitchToAutofill();
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
      SECTION_BILLING,
      NAME_BILLING_FULL,
      gfx::NativeView(),
      gfx::Rect(),
      verified_profile.GetInfo(AutofillType(NAME_FULL), "en-US").substr(0, 1),
      true));
  EXPECT_EQ(UNKNOWN_TYPE, controller()->popup_input_type());
}

// Tests that user is prompted when using instrument with minimal address.
TEST_F(AutofillDialogControllerTest, UpgradeMinimalAddress) {
  // A minimal address being selected should trigger error validation in the
  // view. Called once for each incomplete suggestion.
  EXPECT_CALL(*controller()->GetView(), UpdateForErrors());

  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentWithIdAndAddress(
      "id", wallet::GetTestMinimalAddress()));
  scoped_ptr<wallet::Address> address(wallet::GetTestShippingAddress());
  address->set_is_complete_address(false);
  wallet_items->AddAddress(address.Pass());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Assert that dialog's SECTION_CC_BILLING section is in edit mode.
  ASSERT_TRUE(controller()->IsEditingExistingData(SECTION_CC_BILLING));
  // Shipping section should be in edit mode because of
  // is_minimal_shipping_address.
  ASSERT_TRUE(controller()->IsEditingExistingData(SECTION_SHIPPING));
}

TEST_F(AutofillDialogControllerTest, RiskNeverLoadsWithPendingLegalDocuments) {
  EXPECT_CALL(*controller(), LoadRiskFingerprintData()).Times(0);

  scoped_ptr<wallet::WalletItems> wallet_items = CompleteAndValidWalletItems();
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
}

TEST_F(AutofillDialogControllerTest, RiskLoadsAfterAcceptingLegalDocuments) {
  EXPECT_CALL(*controller(), LoadRiskFingerprintData()).Times(0);

  scoped_ptr<wallet::WalletItems> wallet_items = CompleteAndValidWalletItems();
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  testing::Mock::VerifyAndClear(controller());
  EXPECT_CALL(*controller(), LoadRiskFingerprintData());

  controller()->OnAccept();

  // Simulate a risk load and verify |GetRiskData()| matches the encoded value.
  controller()->OnDidAcceptLegalDocuments();
  controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());
  EXPECT_EQ(kFakeFingerprintEncoded, controller()->GetRiskData());
}

TEST_F(AutofillDialogControllerTest, NoManageMenuItemForNewWalletUsers) {
  // Make sure the menu model item is created for a returning Wallet user.
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING));
  // "Same as billing", "123 address", "Add address...", and "Manage addresses".
  EXPECT_EQ(
      4, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());

  // Make sure the menu model item is not created for new Wallet users.
  base::DictionaryValue dict;
  scoped_ptr<base::ListValue> required_actions(new base::ListValue);
  required_actions->AppendString("setup_wallet");
  dict.Set("required_action", required_actions.release());
  controller()->OnDidGetWalletItems(
      wallet::WalletItems::CreateWalletItems(dict).Pass());

  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC_BILLING));
  // "Same as billing" and "Add address...".
  EXPECT_EQ(
      2, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
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

  SwitchToAutofill();

  EXPECT_FALSE(controller()->SectionIsActive(SECTION_SHIPPING));

  FillCreditCardInputs();
  controller()->OnAccept();
  EXPECT_TRUE(form_structure());
}

TEST_F(AutofillDialogControllerTest, ShippingSectionCanBeHiddenForWallet) {
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

  SetUpControllerWithFormData(form_data);
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_SHIPPING));
  EXPECT_FALSE(controller()->IsShippingAddressRequired());

  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetFullWallet(_));
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  SubmitWithWalletItems(wallet_items.Pass());
  controller()->OnDidGetFullWallet(wallet::GetTestFullWalletInstrumentOnly());
  controller()->ForceFinishSubmit();
  EXPECT_TRUE(form_structure());
}

TEST_F(AutofillDialogControllerTest, NotProdNotification) {
  // To make IsPayingWithWallet() true.
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  ASSERT_EQ(
      "",
      command_line->GetSwitchValueASCII(switches::kWalletServiceUseSandbox));

  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "1");
  EXPECT_EQ(1U,
            NotificationsOfType(DialogNotification::DEVELOPER_WARNING).size());
}

TEST_F(AutofillDialogControllerTest, NoNotProdNotification) {
  // To make IsPayingWithWallet() true.
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  ASSERT_EQ(
      "",
      command_line->GetSwitchValueASCII(switches::kWalletServiceUseSandbox));

  command_line->AppendSwitchASCII(switches::kWalletServiceUseSandbox, "0");
  EXPECT_EQ(0U,
            NotificationsOfType(DialogNotification::DEVELOPER_WARNING).size());
}

// Ensure Wallet instruments marked expired by the server are shown as invalid.
TEST_F(AutofillDialogControllerTest, WalletExpiredCard) {
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentExpired());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  EXPECT_TRUE(controller()->IsEditingExistingData(SECTION_CC_BILLING));

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  FieldValueMap outputs;
  CopyInitialValues(inputs, &outputs);

  // The local inputs are invalid because the server said so. They'll
  // stay invalid until they differ from the remotely fetched model.
  ValidityMessages messages = controller()->InputsAreValid(SECTION_CC_BILLING,
                                                           outputs);
  EXPECT_TRUE(messages.HasSureError(CREDIT_CARD_EXP_MONTH));
  EXPECT_TRUE(messages.HasSureError(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input year differ from the instrument.
  CopyInitialValues(inputs, &outputs);
  outputs[CREDIT_CARD_EXP_4_DIGIT_YEAR] = ASCIIToUTF16("3002");
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_FALSE(HasAnyError(messages, CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(HasAnyError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input month differ from the instrument.
  CopyInitialValues(inputs, &outputs);
  outputs[CREDIT_CARD_EXP_MONTH] = ASCIIToUTF16("06");
  messages = controller()->InputsAreValid(SECTION_CC_BILLING, outputs);
  EXPECT_FALSE(HasAnyError(messages, CREDIT_CARD_EXP_MONTH));
  EXPECT_FALSE(HasAnyError(messages, CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST_F(AutofillDialogControllerTest, ChooseAnotherInstrumentOrAddress) {
  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->OnDidGetFullWallet(
      wallet::GetTestFullWalletWithRequiredActions(
          std::vector<wallet::RequiredAction>(
              1, wallet::CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS)));
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  controller()->OnAccept();
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
}

TEST_F(AutofillDialogControllerTest, NewCardBubbleShown) {
  SwitchToAutofill();
  FillCreditCardInputs();
  controller()->OnAccept();
  controller()->ViewClosed();

  EXPECT_EQ(1, mock_new_card_bubble_controller()->bubbles_shown());
  EXPECT_EQ(0, test_generated_bubble_controller()->bubbles_shown());
}

TEST_F(AutofillDialogControllerTest, GeneratedCardBubbleShown) {
  SubmitWithWalletItems(CompleteAndValidWalletItems());
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();
  controller()->ViewClosed();

  EXPECT_EQ(0, mock_new_card_bubble_controller()->bubbles_shown());
  EXPECT_EQ(1, test_generated_bubble_controller()->bubbles_shown());
}

// Verify that new Wallet data is fetched when the user switches away from the
// tab hosting the Autofill dialog and back. Also verify that the user's
// selection is preserved across this re-fetch.
TEST_F(AutofillDialogControllerTest, ReloadWalletItemsOnActivation) {
  // Initialize some Wallet data.
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Initially, the default entries should be selected.
  ui::MenuModel* cc_billing_model =
      controller()->MenuModelForSection(SECTION_CC_BILLING);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  // "add", "manage", and 2 suggestions.
  ASSERT_EQ(4, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(0));
  // "use billing", "add", "manage", and 2 suggestions.
  ASSERT_EQ(5, shipping_model->GetItemCount());
  EXPECT_TRUE(shipping_model->IsItemCheckedAt(2));

  // Select entries other than the defaults.
  cc_billing_model->ActivatedAt(1);
  shipping_model->ActivatedAt(1);
  // 2 suggestions, "add", and "manage".
  ASSERT_EQ(4, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(1));
  // "use billing", 2 suggestions, "add", "manage".
  ASSERT_EQ(5, shipping_model->GetItemCount());
  EXPECT_TRUE(shipping_model-> IsItemCheckedAt(1));

  // Simulate switching away from the tab and back.  This should issue a request
  // for wallet items.
  controller()->ClearLastWalletItemsFetchTimestampForTesting();
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->TabActivated();

  // Simulate a response that includes different items.
  wallet_items = wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentExpired());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // The previously selected entries should still be selected.
  // 3 suggestions, "add", and "manage".
  ASSERT_EQ(5, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(2));
  // "use billing", 1 suggestion, "add", and "manage".
  ASSERT_EQ(4, shipping_model->GetItemCount());
  EXPECT_TRUE(shipping_model->IsItemCheckedAt(1));
}

// Verify that if the default values change when re-fetching Wallet data, these
// new default values are selected in the dialog.
TEST_F(AutofillDialogControllerTest,
       ReloadWalletItemsOnActivationWithNewDefaults) {
  // Initialize some Wallet data.
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Initially, the default entries should be selected.
  ui::MenuModel* cc_billing_model =
      controller()->MenuModelForSection(SECTION_CC_BILLING);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  // 2 suggestions, "add", and "manage".
  ASSERT_EQ(4, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(0));
  // "use billing", 2 suggestions, "add", and "manage".
  ASSERT_EQ(5, shipping_model->GetItemCount());
  EXPECT_TRUE(shipping_model->IsItemCheckedAt(2));

  // Simulate switching away from the tab and back.  This should issue a request
  // for wallet items.
  controller()->ClearLastWalletItemsFetchTimestampForTesting();
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->TabActivated();

  // Simulate a response that includes different default values.
  wallet_items =
      wallet::GetTestWalletItemsWithDefaultIds("new_default_instrument_id",
                                               "new_default_address_id",
                                               wallet::AMEX_DISALLOWED);
  scoped_ptr<wallet::Address> other_address = wallet::GetTestShippingAddress();
  other_address->set_object_id("other_address_id");
  scoped_ptr<wallet::Address> new_default_address =
      wallet::GetTestNonDefaultShippingAddress();
  new_default_address->set_object_id("new_default_address_id");

  wallet_items->AddInstrument(
      wallet::GetTestMaskedInstrumentWithId("other_instrument_id"));
  wallet_items->AddInstrument(
      wallet::GetTestMaskedInstrumentWithId("new_default_instrument_id"));
  wallet_items->AddAddress(new_default_address.Pass());
  wallet_items->AddAddress(other_address.Pass());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // The new default entries should be selected.
  // 2 suggestions, "add", and "manage".
  ASSERT_EQ(4, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(1));
  // "use billing", 2 suggestions, "add", and "manage".
  ASSERT_EQ(5, shipping_model->GetItemCount());
  EXPECT_TRUE(shipping_model->IsItemCheckedAt(1));
}

TEST_F(AutofillDialogControllerTest, ReloadWithEmptyWalletItems) {
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  controller()->MenuModelForSection(SECTION_CC_BILLING)->ActivatedAt(1);
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);

  controller()->ClearLastWalletItemsFetchTimestampForTesting();
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->TabActivated();

  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED));

  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC_BILLING));
  EXPECT_EQ(
      3, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, SaveInChromeByDefault) {
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
  SwitchToAutofill();
  FillCreditCardInputs();
  controller()->OnAccept();
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
}

TEST_F(AutofillDialogControllerTest,
       SaveInChromePreferenceNotRememberedOnCancel) {
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
  SwitchToAutofill();
  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(false);
  controller()->OnCancel();
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
}

TEST_F(AutofillDialogControllerTest,
       SaveInChromePreferenceRememberedOnSuccess) {
  EXPECT_TRUE(controller()->ShouldSaveInChrome());
  SwitchToAutofill();
  FillCreditCardInputs();
  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(false);
  controller()->OnAccept();
  EXPECT_FALSE(controller()->ShouldSaveInChrome());
}

TEST_F(AutofillDialogControllerTest,
       SubmitButtonIsDisabled_SpinnerFinishesBeforeDelay) {
  // Reset Wallet state.
  controller()->OnDidGetWalletItems(scoped_ptr<wallet::WalletItems>());

  EXPECT_EQ(1, controller()->get_submit_button_delay_count());

  // Begin the submit button delay.
  controller()->SimulateSubmitButtonDelayBegin();

  EXPECT_TRUE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Stop the spinner.
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  EXPECT_FALSE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // End the submit button delay.
  controller()->SimulateSubmitButtonDelayEnd();

  EXPECT_FALSE(controller()->ShouldShowSpinner());
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

TEST_F(AutofillDialogControllerTest,
       SubmitButtonIsDisabled_SpinnerFinishesAfterDelay) {
  // Reset Wallet state.
  controller()->OnDidGetWalletItems(scoped_ptr<wallet::WalletItems>());

  EXPECT_EQ(1, controller()->get_submit_button_delay_count());

  // Begin the submit button delay.
  controller()->SimulateSubmitButtonDelayBegin();

  EXPECT_TRUE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // End the submit button delay.
  controller()->SimulateSubmitButtonDelayEnd();

  EXPECT_TRUE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Stop the spinner.
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  EXPECT_FALSE(controller()->ShouldShowSpinner());
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

TEST_F(AutofillDialogControllerTest, SubmitButtonIsDisabled_NoSpinner) {
  SwitchToAutofill();

  EXPECT_EQ(1, controller()->get_submit_button_delay_count());

  // Begin the submit button delay.
  controller()->SimulateSubmitButtonDelayBegin();

  EXPECT_FALSE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // End the submit button delay.
  controller()->SimulateSubmitButtonDelayEnd();

  EXPECT_FALSE(controller()->ShouldShowSpinner());
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

TEST_F(AutofillDialogControllerTest, SaveCreditCardIncludesName_NoBilling) {
  SwitchToAutofill();

  CreditCard test_credit_card(test::GetVerifiedCreditCard());
  FillInputs(SECTION_CC, test_credit_card);

  AutofillProfile test_profile(test::GetVerifiedProfile());
  FillInputs(SECTION_BILLING, test_profile);

  UseBillingForShipping();

  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(true);
  controller()->OnAccept();

  TestPersonalDataManager* test_pdm = controller()->GetTestingManager();
  const CreditCard& imported_card = test_pdm->imported_credit_card();
  EXPECT_EQ(test_profile.GetInfo(AutofillType(NAME_FULL), "en-US"),
            imported_card.GetRawInfo(CREDIT_CARD_NAME));
}

TEST_F(AutofillDialogControllerTest, SaveCreditCardIncludesName_WithBilling) {
  SwitchToAutofill();

  TestPersonalDataManager* test_pdm = controller()->GetTestingManager();
  AutofillProfile test_profile(test::GetVerifiedProfile());

  EXPECT_CALL(*controller()->GetView(), ModelChanged());
  test_pdm->AddTestingProfile(&test_profile);
  ASSERT_TRUE(controller()->MenuModelForSection(SECTION_BILLING));

  CreditCard test_credit_card(test::GetVerifiedCreditCard());
  FillInputs(SECTION_CC, test_credit_card);

  controller()->GetView()->CheckSaveDetailsLocallyCheckbox(true);
  controller()->OnAccept();

  const CreditCard& imported_card = test_pdm->imported_credit_card();
  EXPECT_EQ(test_profile.GetInfo(AutofillType(NAME_FULL), "en-US"),
            imported_card.GetRawInfo(CREDIT_CARD_NAME));

  controller()->ViewClosed();
}

TEST_F(AutofillDialogControllerTest, InputEditability) {
  // Empty wallet items: all fields are editable.
  scoped_ptr<wallet::WalletItems> items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  controller()->OnDidGetWalletItems(items.Pass());

  DialogSection sections[] = { SECTION_CC_BILLING, SECTION_SHIPPING };
  for (size_t i = 0; i < arraysize(sections); ++i) {
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(sections[i]);
    for (size_t j = 0; j < inputs.size(); ++j) {
      EXPECT_TRUE(controller()->InputIsEditable(inputs[j], sections[i]));
    }
  }

  // Expired instrument: CC number + CVV are not editable.
  items = wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  scoped_ptr<wallet::WalletItems::MaskedInstrument> expired_instrument =
      wallet::GetTestMaskedInstrumentExpired();
  items->AddInstrument(expired_instrument.Pass());
  controller()->OnDidGetWalletItems(items.Pass());
  EXPECT_TRUE(controller()->IsEditingExistingData(SECTION_CC_BILLING));

  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  FieldValueMap outputs;
  CopyInitialValues(inputs, &outputs);
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);

  for (size_t i = 0; i < arraysize(sections); ++i) {
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(sections[i]);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (inputs[j].type == CREDIT_CARD_NUMBER ||
          inputs[j].type == CREDIT_CARD_VERIFICATION_CODE) {
        EXPECT_FALSE(controller()->InputIsEditable(inputs[j], sections[i]));
      } else {
        EXPECT_TRUE(controller()->InputIsEditable(inputs[j], sections[i]));
      }
    }
  }

  // User changes the billing address; same story.
  outputs[ADDRESS_BILLING_ZIP] = ASCIIToUTF16("77025");
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);
  for (size_t i = 0; i < arraysize(sections); ++i) {
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(sections[i]);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (inputs[j].type == CREDIT_CARD_NUMBER ||
          inputs[j].type == CREDIT_CARD_VERIFICATION_CODE) {
        EXPECT_FALSE(controller()->InputIsEditable(inputs[j], sections[i]));
      } else {
        EXPECT_TRUE(controller()->InputIsEditable(inputs[j], sections[i]));
      }
    }
  }

  // User changes a detail of the CC itself (expiration date), CVV is now
  // editable (and mandatory).
  outputs[CREDIT_CARD_EXP_MONTH] = ASCIIToUTF16("06");
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);
  for (size_t i = 0; i < arraysize(sections); ++i) {
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(sections[i]);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (inputs[j].type == CREDIT_CARD_NUMBER)
        EXPECT_FALSE(controller()->InputIsEditable(inputs[j], sections[i]));
      else
        EXPECT_TRUE(controller()->InputIsEditable(inputs[j], sections[i]));
    }
  }
}

// When the default country is something besides US, wallet is not selected
// and the account chooser shouldn't be visible.
TEST_F(AutofillDialogControllerTest, HideWalletInOtherCountries) {
  // Addresses from different countries.
  AutofillProfile us_profile(base::GenerateGUID(), kSettingsOrigin),
      es_profile(base::GenerateGUID(), kSettingsOrigin),
      es_profile2(base::GenerateGUID(), kSettingsOrigin);
  us_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("US"));
  es_profile.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("ES"));
  es_profile2.SetRawInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("ES"));

  // If US is indicated (via timezone), show Wallet.
  ResetControllerWithFormData(DefaultFormData());
  controller()->GetTestingManager()->set_timezone_country_code("US");
  controller()->Show();
  EXPECT_TRUE(
      controller()->AccountChooserModelForTesting()->WalletIsSelected());
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_TRUE(controller()->ShouldShowAccountChooser());
  EXPECT_TRUE(
      controller()->AccountChooserModelForTesting()->WalletIsSelected());

  // If US is not indicated, don't show Wallet.
  ResetControllerWithFormData(DefaultFormData());
  controller()->GetTestingManager()->set_timezone_country_code("ES");
  controller()->Show();
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_FALSE(controller()->ShouldShowAccountChooser());

  // If US is indicated (via a profile), show Wallet.
  ResetControllerWithFormData(DefaultFormData());
  controller()->GetTestingManager()->set_timezone_country_code("ES");
  controller()->GetTestingManager()->AddTestingProfile(&us_profile);
  controller()->Show();
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_TRUE(controller()->ShouldShowAccountChooser());
  EXPECT_TRUE(
      controller()->AccountChooserModelForTesting()->WalletIsSelected());

  // Make sure the profile doesn't just override the timezone.
  ResetControllerWithFormData(DefaultFormData());
  controller()->GetTestingManager()->set_timezone_country_code("US");
  controller()->GetTestingManager()->AddTestingProfile(&es_profile);
  controller()->Show();
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_TRUE(controller()->ShouldShowAccountChooser());
  EXPECT_TRUE(
      controller()->AccountChooserModelForTesting()->WalletIsSelected());

  // Only takes one US address to enable Wallet.
  ResetControllerWithFormData(DefaultFormData());
  controller()->GetTestingManager()->set_timezone_country_code("FR");
  controller()->GetTestingManager()->AddTestingProfile(&es_profile);
  controller()->GetTestingManager()->AddTestingProfile(&es_profile2);
  controller()->GetTestingManager()->AddTestingProfile(&us_profile);
  controller()->Show();
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  EXPECT_TRUE(controller()->ShouldShowAccountChooser());
  EXPECT_TRUE(
      controller()->AccountChooserModelForTesting()->WalletIsSelected());
}

TEST_F(AutofillDialogControllerTest, DontGetWalletTillNecessary) {
  // When starting on local data mode, the dialog will provide a "Use Google
  // Wallet" link.
  profile()->GetPrefs()->SetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet, true);
  ResetControllerWithFormData(DefaultFormData());
  controller()->Show();
  base::string16 use_wallet_text = controller()->SignInLinkText();
  EXPECT_EQ(TestAutofillDialogController::NOT_CHECKED,
            controller()->SignedInState());

  // When clicked, this link will ask for wallet items. If there's a signin
  // failure, the link will switch to "Sign in to use Google Wallet".
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->SignInLinkClicked();
  EXPECT_NE(TestAutofillDialogController::NOT_CHECKED,
            controller()->SignedInState());
  controller()->OnDidFetchWalletCookieValue(std::string());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  controller()->OnPassiveSigninFailure(GoogleServiceAuthError(
      GoogleServiceAuthError::CONNECTION_FAILED));
  EXPECT_NE(use_wallet_text, controller()->SignInLinkText());
}

TEST_F(AutofillDialogControllerTest, MultiAccountSwitch) {
  std::vector<std::string> users;
  users.push_back("user_1@example.com");
  users.push_back("user_2@example.com");
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItemsWithUsers(users, 0));

  // Items should be: Account 1, account 2, add account, disable wallet.
  EXPECT_EQ(4, controller()->MenuModelForAccountChooser()->GetItemCount());
  EXPECT_EQ(0U, controller()->GetTestingWalletClient()->user_index());

  // GetWalletItems should be called when the user switches accounts.
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_, _));
  controller()->MenuModelForAccountChooser()->ActivatedAt(1);
  // The wallet client should be updated to the new user index.
  EXPECT_EQ(1U, controller()->GetTestingWalletClient()->user_index());
}

TEST_F(AutofillDialogControllerTest, PassiveAuthFailure) {
  controller()->OnDidGetWalletItems(
      wallet::GetTestWalletItemsWithRequiredAction(
           wallet::PASSIVE_GAIA_AUTH));
  EXPECT_TRUE(controller()->ShouldShowSpinner());
  controller()->OnPassiveSigninFailure(GoogleServiceAuthError(
      GoogleServiceAuthError::NONE));
  EXPECT_FALSE(controller()->ShouldShowSpinner());
}

TEST_F(AutofillDialogControllerTest, WalletShippingSameAsBilling) {
  // Assert initial state.
  ASSERT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogWalletShippingSameAsBilling));

  // Verify that false pref defaults to wallet defaults.
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  ASSERT_FALSE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogWalletShippingSameAsBilling));
  EXPECT_EQ(2, GetMenuModelForSection(SECTION_SHIPPING)->checked_item());

  // Set "Same as Billing" for the shipping address and verify it sets the pref
  // and selects the appropriate menu item.
  UseBillingForShipping();
  ASSERT_EQ(0, GetMenuModelForSection(SECTION_SHIPPING)->checked_item());
  controller()->ForceFinishSubmit();
  ASSERT_TRUE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogWalletShippingSameAsBilling));

  // Getting new wallet info shouldn't disrupt the preference and menu should be
  // set accordingly.
  Reset();
  wallet_items = wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddAddress(wallet::GetTestNonDefaultShippingAddress());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogWalletShippingSameAsBilling));
  EXPECT_EQ(0, GetMenuModelForSection(SECTION_SHIPPING)->checked_item());

  // Choose a different address and ensure pref gets set to false.
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);
  controller()->ForceFinishSubmit();
  EXPECT_FALSE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogWalletShippingSameAsBilling));
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
    IDR_AUTOFILL_CC_DINERS,
    IDR_AUTOFILL_CC_DISCOVER,
    IDR_AUTOFILL_CC_GENERIC,
    IDR_AUTOFILL_CC_JCB,
    IDR_AUTOFILL_CC_MASTERCARD,
    IDR_AUTOFILL_CC_VISA,
  };
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (size_t i = 0; i < arraysize(kSupportedCardIdrs); ++i) {
    SCOPED_TRACE(base::IntToString(i));
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

  controller()->UserEditedOrActivatedInput(SECTION_CC_BILLING,
                                           ADDRESS_BILLING_COUNTRY,
                                           gfx::NativeView(),
                                           gfx::Rect(),
                                           ASCIIToUTF16("France"),
                                           true);
  updates = view->section_updates();
  EXPECT_EQ(1U, updates[SECTION_CC_BILLING]);
  EXPECT_EQ(1U, updates.size());

  SwitchToAutofill();
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
  EXPECT_CALL(*controller()->GetMockValidator(), LoadRules("US"));
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
  SwitchToAutofill();

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

TEST_F(AutofillDialogControllerTest, WalletUnsupportedCountries) {
  // Create a form data that simulates:
  //   <select autocomplete="billing country">
  //     <option value="IQ">Iraq</option>
  //     <option value="MX">Mexico</option>
  //   </select>
  // i.e. contains a mix of supported and unsupported countries.
  FormData form_data;
  FormFieldData field;
  field.autocomplete_attribute = "shipping country";
  field.option_contents.push_back(ASCIIToUTF16("Iraq"));
  field.option_values.push_back(ASCIIToUTF16("IQ"));
  field.option_contents.push_back(ASCIIToUTF16("Mexico"));
  field.option_values.push_back(ASCIIToUTF16("MX"));

  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-csc";

  form_data.fields.push_back(field);
  form_data.fields.push_back(cc_field);
  ResetControllerWithFormData(form_data);
  controller()->Show();

  ui::ComboboxModel* shipping_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_HOME_COUNTRY);
  ASSERT_EQ(2, shipping_country_model->GetItemCount());
  EXPECT_EQ(shipping_country_model->GetItemAt(0), ASCIIToUTF16("Iraq"));
  EXPECT_EQ(shipping_country_model->GetItemAt(1), ASCIIToUTF16("Mexico"));

  // Switch to Wallet, add limitations.
  SetUpControllerWithFormData(form_data);
  SwitchToWallet();
  scoped_ptr<wallet::WalletItems> wallet_items =
      wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddAllowedShippingCountry("MX");
  wallet_items->AddAllowedShippingCountry("GB");
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Only the intersection is available.
  EXPECT_TRUE(controller()->IsPayingWithWallet());
  shipping_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_HOME_COUNTRY);
  ASSERT_EQ(1, shipping_country_model->GetItemCount());
  EXPECT_EQ(shipping_country_model->GetItemAt(0), ASCIIToUTF16("Mexico"));

  // Empty intersection; Wallet's automatically disabled.
  wallet_items = wallet::GetTestWalletItems(wallet::AMEX_DISALLOWED);
  wallet_items->AddAllowedShippingCountry("CA");
  wallet_items->AddAllowedShippingCountry("GB");
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  EXPECT_FALSE(controller()->IsPayingWithWallet());

  // Since it's disabled, we revert to accepting all the countries.
  shipping_country_model =
      controller()->ComboboxModelForAutofillType(ADDRESS_HOME_COUNTRY);
  ASSERT_EQ(2, shipping_country_model->GetItemCount());
  EXPECT_EQ(shipping_country_model->GetItemAt(0), ASCIIToUTF16("Iraq"));
  EXPECT_EQ(shipping_country_model->GetItemAt(1), ASCIIToUTF16("Mexico"));
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
  SwitchToAutofill();
  // Typically, MC and Visa are both valid.
  ValidateCCNumber(SECTION_CC, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC, kTestCCNumberVisa, true);

  FormData form_data;
  FormFieldData field;
  field.autocomplete_attribute = "billing cc-type";
  field.option_contents.push_back(ASCIIToUTF16("Visa"));
  field.option_values.push_back(ASCIIToUTF16("V"));
  field.option_contents.push_back(ASCIIToUTF16("American Express"));
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

  CreditCard master_card(base::GenerateGUID(), "chrome settings");
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

  // No MC; Wallet is disabled.
  SetUpControllerWithFormData(form_data);
  EXPECT_FALSE(controller()->IsPayingWithWallet());

  // In Autofill mode, Discover is disallowed because it's not in FormData.
  ValidateCCNumber(SECTION_CC, kTestCCNumberDiscover, false);

  field.option_contents.push_back(ASCIIToUTF16("Mastercard"));
  field.option_values.push_back(ASCIIToUTF16("Mastercard"));
  form_data.fields[0] = field;

  // Add MC to FormData; Wallet is enabled.
  SetUpControllerWithFormData(form_data);
  EXPECT_TRUE(controller()->IsPayingWithWallet());
  // Even though Discover isn't in FormData, it's allowed because Wallet always
  // generates a MC Virtual card.
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberDiscover, true);
}

TEST_F(AutofillDialogControllerTest, SuggestCountrylessProfiles) {
  SwitchToAutofill();

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

TEST_F(AutofillDialogControllerTest, SwitchFromWalletWithFirstName) {
  controller()->MenuModelForSection(SECTION_CC_BILLING)->ActivatedAt(2);

  FieldValueMap outputs;
  outputs[NAME_FULL] = ASCIIToUTF16("madonna");
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);

  ASSERT_NO_FATAL_FAILURE(SwitchToAutofill());
}

// Regression test for http://crbug.com/382777
TEST_F(AutofillDialogControllerTest, WalletBillingCountry) {
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-number";
  FormFieldData billing_country, billing_country_name, shipping_country,
      shipping_country_name;
  billing_country.autocomplete_attribute = "billing country";
  billing_country_name.autocomplete_attribute = "billing country-name";
  shipping_country.autocomplete_attribute = "shipping country";
  shipping_country_name.autocomplete_attribute = "shipping country-name";

  FormData form_data;
  form_data.fields.push_back(cc_field);
  form_data.fields.push_back(billing_country);
  form_data.fields.push_back(billing_country_name);
  form_data.fields.push_back(shipping_country);
  form_data.fields.push_back(shipping_country_name);

  SetUpControllerWithFormData(form_data);
  AcceptAndLoadFakeFingerprint();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();

  ASSERT_EQ(5U, form_structure()->field_count());
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            form_structure()->field(1)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("US"), form_structure()->field(1)->value);
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            form_structure()->field(2)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("United States"), form_structure()->field(2)->value);
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            form_structure()->field(3)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("US"), form_structure()->field(3)->value);
  EXPECT_EQ(ADDRESS_HOME_COUNTRY,
            form_structure()->field(4)->Type().GetStorableType());
  EXPECT_EQ(ASCIIToUTF16("United States"), form_structure()->field(4)->value);
}

}  // namespace autofill
