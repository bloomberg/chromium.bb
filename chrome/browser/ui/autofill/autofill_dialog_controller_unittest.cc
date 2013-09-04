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


void SetOutputValue(const DetailInputs& inputs,
                    DetailOutputMap* outputs,
                    ServerFieldType type,
                    const base::string16& value) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    (*outputs)[&input] = input.type == type ?
        value :
        input.initial_value;
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
      : AccountChooserModel(delegate, prefs, metric_logger,
                            DIALOG_TYPE_REQUEST_AUTOCOMPLETE) {}
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
      const DialogType dialog_type,
      const base::Callback<void(const FormStructure*,
                                const std::string&)>& callback,
      MockNewCreditCardBubbleController* mock_new_card_bubble_controller)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     dialog_type,
                                     callback),
        metric_logger_(metric_logger),
        mock_wallet_client_(
            Profile::FromBrowserContext(contents->GetBrowserContext())->
                GetRequestContext(), this),
        dialog_type_(dialog_type),
        mock_new_card_bubble_controller_(mock_new_card_bubble_controller) {}

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

  virtual DialogType GetDialogType() const OVERRIDE {
    return dialog_type_;
  }

  void set_dialog_type(DialogType dialog_type) { dialog_type_ = dialog_type; }

  void SimulateSigninError() {
    OnWalletSigninError();
  }

  // Skips past the 2 second wait between FinishSubmit and DoFinishSubmit.
  void ForceFinishSubmit() {
#if defined(TOOLKIT_VIEWS)
    DoFinishSubmit();
#endif
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

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  TestPersonalDataManager test_manager_;
  testing::NiceMock<wallet::MockWalletClient> mock_wallet_client_;
  GURL open_tab_url_;
  DialogType dialog_type_;
  MockNewCreditCardBubbleController* mock_new_card_bubble_controller_;

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

    base::Callback<void(const FormStructure*, const std::string&)> callback =
        base::Bind(&AutofillDialogControllerTest::FinishedCallback,
                   base::Unretained(this));
    controller_ = (new testing::NiceMock<TestAutofillDialogController>(
        web_contents(),
        form_data,
        GURL(),
        metric_logger_,
        DIALOG_TYPE_REQUEST_AUTOCOMPLETE,
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

    SetOutputValue(inputs, &outputs, CREDIT_CARD_NUMBER,
                   ASCIIToUTF16(cc_number));
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

  TestAutofillDialogController* controller() { return controller_.get(); }

  const FormStructure* form_structure() { return form_structure_; }

  TestGeneratedCreditCardBubbleController* test_generated_bubble_controller() {
    return test_generated_bubble_controller_;
  }

  const MockNewCreditCardBubbleController* mock_new_card_bubble_controller() {
    return mock_new_card_bubble_controller_.get();
  }

 private:
  void FinishedCallback(const FormStructure* form_structure,
                        const std::string& google_transaction_id) {
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

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  for (size_t i = 0; i < 2; ++i) {
    ServerFieldType phone = i == 0 ? PHONE_HOME_WHOLE_NUMBER :
                                     PHONE_BILLING_WHOLE_NUMBER;
    ServerFieldType address = i == 0 ? ADDRESS_HOME_COUNTRY :
                                       ADDRESS_BILLING_COUNTRY;
    DialogSection section = i == 0 ? SECTION_SHIPPING : SECTION_BILLING;

    controller()->EditClickedForSection(section);

    DetailOutputMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    // Make sure country is United States.
    SetOutputValue(inputs, &outputs, address, ASCIIToUTF16("United States"));

    // Existing data should have no errors.
    ValidityData validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an empty phone number with VALIDATE_FINAL.
    SetOutputValue( inputs, &outputs, phone, base::string16());
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input an empty phone number with VALIDATE_EDIT.
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid phone number.
    SetOutputValue(inputs, &outputs, phone, ASCIIToUTF16("ABC"));
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input a local phone number.
    SetOutputValue(inputs, &outputs, phone, ASCIIToUTF16("2155546699"));
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid local phone number.
    SetOutputValue(inputs, &outputs, phone, ASCIIToUTF16("215554669"));
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input an international phone number.
    SetOutputValue(inputs, &outputs, phone, ASCIIToUTF16("+33 892 70 12 39"));
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid international phone number.
    SetOutputValue(inputs, &outputs, phone,
                   ASCIIToUTF16("+112333 892 70 12 39"));
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

  base::string16 other_year_value =
      exp_year_model->GetItemAt(exp_year_model->GetItemCount() - 1);
  base::string16 other_month_value =
      exp_month_model->GetItemAt(exp_month_model->GetItemCount() - 1);

  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 default_year_value);

  // Expiration default values "validate" with VALIDATE_EDIT.
  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default month "validates" with VALIDATE_EDIT.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 other_year_value);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default year "validates" with VALIDATE_EDIT.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_MONTH, other_month_value);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration default values fail with VALIDATE_FINAL.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 default_year_value);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default month fails with VALIDATE_FINAL.
  SetOutputValue(inputs,
                 &outputs,
                 CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 other_year_value);

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));

  // Expiration date with default year fails with VALIDATE_FINAL.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_MONTH, other_month_value);

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
  SetOutputValue(inputs, &outputs, NAME_BILLING_FULL, base::string16());
  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_BILLING, outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(NAME_BILLING_FULL));

  // Input an empty billing name with VALIDATE_EDIT.
  validity_data =
      controller()->InputsAreValid(SECTION_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a non-empty billing name.
  SetOutputValue(inputs, &outputs, NAME_BILLING_FULL, ASCIIToUTF16("Bob"));
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
  SetOutputValue(wallet_inputs, &wallet_outputs, NAME_BILLING_FULL,
                 base::string16());
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
  SetOutputValue(wallet_inputs, &wallet_outputs, NAME_BILLING_FULL,
                 ASCIIToUTF16("Bob"));
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(NAME_BILLING_FULL));

  // Input a two name billing name.
  SetOutputValue(wallet_inputs, &wallet_outputs, NAME_BILLING_FULL,
                 ASCIIToUTF16("Bob Barker"));
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a more than two name billing name.
  SetOutputValue(wallet_inputs, &wallet_outputs, NAME_BILLING_FULL,
                 ASCIIToUTF16("John Jacob Jingleheimer Schmidt"));
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(NAME_BILLING_FULL));

  // Input a billing name with lots of crazy whitespace.
  SetOutputValue(
      wallet_inputs, &wallet_outputs, NAME_BILLING_FULL,
      ASCIIToUTF16("     \\n\\r John \\n  Jacob Jingleheimer \\t Schmidt  "));
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
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_EMAIL));

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(3);

  // Empty profiles are ignored.
  AutofillProfile empty_profile(base::GenerateGUID(), kSettingsOrigin);
  empty_profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Doe"));
  controller()->GetTestingManager()->AddTestingProfile(&empty_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_EMAIL));

  // An otherwise full but unverified profile should be ignored.
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.set_origin("https://www.example.com");
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(3, shipping_model->GetItemCount());
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_EMAIL));

  // A full, verified profile should be picked up.
  AutofillProfile verified_profile(test::GetVerifiedProfile());
  verified_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&verified_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  ASSERT_TRUE(shipping_model);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  EXPECT_TRUE(!!controller()->MenuModelForSection(SECTION_EMAIL));
}

