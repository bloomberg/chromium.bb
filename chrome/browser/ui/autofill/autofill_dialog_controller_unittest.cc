// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/tuple.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/mock_new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/mock_wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_common_test.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/mock_render_process_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

using testing::_;

namespace autofill {

namespace {

const char kFakeEmail[] = "user@example.com";
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
      "billing locality",
      "billing region",
      "billing postal-code",
      "billing country",
      "billing tel",
      "shipping name",
      "shipping address-line1",
      "shipping locality",
      "shipping region",
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

// Sets the value of |type| in |outputs| to |value|.
void SetOutputValue(const DetailInputs& inputs,
                    ServerFieldType type,
                    const base::string16& value,
                    DetailOutputMap* outputs) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    if (input.type == type)
      (*outputs)[&input] = value;
  }
}

// Copies the initial values from |inputs| into |outputs|.
void CopyInitialValues(const DetailInputs& inputs, DetailOutputMap* outputs) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    (*outputs)[&input] = input.initial_value;
  }
}

scoped_ptr<wallet::WalletItems> CompleteAndValidWalletItems() {
  scoped_ptr<wallet::WalletItems> items = wallet::GetTestWalletItems();
  items->AddInstrument(wallet::GetTestMaskedInstrument());
  items->AddAddress(wallet::GetTestShippingAddress());
  return items.Pass();
}

scoped_ptr<wallet::FullWallet> CreateFullWallet(const char* required_action) {
  base::DictionaryValue dict;
  scoped_ptr<base::ListValue> list(new base::ListValue());
  list->AppendString(required_action);
  dict.Set("required_action", list.release());
  return wallet::FullWallet::CreateFullWallet(dict);
}

scoped_ptr<risk::Fingerprint> GetFakeFingerprint() {
  scoped_ptr<risk::Fingerprint> fingerprint(new risk::Fingerprint());
  // Add some data to the proto, else the encoded content is empty.
  fingerprint->mutable_machine_characteristics()->mutable_screen_size()->
      set_width(1024);
  return fingerprint.Pass();
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
    EXPECT_GE(updates_started_, 1);
  }

  virtual void FillSection(DialogSection section,
                           const DetailInput& originating_input) OVERRIDE {};
  virtual void GetUserInput(DialogSection section, DetailOutputMap* output)
      OVERRIDE {
    *output = outputs_[section];
  }
  virtual TestableAutofillDialogView* GetTestableView() OVERRIDE {
    return NULL;
  }

  virtual string16 GetCvc() OVERRIDE { return string16(); }
  virtual bool HitTestInput(const DetailInput& input,
                            const gfx::Point& screen_point) OVERRIDE {
    return false;
  }

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

  void SetUserInput(DialogSection section, const DetailOutputMap& map) {
    outputs_[section] = map;
  }

  void CheckSaveDetailsLocallyCheckbox(bool checked) {
    save_details_locally_checked_ = checked;
  }

 private:
  std::map<DialogSection, DetailOutputMap> outputs_;

  int updates_started_;
  bool save_details_locally_checked_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogView);
};

// Bring over command-ids from AccountChooserModel.
class TestAccountChooserModel : public AccountChooserModel {
 public:
  TestAccountChooserModel(AccountChooserModelDelegate* delegate,
                          PrefService* prefs,
                          const AutofillMetrics& metric_logger)
      : AccountChooserModel(delegate, prefs, metric_logger) {}
  virtual ~TestAccountChooserModel() {}

  using AccountChooserModel::kActiveWalletItemId;
  using AccountChooserModel::kAutofillItemId;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAccountChooserModel);
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
      const base::Callback<void(const FormStructure*)>& callback,
      MockNewCreditCardBubbleController* mock_new_card_bubble_controller)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     callback),
        metric_logger_(metric_logger),
        mock_wallet_client_(
            Profile::FromBrowserContext(contents->GetBrowserContext())->
                GetRequestContext(), this),
        mock_new_card_bubble_controller_(mock_new_card_bubble_controller),
        submit_button_delay_count_(0) {}

  virtual ~TestAutofillDialogController() {}

  virtual AutofillDialogView* CreateView() OVERRIDE {
    return new testing::NiceMock<TestAutofillDialogView>();
  }

  void Init(content::BrowserContext* browser_context) {
    test_manager_.Init(browser_context);
  }

  TestAutofillDialogView* GetView() {
    return static_cast<TestAutofillDialogView*>(view());
  }

  TestPersonalDataManager* GetTestingManager() {
    return &test_manager_;
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
#if defined(TOOLKIT_VIEWS)
    DoFinishSubmit();
#endif
  }

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
  using AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData;
  using AutofillDialogControllerImpl::IsEditingExistingData;

 protected:
  virtual PersonalDataManager* GetManager() OVERRIDE {
    return &test_manager_;
  }

  virtual wallet::WalletClient* GetWalletClient() OVERRIDE {
    return &mock_wallet_client_;
  }

  virtual void OpenTabWithUrl(const GURL& url) OVERRIDE {
    open_tab_url_ = url;
  }

  // Whether the information input in this dialog will be securely transmitted
  // to the requesting site.
  virtual bool TransmissionWillBeSecure() const OVERRIDE {
    return true;
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
  GURL open_tab_url_;
  MockNewCreditCardBubbleController* mock_new_card_bubble_controller_;

  // The number of times that the submit button was delayed.
  int submit_button_delay_count_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class TestGeneratedCreditCardBubbleController :
    public GeneratedCreditCardBubbleController {
 public:
  explicit TestGeneratedCreditCardBubbleController(
      content::WebContents* contents)
      : GeneratedCreditCardBubbleController(contents) {
    contents->SetUserData(UserDataKey(), this);
    CHECK_EQ(contents->GetUserData(UserDataKey()), this);
  }

  virtual ~TestGeneratedCreditCardBubbleController() {}

  MOCK_METHOD2(SetupAndShow, void(const base::string16& backing_card_name,
                                  const base::string16& fronting_card_name));

 protected:
  virtual base::WeakPtr<GeneratedCreditCardBubbleView> CreateBubble() OVERRIDE {
    return TestGeneratedCreditCardBubbleView::Create(GetWeakPtr());
  }

  virtual bool CanShow() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TestGeneratedCreditCardBubbleController);
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
    mock_new_card_bubble_controller_.reset(
        new MockNewCreditCardBubbleController);

    // Don't get stuck on the first run wallet interstitial.
    profile()->GetPrefs()->SetBoolean(::prefs::kAutofillDialogHasPaidWithWallet,
                                      true);
    profile()->GetPrefs()->ClearPref(::prefs::kAutofillDialogSaveData);

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

  void SetUpControllerWithFormData(const FormData& form_data) {
    if (controller_)
      controller_->ViewClosed();

    base::Callback<void(const FormStructure*)> callback =
        base::Bind(&AutofillDialogControllerTest::FinishedCallback,
                   base::Unretained(this));
    controller_ = (new testing::NiceMock<TestAutofillDialogController>(
        web_contents(),
        form_data,
        GURL(),
        metric_logger_,
        callback,
        mock_new_card_bubble_controller_.get()))->AsWeakPtr();
    controller_->Init(profile());
    controller_->Show();
    controller_->OnUserNameFetchSuccess(kFakeEmail);
  }

  void FillCreditCardInputs() {
    DetailOutputMap cc_outputs;
    const DetailInputs& cc_inputs =
        controller()->RequestedFieldsForSection(SECTION_CC);
    for (size_t i = 0; i < cc_inputs.size(); ++i) {
      cc_outputs[&cc_inputs[i]] = cc_inputs[i].type == CREDIT_CARD_NUMBER ?
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
    model->ActivatedAt(model->GetItemCount() - 2);

    // Fill the inputs.
    DetailOutputMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    for (size_t i = 0; i < inputs.size(); ++i) {
      ServerFieldType type = inputs[i].type;
      base::string16 output;
      if (type == CREDIT_CARD_VERIFICATION_CODE)
        output = ASCIIToUTF16("123");
      else
        output = data_model.GetInfo(AutofillType(type), "en-US");
      outputs[&inputs[i]] = output;
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
    controller_->MenuModelForAccountChooser()->ActivatedAt(
        TestAccountChooserModel::kAutofillItemId);
  }

  void SwitchToWallet() {
    controller_->MenuModelForAccountChooser()->ActivatedAt(
        TestAccountChooserModel::kActiveWalletItemId);
  }

  void SimulateSigninError() {
    controller_->SimulateSigninError();
  }

  void UseBillingForShipping() {
    controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(0);
  }

  void ValidateCCNumber(DialogSection section,
                        const std::string& cc_number,
                        bool should_pass) {
    DetailOutputMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);

    SetOutputValue(inputs, CREDIT_CARD_NUMBER,
                   ASCIIToUTF16(cc_number), &outputs);
    ValidityData validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(should_pass ? 0U : 1U, validity_data.count(CREDIT_CARD_NUMBER));
  }

  void SubmitWithWalletItems(scoped_ptr<wallet::WalletItems> wallet_items) {
    controller()->OnDidGetWalletItems(wallet_items.Pass());
    AcceptAndLoadFakeFingerprint();
  }

  void AcceptAndLoadFakeFingerprint() {
    controller()->OnAccept();
    controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());
  }

  bool ReadSetVisuallyDeemphasizedIpc() {
    EXPECT_EQ(1U, process()->sink().message_count());
    uint32 kMsgID = ChromeViewMsg_SetVisuallyDeemphasized::ID;
    const IPC::Message* message =
        process()->sink().GetFirstMessageMatching(kMsgID);
    EXPECT_TRUE(message);
    Tuple1<bool> payload;
    ChromeViewMsg_SetVisuallyDeemphasized::Read(message, &payload);
    process()->sink().ClearMessages();
    return payload.a;
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

  TestAutofillDialogController* controller() { return controller_.get(); }

  const FormStructure* form_structure() { return form_structure_; }

  TestGeneratedCreditCardBubbleController* test_generated_bubble_controller() {
    return test_generated_bubble_controller_;
  }

  const MockNewCreditCardBubbleController* mock_new_card_bubble_controller() {
    return mock_new_card_bubble_controller_.get();
  }

 private:
  void FinishedCallback(const FormStructure* form_structure) {
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
};

}  // namespace

// This test makes sure nothing falls over when fields are being validity-
// checked.
TEST_F(AutofillDialogControllerTest, ValidityCheck) {
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    const DetailInputs& shipping_inputs =
        controller()->RequestedFieldsForSection(section);
    for (DetailInputs::const_iterator iter = shipping_inputs.begin();
         iter != shipping_inputs.end(); ++iter) {
      controller()->InputValidityMessage(section, iter->type, string16());
    }
  }
}