// Makes sure that the choice of which Autofill profile to use for each section
// is sticky.
TEST_F(AutofillDialogControllerTest, AutofillProfileDefaults) {
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.set_origin(kSettingsOrigin);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  AutofillProfile full_profile2(test::GetFullProfile2());
  full_profile2.set_origin(kSettingsOrigin);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile2);

  // Until a selection has been made, the default shipping suggestion is the
  // first one (after "use billing").
  SuggestionsMenuModel* shipping_model = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_SHIPPING));
  EXPECT_EQ(1, shipping_model->checked_item());

  for (int i = 2; i >= 0; --i) {
    shipping_model = static_cast<SuggestionsMenuModel*>(
        controller()->MenuModelForSection(SECTION_SHIPPING));
    shipping_model->ExecuteCommand(i, 0);
    FillCreditCardInputs();
    controller()->OnAccept();

    Reset();
    controller()->GetTestingManager()->AddTestingProfile(&full_profile);
    controller()->GetTestingManager()->AddTestingProfile(&full_profile2);
    shipping_model = static_cast<SuggestionsMenuModel*>(
        controller()->MenuModelForSection(SECTION_SHIPPING));
    EXPECT_EQ(i, shipping_model->checked_item());
  }

  // Try again, but don't add the default profile to the PDM. The dialog
  // should fall back to the first profile.
  shipping_model->ExecuteCommand(2, 0);
  FillCreditCardInputs();
  controller()->OnAccept();
  Reset();
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  shipping_model = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_SHIPPING));
  EXPECT_EQ(1, shipping_model->checked_item());
}