// Test for phone number validation.
TEST_F(AutofillDialogControllerTest, PhoneNumberValidation) {
  // Construct DetailOutputMap from existing data.
  SwitchToAutofill();

  for (size_t i = 0; i < 2; ++i) {
    ServerFieldType phone = i == 0 ? PHONE_HOME_WHOLE_NUMBER :
                                     PHONE_BILLING_WHOLE_NUMBER;
    ServerFieldType address = i == 0 ? ADDRESS_HOME_COUNTRY :
                                       ADDRESS_BILLING_COUNTRY;
    DialogSection section = i == 0 ? SECTION_SHIPPING : SECTION_BILLING;

    DetailOutputMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    AutofillProfile full_profile(test::GetVerifiedProfile());
    for (size_t i = 0; i < inputs.size(); ++i) {
      const DetailInput& input = inputs[i];
      outputs[&input] = full_profile.GetInfo(AutofillType(input.type),
                                             "en-US");
    }

    // Make sure country is United States.
    SetOutputValue(inputs, address, ASCIIToUTF16("United States"), &outputs);

    // Existing data should have no errors.
    ValidityData validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an empty phone number with VALIDATE_FINAL.
    SetOutputValue(inputs, phone, base::string16(), &outputs);
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input an empty phone number with VALIDATE_EDIT.
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid phone number.
    SetOutputValue(inputs, phone, ASCIIToUTF16("ABC"), &outputs);
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input a local phone number.
    SetOutputValue(inputs, phone, ASCIIToUTF16("2155546699"), &outputs);
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid local phone number.
    SetOutputValue(inputs, phone, ASCIIToUTF16("215554669"), &outputs);
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input an international phone number.
    SetOutputValue(inputs, phone, ASCIIToUTF16("+33 892 70 12 39"), &outputs);
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid international phone number.
    SetOutputValue(inputs, phone,
                   ASCIIToUTF16("+112333 892 70 12 39"), &outputs);
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));
  }
}

TEST_F(AutofillDialogControllerTest, ExpirationDateValidity) {
  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);

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

  SetOutputValue(inputs, CREDIT_CARD_EXP_MONTH, default_month_value, &outputs);
  SetOutputValue(inputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 default_year_value, &outputs);

  // Expiration default values "validate" with VALIDATE_EDIT.
  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration default values fail with VALIDATE_FINAL.
  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default month "validates" with VALIDATE_EDIT.
  SetOutputValue(inputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 other_year_value, &outputs);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default year "validates" with VALIDATE_EDIT.
  SetOutputValue(inputs, CREDIT_CARD_EXP_MONTH, other_month_value, &outputs);
  SetOutputValue(inputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 default_year_value, &outputs);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default month fails with VALIDATE_FINAL.
  SetOutputValue(inputs, CREDIT_CARD_EXP_MONTH, default_month_value, &outputs);
  SetOutputValue(inputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 other_year_value, &outputs);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default year fails with VALIDATE_FINAL.
  SetOutputValue(inputs, CREDIT_CARD_EXP_MONTH, other_month_value, &outputs);
  SetOutputValue(inputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 default_year_value, &outputs);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));
}