TEST_F(AutofillDialogControllerTest, AutofillProfileVariants) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  ui::MenuModel* email_model =
      controller()->MenuModelForSection(SECTION_EMAIL);
  EXPECT_FALSE(email_model);

  // Set up some variant data.
  AutofillProfile full_profile(test::GetVerifiedProfile());
  std::vector<string16> names;
  names.push_back(ASCIIToUTF16("John Doe"));
  names.push_back(ASCIIToUTF16("Jane Doe"));
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, names);
  const string16 kEmail1 = ASCIIToUTF16(kFakeEmail);
  const string16 kEmail2 = ASCIIToUTF16("admin@example.com");
  std::vector<string16> emails;
  emails.push_back(kEmail1);
  emails.push_back(kEmail2);
  full_profile.SetRawMultiInfo(EMAIL_ADDRESS, emails);

  // Respect variants for the email address field only.
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  email_model = controller()->MenuModelForSection(SECTION_EMAIL);
  ASSERT_TRUE(!!email_model);
  EXPECT_EQ(4, email_model->GetItemCount());

  // The first one is the default.
  SuggestionsMenuModel* email_suggestions = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_EMAIL));
  EXPECT_EQ(0, email_suggestions->checked_item());

  email_model->ActivatedAt(0);
  EXPECT_EQ(kEmail1,
            controller()->SuggestionStateForSection(SECTION_EMAIL).
                vertically_compact_text);
  email_model->ActivatedAt(1);
  EXPECT_EQ(kEmail2,
            controller()->SuggestionStateForSection(SECTION_EMAIL).
                vertically_compact_text);

  controller()->EditClickedForSection(SECTION_EMAIL);
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(kEmail2, inputs[0].initial_value);

  // The choice of variant is persisted across runs of the dialog.
  email_model->ActivatedAt(0);
  email_model->ActivatedAt(1);
  FillCreditCardInputs();
  controller()->OnAccept();

  Reset();
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  email_suggestions = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_EMAIL));
  EXPECT_EQ(1, email_suggestions->checked_item());
}

TEST_F(AutofillDialogControllerTest, SuggestValidEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  const string16 kValidEmail = ASCIIToUTF16(kFakeEmail);
  profile.SetRawInfo(EMAIL_ADDRESS, kValidEmail);
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  controller()->MenuModelForSection(SECTION_EMAIL)->ActivatedAt(0);
  EXPECT_EQ(kValidEmail,
            controller()->SuggestionStateForSection(SECTION_EMAIL).
                vertically_compact_text);
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestInvalidEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(".!#$%&'*+/=?^_`-@-.."));
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  EXPECT_EQ(static_cast<ui::MenuModel*>(NULL),
            controller()->MenuModelForSection(SECTION_EMAIL));
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestEmailFromIncompleteProfile) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(kFakeEmail));
  profile.SetRawInfo(ADDRESS_HOME_STATE, base::string16());
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  EXPECT_EQ(static_cast<ui::MenuModel*>(NULL),
            controller()->MenuModelForSection(SECTION_EMAIL));
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestEmailFromInvalidProfile) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(kFakeEmail));
  profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("C"));
  controller()->GetTestingManager()->AddTestingProfile(&profile);
  EXPECT_EQ(static_cast<ui::MenuModel*>(NULL),
            controller()->MenuModelForSection(SECTION_EMAIL));
}