TEST_F(AutofillDialogControllerTest, BillingNameValidation) {
  // Construct DetailOutputMap from AutofillProfile data.
  SwitchToAutofill();

  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);

  // Input an empty billing name with VALIDATE_FINAL.
  SetOutputValue(inputs, NAME_BILLING_FULL, base::string16(), &outputs);
  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(NAME_BILLING_FULL));

  // Input an empty billing name with VALIDATE_EDIT.
  validity_data =
      controller()->InputsAreValid(SECTION_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a non-empty billing name.
  SetOutputValue(inputs, NAME_BILLING_FULL, ASCIIToUTF16("Bob"), &outputs);
  validity_data =
      controller()->InputsAreValid(SECTION_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Switch to Wallet which only considers names with with at least two names to
  // be valid.
  SwitchToWallet();

  // Setup some wallet state.
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  DetailOutputMap wallet_outputs;
  const DetailInputs& wallet_inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);

  // Input an empty billing name with VALIDATE_FINAL. Data source should not
  // change this behavior.
  SetOutputValue(wallet_inputs, NAME_BILLING_FULL,
                 base::string16(), &wallet_outputs);
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(NAME_BILLING_FULL));

  // Input an empty billing name with VALIDATE_EDIT. Data source should not
  // change this behavior.
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a one name billing name. Wallet does not currently support this.
  SetOutputValue(wallet_inputs, NAME_BILLING_FULL,
                 ASCIIToUTF16("Bob"), &wallet_outputs);
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(NAME_BILLING_FULL));

  // Input a two name billing name.
  SetOutputValue(wallet_inputs, NAME_BILLING_FULL,
                 ASCIIToUTF16("Bob Barker"), &wallet_outputs);
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a more than two name billing name.
  SetOutputValue(wallet_inputs, NAME_BILLING_FULL,
                 ASCIIToUTF16("John Jacob Jingleheimer Schmidt"),
                 &wallet_outputs);
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a billing name with lots of crazy whitespace.
  SetOutputValue(
      wallet_inputs, NAME_BILLING_FULL,
      ASCIIToUTF16("     \\n\\r John \\n  Jacob Jingleheimer \\t Schmidt  "),
      &wallet_outputs);
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));
}

TEST_F(AutofillDialogControllerTest, CreditCardNumberValidation) {
  // Construct DetailOutputMap from AutofillProfile data.
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

  // Setup some wallet state.
  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());

  // Should accept Visa, Master and Discover, but not AMEX.
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberVisa, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberMaster, true);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberDiscover, true);

  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberAmex, false);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberIncomplete, false);
  ValidateCCNumber(SECTION_CC_BILLING, kTestCCNumberInvalid, false);
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
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());

  // A full, verified profile should be picked up.
  AutofillProfile verified_profile(test::GetVerifiedProfile());
  verified_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(4, shipping_model->GetItemCount());
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
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(!!shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());

  // Set up some variant data.
  AutofillProfile full_profile(test::GetVerifiedProfile());
  std::vector<string16> names;
  names.push_back(ASCIIToUTF16("John Doe"));
  names.push_back(ASCIIToUTF16("Jane Doe"));
  full_profile.SetRawMultiInfo(NAME_FULL, names);
  std::vector<string16> emails;
  emails.push_back(ASCIIToUTF16(kFakeEmail));
  emails.push_back(ASCIIToUTF16("admin@example.com"));
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, emails);

  // Non-default variants are ignored by the dialog.
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  EXPECT_EQ(4, shipping_model->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, SuggestValidEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  const string16 kValidEmail = ASCIIToUTF16(kFakeEmail);
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

  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestIncompleteAddress) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  EXPECT_FALSE(!!controller()->MenuModelForSection(SECTION_BILLING));
}

TEST_F(AutofillDialogControllerTest, AutofillCreditCards) {
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
  string16 billing_state = form_structure()->field(9)->value;
  string16 shipping_state = form_structure()->field(16)->value;
  EXPECT_FALSE(billing_state.empty());
  EXPECT_FALSE(shipping_state.empty());
  EXPECT_NE(billing_state, shipping_state);

  EXPECT_EQ(CREDIT_CARD_NAME,
            form_structure()->field(1)->Type().GetStorableType());
  string16 cc_name = form_structure()->field(1)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(6)->Type().GetStorableType());
  EXPECT_EQ(NAME_BILLING, form_structure()->field(6)->Type().group());
  string16 billing_name = form_structure()->field(6)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(13)->Type().GetStorableType());
  EXPECT_EQ(NAME, form_structure()->field(13)->Type().group());
  string16 shipping_name = form_structure()->field(13)->value;

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
  AutofillProfile full_profile2(test::GetVerifiedProfile2());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile2);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  // Test after setting use billing for shipping.
  UseBillingForShipping();

  controller()->OnAccept();
  ASSERT_EQ(20U, form_structure()->field_count());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure()->field(9)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_BILLING, form_structure()->field(9)->Type().group());
  EXPECT_EQ(ADDRESS_HOME_STATE,
            form_structure()->field(16)->Type().GetStorableType());
  EXPECT_EQ(ADDRESS_HOME, form_structure()->field(16)->Type().group());
  string16 billing_state = form_structure()->field(9)->value;
  string16 shipping_state = form_structure()->field(16)->value;
  EXPECT_FALSE(billing_state.empty());
  EXPECT_FALSE(shipping_state.empty());
  EXPECT_EQ(billing_state, shipping_state);

  EXPECT_EQ(CREDIT_CARD_NAME,
            form_structure()->field(1)->Type().GetStorableType());
  string16 cc_name = form_structure()->field(1)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(6)->Type().GetStorableType());
  EXPECT_EQ(NAME_BILLING, form_structure()->field(6)->Type().group());
  string16 billing_name = form_structure()->field(6)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(13)->Type().GetStorableType());
  EXPECT_EQ(NAME, form_structure()->field(13)->Type().group());
  string16 shipping_name = form_structure()->field(13)->value;

  EXPECT_FALSE(cc_name.empty());
  EXPECT_FALSE(billing_name.empty());
  EXPECT_FALSE(shipping_name.empty());
  EXPECT_EQ(cc_name, billing_name);
  EXPECT_EQ(cc_name, shipping_name);
}