TEST_F(AutofillDialogControllerTest, SuggestValidAddress) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  EXPECT_EQ(
      4, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
}

TEST_F(AutofillDialogControllerTest, DoNotSuggestInvalidAddress) {
  AutofillProfile full_profile(test::GetVerifiedProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("C"));
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  EXPECT_EQ(
      3, controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
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
  // by the following tests. The last item in the menu is "add-new-item".
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  shipping_model->ActivatedAt(shipping_model->GetItemCount() - 1);
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

// Tests that editing an address (in wallet mode0 and submitting the dialog
// should update the existing address on the server via WalletClient.
TEST_F(AutofillDialogControllerTest, UpdateAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::IsNull(),
                               IsUpdatingExistingData(),
                               _)).Times(1);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  controller()->EditClickedForSection(SECTION_SHIPPING);
  AcceptAndLoadFakeFingerprint();
}

// Tests that editing an instrument (CC + address) in wallet mode updates an
// existing instrument on the server via WalletClient.
TEST_F(AutofillDialogControllerTest, UpdateInstrument) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(IsUpdatingExistingData(),
                               testing::IsNull(),
                               _)).Times(1);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  controller()->EditClickedForSection(SECTION_CC_BILLING);
  AcceptAndLoadFakeFingerprint();
}

// Test that a user is able to edit their instrument and add a new address in
// the same submission.
TEST_F(AutofillDialogControllerTest, UpdateInstrumentSaveAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(IsUpdatingExistingData(),
                               testing::NotNull(),
                               _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  controller()->EditClickedForSection(SECTION_CC_BILLING);
  AcceptAndLoadFakeFingerprint();
}

// Test that saving a new instrument and editing an address works.
TEST_F(AutofillDialogControllerTest, SaveInstrumentUpdateAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveToWalletMock(testing::NotNull(),
                               IsUpdatingExistingData(),
                               _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());

  controller()->OnDidGetWalletItems(wallet_items.Pass());

  controller()->EditClickedForSection(SECTION_SHIPPING);
  AcceptAndLoadFakeFingerprint();
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

  controller()->EditClickedForSection(SECTION_CC_BILLING);
  controller()->OnAccept();

  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    outputs[&input] = input.type == ADDRESS_BILLING_LINE1 ?
        ASCIIToUTF16(kEditedBillingAddress) : input.initial_value;
  }
  controller()->GetView()->SetUserInput(SECTION_CC_BILLING, outputs);

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

  SuggestionsMenuModel* shipping = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_SHIPPING));
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

  SuggestionsMenuModel* billing = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_CC_BILLING));
  controller()->SuggestionItemSelected(billing, billing->GetItemCount() - 1);
  GURL wallet_manage_instruments_url = controller()->open_tab_url();
  EXPECT_EQ("https", wallet_manage_instruments_url.scheme());

  EXPECT_NE(autofill_manage_url, wallet_manage_instruments_url);
  EXPECT_NE(wallet_manage_instruments_url, wallet_manage_addresses_url);
}

TEST_F(AutofillDialogControllerTest, EditClicked) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  const string16 kEmail = ASCIIToUTF16("first@johndoe.com");
  full_profile.SetRawInfo(EMAIL_ADDRESS, kEmail);
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  ui::MenuModel* email_model =
      controller()->MenuModelForSection(SECTION_EMAIL);
  EXPECT_EQ(3, email_model->GetItemCount());

  // When unedited, the initial_value should be empty.
  email_model->ActivatedAt(0);
  const DetailInputs& inputs0 =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(string16(), inputs0[0].initial_value);
  EXPECT_EQ(kEmail,
            controller()->SuggestionStateForSection(SECTION_EMAIL).
                vertically_compact_text);

  // When edited, the initial_value should contain the value.
  controller()->EditClickedForSection(SECTION_EMAIL);
  const DetailInputs& inputs1 =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(kEmail, inputs1[0].initial_value);
  EXPECT_FALSE(controller()->SuggestionStateForSection(SECTION_EMAIL).visible);
}