// Tests that shipping and billing telephone fields are supported, and filled
// in by their respective profiles. http://crbug.com/244515
TEST_F(AutofillDialogControllerTest, BillingVsShippingPhoneNumber) {
  FormFieldData shipping_tel;
  shipping_tel.autocomplete_attribute = "shipping tel";
  FormFieldData billing_tel;
  billing_tel.autocomplete_attribute = "billing tel";

  FormData form_data;
  form_data.fields.push_back(shipping_tel);
  form_data.fields.push_back(billing_tel);
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
  ASSERT_EQ(2U, form_structure()->field_count());
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

TEST_F(AutofillDialogControllerTest, AcceptLegalDocuments) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AcceptLegalDocuments(_, _, _)).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);
  EXPECT_CALL(*controller(), LoadRiskFingerprintData()).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = CompleteAndValidWalletItems();
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
  controller()->OnDidAcceptLegalDocuments();
  controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());
}

// Makes sure the default object IDs are respected.
TEST_F(AutofillDialogControllerTest, WalletDefaultItems) {
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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

  // Tests if default instrument is AMEX, then, the first valid instrument is
  // selected instead of the default instrument.
  wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentAmex());
  wallet_items->AddInstrument(wallet::GetTestNonDefaultMaskedInstrument());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // 4 suggestions and "add", "manage".
  EXPECT_EQ(6,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(0));

  // Tests if only have AMEX and invalid instrument, then "add" is selected.
  wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentInvalid());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentAmex());

  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // 2 suggestions and "add", "manage".
  EXPECT_EQ(4,
      controller()->MenuModelForSection(SECTION_CC_BILLING)->GetItemCount());
  // "add"
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_CC_BILLING)->
      IsItemCheckedAt(2));
}

TEST_F(AutofillDialogControllerTest, SaveAddress) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::IsNull(),
                               testing::NotNull(),
                               _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // If there is no shipping address in wallet, it will default to
  // "same-as-billing" instead of "add-new-item". "same-as-billing" is covered
  // by the following tests. The penultimate item in the menu is "add-new-item".
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  shipping_model->ActivatedAt(shipping_model->GetItemCount() - 2);
  AcceptAndLoadFakeFingerprint();
}

TEST_F(AutofillDialogControllerTest, SaveInstrument) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               testing::IsNull(),
                               _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  SubmitWithWalletItems(wallet_items.Pass());
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentWithInvalidInstruments) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               testing::IsNull(),
                               _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentInvalid());
  SubmitWithWalletItems(wallet_items.Pass());
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentAndAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               testing::NotNull(),
                               _)).Times(1);

  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());
  AcceptAndLoadFakeFingerprint();
}

MATCHER(IsUpdatingExistingData, "updating existing Wallet data") {
  return !arg->object_id().empty();
}

MATCHER(UsesLocalBillingAddress, "uses the local billing address") {
  return arg->address_line_1() == ASCIIToUTF16(kEditedBillingAddress);
}

// Tests that when using billing address for shipping, and there is no exact
// matched shipping address, then a shipping address should be added.
TEST_F(AutofillDialogControllerTest, BillingForShipping) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::IsNull(),
                               testing::NotNull(),
                               _)).Times(1);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  // Select "Same as billing" in the address menu.
  UseBillingForShipping();

  AcceptAndLoadFakeFingerprint();
}