// Tests that editing an autofill profile and then submitting works.
TEST_F(AutofillDialogControllerTest, EditAutofillProfile) {
  SwitchToAutofill();

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  controller()->EditClickedForSection(SECTION_SHIPPING);

  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    outputs[&input] = input.type == NAME_FULL ? ASCIIToUTF16("Edited Name") :
                                                input.initial_value;
  }
  controller()->GetView()->SetUserInput(SECTION_SHIPPING, outputs);

  // We also have to simulate CC inputs to keep the controller happy.
  FillCreditCardInputs();

  controller()->OnAccept();
  const AutofillProfile& edited_profile =
      controller()->GetTestingManager()->imported_profile();

  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    EXPECT_EQ(input.type == NAME_FULL ? ASCIIToUTF16("Edited Name") :
                                        input.initial_value,
              edited_profile.GetInfo(AutofillType(input.type), "en-US"));
  }
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

  // Also, the currently selected email address should get added to the new
  // profile.
  string16 original_email =
      full_profile.GetInfo(AutofillType(EMAIL_ADDRESS), "en-US");
  EXPECT_FALSE(original_email.empty());
  EXPECT_EQ(original_email,
            added_profile.GetInfo(AutofillType(EMAIL_ADDRESS), "en-US"));
}

// Makes sure that a newly added email address gets added to an existing profile
// (as opposed to creating its own profile). http://crbug.com/240926
TEST_F(AutofillDialogControllerTest, AddEmail) {
  SwitchToAutofill();
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);

  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_EMAIL);
  ASSERT_TRUE(model);
  // Activate the "Add email address" menu item.
  model->ActivatedAt(model->GetItemCount() - 2);

  // Fill in the inputs from the profile.
  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  const DetailInput& input = inputs[0];
  string16 new_email = ASCIIToUTF16("addemailtest@example.com");
  outputs[&input] = new_email;
  controller()->GetView()->SetUserInput(SECTION_EMAIL, outputs);

  FillCreditCardInputs();
  controller()->OnAccept();
  std::vector<base::string16> email_values;
  full_profile.GetMultiInfo(
      AutofillType(EMAIL_ADDRESS), "en-US", &email_values);
  ASSERT_EQ(2U, email_values.size());
  EXPECT_EQ(new_email, email_values[1]);
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

  // Email section should be showing when using Autofill.
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_EMAIL));

  SwitchToWallet();

  // Setup some wallet state, submit, and get a full wallet to end the flow.
  scoped_ptr<wallet::WalletItems> wallet_items = CompleteAndValidWalletItems();

  // Filling |form_structure()| depends on the current username and wallet items
  // being fetched. Until both of these have occurred, the user should not be
  // able to click Submit if using Wallet. The username fetch happened earlier.
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));

  // Email section should be hidden when using Wallet.
  EXPECT_FALSE(controller()->SectionIsActive(SECTION_EMAIL));

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
  ASSERT_LT(i, form_structure()->field_count());
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

  controller()->EditClickedForSection(SECTION_EMAIL);
  EXPECT_TRUE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_EMAIL)->ActivatedAt(0);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_EMAIL)->ActivatedAt(1);
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
  SetOutputValue(inputs, &outputs, COMPANY_NAME, ASCIIToUTF16("Bluth Company"));

  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input year differ from the instrument.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                 ASCIIToUTF16("3002"));

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input month differ from the instrument.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("06"));

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
  // "add", "manage", and 2 suggestions.
  ASSERT_EQ(4, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(1));
  // "use billing", "add", "manage", and 2 suggestions.
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
  // "add", "manage", and 3 suggestions.
  ASSERT_EQ(5, cc_billing_model->GetItemCount());
  EXPECT_TRUE(cc_billing_model->IsItemCheckedAt(2));
  // "use billing", "add", "manage", and 1 suggestion.
  ASSERT_EQ(4, shipping_model->GetItemCount());
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

}  // namespace autofill