// Tests that when using billing address for shipping, and there is an exact
// matched shipping address, then a shipping address should not be added.
TEST_F(AutofillDialogControllerTest, BillingForShippingHasMatch) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(_, _, _)).Times(0);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_CC_BILLING);
  model->ActivatedAt(model->GetItemCount() - 2);

  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  AutofillProfile full_profile(test::GetVerifiedProfile());
  CreditCard full_card(test::GetCreditCard());
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    if (input.type == ADDRESS_BILLING_LINE1) {
      outputs[&input] = ASCIIToUTF16(kEditedBillingAddress);
    } else {
      outputs[&input] = full_profile.GetInfo(AutofillType(input.type),
                                             "en-US");
    }

    if (outputs[&input].empty())
      outputs[&input] = full_card.GetInfo(AutofillType(input.type), "en-US");
  }
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);

  controller()->OnAccept();

  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               UsesLocalBillingAddress(),
                               _)).Times(1);
  AcceptAndLoadFakeFingerprint();
}

TEST_F(AutofillDialogControllerTest, CancelNoSave) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(_, _, _)).Times(0);

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());
  controller()->OnCancel();
}

// Checks that clicking the Manage menu item opens a new tab with a different
// URL for Wallet and Autofill.
TEST_F(AutofillDialogControllerTest, ManageItem) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  SwitchToAutofill();

  SuggestionsMenuModel* shipping = GetMenuModelForSection(SECTION_SHIPPING);
  shipping->ExecuteCommand(shipping->GetItemCount() - 1, 0);
  GURL autofill_manage_url = controller()->open_tab_url();
  EXPECT_EQ("chrome", autofill_manage_url.scheme());

  SwitchToWallet();
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  AutofillProfile full_profile2(test::GetVerifiedProfile2());
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    outputs[&input] = full_profile2.GetInfo(AutofillType(input.type), "en-US");
  }
  controller()->GetView()->SetUserInput(SECTION_BILLING, outputs);

  controller()->OnAccept();
  const AutofillProfile& added_profile =
      controller()->GetTestingManager()->imported_profile();

  const DetailInputs& shipping_inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  for (size_t i = 0; i < shipping_inputs.size(); ++i) {
    const DetailInput& input = shipping_inputs[i];
    EXPECT_EQ(full_profile2.GetInfo(AutofillType(input.type), "en-US"),
              added_profile.GetInfo(AutofillType(input.type), "en-US"));
  }
}

TEST_F(AutofillDialogControllerTest, VerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AuthenticateInstrument(_, _)).Times(1);

  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_TRUE(NotificationsOfType(DialogNotification::REQUIRED_ACTION).empty());
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SuggestionState suggestion_state =
      controller()->SuggestionStateForSection(SECTION_CC_BILLING);
  EXPECT_TRUE(suggestion_state.extra_text.empty());

  controller()->OnDidGetFullWallet(CreateFullWallet("verify_cvv"));

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
}

TEST_F(AutofillDialogControllerTest, ErrorDuringSubmit) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// TODO(dbeam): disallow changing accounts instead and remove this test.
TEST_F(AutofillDialogControllerTest, ChangeAccountDuringSubmit) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SwitchToWallet();
  SwitchToAutofill();

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(AutofillDialogControllerTest, ErrorDuringVerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  SubmitWithWalletItems(CompleteAndValidWalletItems());
  controller()->OnDidGetFullWallet(CreateFullWallet("verify_cvv"));

  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// TODO(dbeam): disallow changing accounts instead and remove this test.
TEST_F(AutofillDialogControllerTest, ChangeAccountDuringVerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  SubmitWithWalletItems(CompleteAndValidWalletItems());
  controller()->OnDidGetFullWallet(CreateFullWallet("verify_cvv"));

  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SwitchToWallet();
  SwitchToAutofill();

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// Simulates receiving an INVALID_FORM_FIELD required action while processing a
// |WalletClientDelegate::OnDid{Save,Update}*()| call. This can happen if Online
// Wallet's server validation differs from Chrome's local validation.
TEST_F(AutofillDialogControllerTest, WalletServerSideValidation) {
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();

  std::vector<wallet::RequiredAction> required_actions;
  required_actions.push_back(wallet::INVALID_FORM_FIELD);

  std::vector<wallet::FormFieldError> form_errors;
  form_errors.push_back(
      wallet::FormFieldError(wallet::FormFieldError::INVALID_POSTAL_CODE,
                             wallet::FormFieldError::SHIPPING_ADDRESS));

  EXPECT_CALL(*controller()->GetView(), UpdateForErrors()).Times(1);
  controller()->OnDidSaveToWallet(std::string(),
                                  std::string(),
                                  required_actions,
                                  form_errors);
}

// Simulates receiving unrecoverable Wallet server validation errors.
TEST_F(AutofillDialogControllerTest, WalletServerSideValidationUnrecoverable) {
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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

// Test Wallet banners are show in the right situations. These banners explain
// where Chrome got the user's data (i.e. "Got details from Wallet") or promote
// saving details into Wallet (i.e. "[x] Save details to Wallet").
TEST_F(AutofillDialogControllerTest, WalletBanners) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  command_line->AppendSwitch(switches::kWalletServiceUseProd);
  PrefService* prefs = profile()->GetPrefs();

  // Simulate first run.
  prefs->SetBoolean(::prefs::kAutofillDialogHasPaidWithWallet, false);
  SetUpControllerWithFormData(DefaultFormData());

  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // Sign in a user with a completed account.
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  // Full account; should show "Details from Wallet" message.
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // Full account; no "[x] Save details in Wallet" option should show.
  SwitchToAutofill();
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  SetUpControllerWithFormData(DefaultFormData());
  // |controller()| has already been initialized. Test that should not take
  // effect until the next call of |SetUpControllerWithFormData()|.
  prefs->SetBoolean(::prefs::kAutofillDialogHasPaidWithWallet, true);

  // Sign in a user with a incomplete account.
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  // Partial account; no "Details from Wallet" message should show, but a
  // "[x] Save details in Wallet" should.
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // Once the usage confirmation banner is shown once, it keeps showing even if
  // the user switches to Autofill data.
  SwitchToAutofill();
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  // A Wallet error should kill any Wallet promos.
  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_ERROR).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  SetUpControllerWithFormData(DefaultFormData());
  // |controller()| is error free and thinks the user has already paid w/Wallet.

  // User has already paid with wallet. Don't show promos.
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).size());

  wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  SwitchToAutofill();
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());
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
// TODO(estade): enable on other platforms when overlays are supported there.
#if defined(TOOLKIT_VIEWS)
TEST_F(AutofillDialogControllerTest, WalletFirstRun) {
  // Simulate fist run.
  PrefService* prefs = profile()->GetPrefs();
  prefs->SetBoolean(::prefs::kAutofillDialogHasPaidWithWallet, false);
  SetUpControllerWithFormData(DefaultFormData());

  SwitchToWallet();
  EXPECT_TRUE(controller()->GetDialogOverlay().image.IsEmpty());

  SubmitWithWalletItems(CompleteAndValidWalletItems());
  EXPECT_FALSE(controller()->GetDialogOverlay().image.IsEmpty());

  EXPECT_FALSE(prefs->GetBoolean(::prefs::kAutofillDialogHasPaidWithWallet));
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  EXPECT_FALSE(prefs->GetBoolean(::prefs::kAutofillDialogHasPaidWithWallet));
  EXPECT_FALSE(controller()->GetDialogOverlay().image.IsEmpty());
  EXPECT_FALSE(form_structure());

  // Don't wait for 2 seconds.
  controller()->ForceFinishSubmit();
  EXPECT_TRUE(form_structure());
}
#endif

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
  SetUpControllerWithFormData(DefaultFormData());

  SwitchToWallet();
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(0);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_BILLING)->ActivatedAt(1);
  EXPECT_TRUE(controller()->ShouldOfferToSaveInChrome());

  profile()->ForceIncognito(true);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());
}

// Tests that user is prompted when using instrument with minimal address.
TEST_F(AutofillDialogControllerTest, UpgradeMinimalAddress) {
  // A minimal address being selected should trigger error validation in the
  // view. Called once for each incomplete suggestion.
  EXPECT_CALL(*controller()->GetView(), UpdateForErrors()).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
}

TEST_F(AutofillDialogControllerTest, RiskLoadsAfterAcceptingLegalDocuments) {
  EXPECT_CALL(*controller(), LoadRiskFingerprintData()).Times(0);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  testing::Mock::VerifyAndClear(controller());
  EXPECT_CALL(*controller(), LoadRiskFingerprintData()).Times(1);

  controller()->OnAccept();

  // Simulate a risk load and verify |GetRiskData()| matches the encoded value.
  controller()->OnDidAcceptLegalDocuments();
  controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());
  EXPECT_EQ(kFakeFingerprintEncoded, controller()->GetRiskData());
}

TEST_F(AutofillDialogControllerTest, NoManageMenuItemForNewWalletUsers) {
  // Make sure the menu model item is created for a returning Wallet user.
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  SwitchToAutofill();

  FormFieldData email_field;
  email_field.autocomplete_attribute = "email";
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-number";
  FormFieldData billing_field;
  billing_field.autocomplete_attribute = "billing region";

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

TEST_F(AutofillDialogControllerTest, ShippingSectionCanBeHiddenForWallet) {
  SwitchToWallet();

  FormFieldData email_field;
  email_field.autocomplete_attribute = "email";
  FormFieldData cc_field;
  cc_field.autocomplete_attribute = "cc-number";
  FormFieldData billing_field;
  billing_field.autocomplete_attribute = "billing region";

  FormData form_data;
  form_data.fields.push_back(email_field);
  form_data.fields.push_back(cc_field);
  form_data.fields.push_back(billing_field);

  SetUpControllerWithFormData(form_data);
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_SHIPPING));
  EXPECT_FALSE(controller()->IsShippingAddressRequired());

  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  SubmitWithWalletItems(wallet_items.Pass());
  controller()->OnDidGetFullWallet(wallet::GetTestFullWalletInstrumentOnly());
  controller()->ForceFinishSubmit();
  EXPECT_TRUE(form_structure());
}

TEST_F(AutofillDialogControllerTest, NotProdNotification) {
  // To make IsPayingWithWallet() true.
  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  ASSERT_FALSE(command_line->HasSwitch(switches::kWalletServiceUseProd));
  EXPECT_FALSE(
      NotificationsOfType(DialogNotification::DEVELOPER_WARNING).empty());

  command_line->AppendSwitch(switches::kWalletServiceUseProd);
  EXPECT_TRUE(
      NotificationsOfType(DialogNotification::DEVELOPER_WARNING).empty());
}

// Ensure Wallet instruments marked expired by the server are shown as invalid.
TEST_F(AutofillDialogControllerTest, WalletExpiredCard) {
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentExpired());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  EXPECT_TRUE(controller()->IsEditingExistingData(SECTION_CC_BILLING));

  // Use |SetOutputValue()| to put the right ServerFieldTypes into the map.
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  DetailOutputMap outputs;
  CopyInitialValues(inputs, &outputs);
  SetOutputValue(inputs, COMPANY_NAME, ASCIIToUTF16("Bluth Company"), &outputs);

  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input year differ from the instrument.
  CopyInitialValues(inputs, &outputs);
  SetOutputValue(inputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 ASCIIToUTF16("3002"), &outputs);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input month differ from the instrument.
  CopyInitialValues(inputs, &outputs);
  SetOutputValue(inputs, CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("06"), &outputs);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
}

TEST_F(AutofillDialogControllerTest, ChooseAnotherInstrumentOrAddress) {
  SubmitWithWalletItems(CompleteAndValidWalletItems());

  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetWalletItems(_)).Times(1);
  controller()->OnDidGetFullWallet(
      CreateFullWallet("choose_another_instrument_or_address"));
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  controller()->OnAccept();
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
}

TEST_F(AutofillDialogControllerTest, NewCardBubbleShown) {
  EXPECT_CALL(*test_generated_bubble_controller(), SetupAndShow(_, _)).Times(0);

  SwitchToAutofill();
  FillCreditCardInputs();
  controller()->OnAccept();
  controller()->ViewClosed();

  EXPECT_EQ(1, mock_new_card_bubble_controller()->bubbles_shown());
}

TEST_F(AutofillDialogControllerTest, GeneratedCardBubbleShown) {
  EXPECT_CALL(*test_generated_bubble_controller(), SetupAndShow(_, _)).Times(1);

  SubmitWithWalletItems(CompleteAndValidWalletItems());
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  controller()->ForceFinishSubmit();
  controller()->ViewClosed();

  EXPECT_EQ(0, mock_new_card_bubble_controller()->bubbles_shown());
}

// Verify that new Wallet data is fetched when the user switches away from the
// tab hosting the Autofill dialog and back. Also verify that the user's
// selection is preserved across this re-fetch.
TEST_F(AutofillDialogControllerTest, ReloadWalletItemsOnActivation) {
  // Switch into Wallet mode and initialize some Wallet data.
  SwitchToWallet();

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_));
  controller()->TabActivated();

  // Simulate a response that includes different items.
  wallet_items = wallet::GetTestWalletItems();
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
  // Switch into Wallet mode and initialize some Wallet data.
  SwitchToWallet();

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_));
  controller()->TabActivated();

  // Simulate a response that includes different default values.
  wallet_items =
      wallet::GetTestWalletItemsWithDefaultIds("new_default_instrument_id",
                                               "new_default_address_id");
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
  SwitchToWallet();

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  controller()->MenuModelForSection(SECTION_CC_BILLING)->ActivatedAt(1);
  controller()->MenuModelForSection(SECTION_SHIPPING)->ActivatedAt(1);

  EXPECT_CALL(*controller()->GetTestingWalletClient(), GetWalletItems(_));
  controller()->TabActivated();

  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());

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
  EXPECT_EQ(1, controller()->get_submit_button_delay_count());

  // Begin the submit button delay.
  controller()->SimulateSubmitButtonDelayBegin();

  EXPECT_TRUE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // There's no spinner in Autofill mode.
  SwitchToAutofill();

  EXPECT_FALSE(controller()->ShouldShowSpinner());
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // End the submit button delay.
  controller()->SimulateSubmitButtonDelayEnd();

  EXPECT_FALSE(controller()->ShouldShowSpinner());
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
}

}  // namespace autofill
