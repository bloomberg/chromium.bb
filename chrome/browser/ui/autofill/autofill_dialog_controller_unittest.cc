// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_client.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_common_test.h"
#include "components/autofill/core/browser/autofill_metrics.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
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
                    AutofillFieldType type,
                    const std::string& value) {
  for (size_t i = 0; i < inputs.size(); ++i) {
    const DetailInput& input = inputs[i];
    (*outputs)[&input] = input.type == type ?
        ASCIIToUTF16(value) :
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
  TestAutofillDialogView() {}
  virtual ~TestAutofillDialogView() {}

  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void UpdateNotificationArea() OVERRIDE {}
  virtual void UpdateAccountChooser() OVERRIDE {}
  virtual void UpdateButtonStrip() OVERRIDE {}
  virtual void UpdateDetailArea() OVERRIDE {}
  virtual void UpdateAutocheckoutStepsArea() OVERRIDE {}
  virtual void UpdateSection(DialogSection section) OVERRIDE {}
  virtual void FillSection(DialogSection section,
                           const DetailInput& originating_input) OVERRIDE {};
  virtual void GetUserInput(DialogSection section, DetailOutputMap* output)
      OVERRIDE {
    *output = outputs_[section];
  }

  virtual string16 GetCvc() OVERRIDE { return string16(); }
  virtual bool SaveDetailsLocally() OVERRIDE { return true; }
  virtual const content::NavigationController* ShowSignIn() OVERRIDE {
    return NULL;
  }
  virtual void HideSignIn() OVERRIDE {}
  virtual void UpdateProgressBar(double value) OVERRIDE {}

  MOCK_METHOD0(ModelChanged, void());
  MOCK_METHOD0(UpdateForErrors, void());

  virtual void OnSignInResize(const gfx::Size& pref_size) OVERRIDE {}

  void SetUserInput(DialogSection section, const DetailOutputMap& map) {
    outputs_[section] = map;
  }

 private:
  std::map<DialogSection, DetailOutputMap> outputs_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogView);
};

class TestWalletClient : public wallet::WalletClient {
 public:
  TestWalletClient(net::URLRequestContextGetter* context,
                   wallet::WalletClientDelegate* delegate)
      : wallet::WalletClient(context, delegate) {}
  virtual ~TestWalletClient() {}

  MOCK_METHOD3(AcceptLegalDocuments,
      void(const std::vector<wallet::WalletItems::LegalDocument*>& documents,
           const std::string& google_transaction_id,
           const GURL& source_url));

  MOCK_METHOD3(AuthenticateInstrument,
      void(const std::string& instrument_id,
           const std::string& card_verification_number,
           const std::string& obfuscated_gaia_id));

  MOCK_METHOD1(GetFullWallet,
      void(const wallet::WalletClient::FullWalletRequest& request));

  MOCK_METHOD1(GetWalletItems, void(const GURL& source_url));

  MOCK_METHOD2(SaveAddress,
      void(const wallet::Address& address, const GURL& source_url));

  MOCK_METHOD3(SaveInstrument,
      void(const wallet::Instrument& instrument,
           const std::string& obfuscated_gaia_id,
           const GURL& source_url));

  MOCK_METHOD4(SaveInstrumentAndAddress,
      void(const wallet::Instrument& instrument,
           const wallet::Address& address,
           const std::string& obfuscated_gaia_id,
           const GURL& source_url));

  MOCK_METHOD2(UpdateAddress,
      void(const wallet::Address& address, const GURL& source_url));

  virtual void UpdateInstrument(
      const wallet::WalletClient::UpdateInstrumentRequest& update_request,
      scoped_ptr<wallet::Address> billing_address) {
    updated_billing_address_ = billing_address.Pass();
  }

  const wallet::Address* updated_billing_address() {
    return updated_billing_address_.get();
  }

 private:
  scoped_ptr<wallet::Address> updated_billing_address_;

  DISALLOW_COPY_AND_ASSIGN(TestWalletClient);
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
                                const std::string&)>& callback)
      : AutofillDialogControllerImpl(contents,
                                     form_structure,
                                     source_url,
                                     dialog_type,
                                     callback),
        metric_logger_(metric_logger),
        test_wallet_client_(
            Profile::FromBrowserContext(contents->GetBrowserContext())->
                GetRequestContext(), this),
        dialog_type_(dialog_type) {}
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

  TestWalletClient* GetTestingWalletClient() {
    return &test_wallet_client_;
  }

  const GURL& open_tab_url() { return open_tab_url_; }

  virtual DialogType GetDialogType() const OVERRIDE {
    return dialog_type_;
  }

  void set_dialog_type(DialogType dialog_type) { dialog_type_ = dialog_type; }

  void SimulateSigninError() {
    OnWalletSigninError();
  }

  MOCK_METHOD0(LoadRiskFingerprintData, void());
  using AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData;
  using AutofillDialogControllerImpl::IsEditingExistingData;

 protected:
  virtual PersonalDataManager* GetManager() OVERRIDE {
    return &test_manager_;
  }

  virtual wallet::WalletClient* GetWalletClient() OVERRIDE {
    return &test_wallet_client_;
  }

  virtual void OpenTabWithUrl(const GURL& url) OVERRIDE {
    open_tab_url_ = url;
  }

 private:
  // To specify our own metric logger.
  virtual const AutofillMetrics& GetMetricLogger() const OVERRIDE {
    return metric_logger_;
  }

  const AutofillMetrics& metric_logger_;
  TestPersonalDataManager test_manager_;
  testing::NiceMock<TestWalletClient> test_wallet_client_;
  GURL open_tab_url_;
  DialogType dialog_type_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogControllerTest : public testing::Test {
 protected:
  AutofillDialogControllerTest()
    : form_structure_(NULL) {
  }

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    profile()->CreateRequestContext();
    test_web_contents_.reset(
        content::WebContentsTester::CreateTestWebContents(profile(), NULL));

    SetUpControllerWithFormData(DefaultFormData());
  }

  virtual void TearDown() OVERRIDE {
    if (controller_.get())
      controller_->ViewClosed();
  }

 protected:
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
    if (controller_.get())
      controller_->ViewClosed();

    base::Callback<void(const FormStructure*, const std::string&)> callback =
        base::Bind(&AutofillDialogControllerTest::FinishedCallback,
                   base::Unretained(this));
    controller_ = (new testing::NiceMock<TestAutofillDialogController>(
        test_web_contents_.get(),
        form_data,
        GURL(),
        metric_logger_,
        DIALOG_TYPE_REQUEST_AUTOCOMPLETE,
        callback))->AsWeakPtr();
    controller_->Init(profile());
    controller_->Show();
    controller_->OnUserNameFetchSuccess(kFakeEmail);
  }

  void FillCreditCardInputs() {
    DetailOutputMap cc_outputs;
    const DetailInputs& cc_inputs =
        controller()->RequestedFieldsForSection(SECTION_CC);
    for (size_t i = 0; i < cc_inputs.size(); ++i) {
      cc_outputs[&cc_inputs[i]] = ASCIIToUTF16("11");
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

    SetOutputValue(inputs, &outputs, CREDIT_CARD_NUMBER, cc_number);
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

  TestAutofillDialogController* controller() { return controller_.get(); }

  TestingProfile* profile() { return &profile_; }

  const FormStructure* form_structure() { return form_structure_; }

 private:
  void FinishedCallback(const FormStructure* form_structure,
                        const std::string& google_transaction_id) {
    form_structure_ = form_structure;
    if (controller()->GetDialogType() == DIALOG_TYPE_AUTOCHECKOUT)
      EXPECT_TRUE(controller()->AutocheckoutIsRunning());
  }

  // Must be first member to ensure TestBrowserThreads outlive other objects.
  content::TestBrowserThreadBundle thread_bundle_;

#if defined(OS_WIN)
   // http://crbug.com/227221
   ui::ScopedOleInitializer ole_initializer_;
#endif

  TestingProfile profile_;

  // The controller owns itself.
  base::WeakPtr<TestAutofillDialogController> controller_;

  scoped_ptr<content::WebContents> test_web_contents_;

  // Must outlive the controller.
  AutofillMetrics metric_logger_;

  // Returned when the dialog closes successfully.
  const FormStructure* form_structure_;
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
    AutofillFieldType phone = i == 0 ? PHONE_HOME_WHOLE_NUMBER :
                                       PHONE_BILLING_WHOLE_NUMBER;
    AutofillFieldType address = i == 0 ? ADDRESS_HOME_COUNTRY :
                                         ADDRESS_BILLING_COUNTRY;
    DialogSection section = i == 0 ? SECTION_SHIPPING : SECTION_BILLING;

    controller()->EditClickedForSection(section);

    DetailOutputMap outputs;
    const DetailInputs& inputs =
        controller()->RequestedFieldsForSection(section);
    // Make sure country is United States.
    SetOutputValue(inputs, &outputs, address, "United States");

    // Existing data should have no errors.
    ValidityData validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an empty phone number with VALIDATE_FINAL.
    SetOutputValue( inputs, &outputs, phone, "");
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_FINAL);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input an empty phone number with VALIDATE_EDIT.
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid phone number.
    SetOutputValue(inputs, &outputs, phone, "ABC");
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input a local phone number.
    SetOutputValue(inputs, &outputs, phone, "2155546699");
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid local phone number.
    SetOutputValue(inputs, &outputs, phone, "215554669");
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));

    // Input an international phone number.
    SetOutputValue(inputs, &outputs, phone, "+33 892 70 12 39");
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(0U, validity_data.count(phone));

    // Input an invalid international phone number.
    SetOutputValue(inputs, &outputs, phone, "+112333 892 70 12 39");
    validity_data =
        controller()->InputsAreValid(section, outputs, VALIDATE_EDIT);
    EXPECT_EQ(1U, validity_data.count(phone));
  }
}

TEST_F(AutofillDialogControllerTest, CardHolderNameValidation) {
  // Construct DetailOutputMap from AutofillProfile data.
  SwitchToAutofill();

  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC);

  // Input an empty card holder name with VALIDATE_FINAL.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_NAME, "");
  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_CC, outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_NAME));

  // Input an empty card holder name with VALIDATE_EDIT.
  validity_data =
      controller()->InputsAreValid(SECTION_CC, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_NAME));

  // Input a non-empty card holder name.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_NAME, "Bob");
  validity_data =
      controller()->InputsAreValid(SECTION_CC, outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_NAME));

  // Switch to Wallet which only considers names with with at least two names to
  // be valid.
  SwitchToWallet();

  // Setup some wallet state.
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  DetailOutputMap wallet_outputs;
  const DetailInputs& wallet_inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);

  // Input an empty card holder name with VALIDATE_FINAL. Data source should not
  // change this behavior.
  SetOutputValue(wallet_inputs, &wallet_outputs, CREDIT_CARD_NAME, "");
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_NAME));

  // Input an empty card holder name with VALIDATE_EDIT. Data source should not
  // change this behavior.
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_NAME));

  // Input a one name card holder name. Wallet does not currently support this.
  SetOutputValue(wallet_inputs, &wallet_outputs, CREDIT_CARD_NAME, "Bob");
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_NAME));

  // Input a two name card holder name.
  SetOutputValue(wallet_inputs, &wallet_outputs, CREDIT_CARD_NAME,
                 "Bob Barker");
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_NAME));

  // Input a more than two name card holder name.
  SetOutputValue(wallet_inputs, &wallet_outputs, CREDIT_CARD_NAME,
                 "John Jacob Jingleheimer Schmidt");
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_NAME));

  // Input a card holder name with lots of crazy whitespace.
  SetOutputValue(wallet_inputs, &wallet_outputs, CREDIT_CARD_NAME,
                 "     \\n\\r John \\n  Jacob Jingleheimer \\t Schmidt  ");
  validity_data =
      controller()->InputsAreValid(
          SECTION_CC_BILLING, wallet_outputs, VALIDATE_FINAL);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_NAME));
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

    TearDown();
    SetUp();
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
  TearDown();
  SetUp();
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
            controller()->SuggestionStateForSection(SECTION_EMAIL).text);
  email_model->ActivatedAt(1);
  EXPECT_EQ(kEmail2,
            controller()->SuggestionStateForSection(SECTION_EMAIL).text);

  controller()->EditClickedForSection(SECTION_EMAIL);
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(kEmail2, inputs[0].initial_value);

  // The choice of variant is persisted across runs of the dialog.
  email_model->ActivatedAt(0);
  email_model->ActivatedAt(1);
  FillCreditCardInputs();
  controller()->OnAccept();

  TearDown();
  SetUp();
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  email_suggestions = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_EMAIL));
  EXPECT_EQ(1, email_suggestions->checked_item());
}

TEST_F(AutofillDialogControllerTest, ValidSavedEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  const string16 kValidEmail = ASCIIToUTF16(kFakeEmail);
  profile.SetRawInfo(EMAIL_ADDRESS, kValidEmail);
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  controller()->MenuModelForSection(SECTION_EMAIL)->ActivatedAt(0);
  EXPECT_EQ(kValidEmail,
            controller()->SuggestionStateForSection(SECTION_EMAIL).text);
}

TEST_F(AutofillDialogControllerTest, InvalidSavedEmail) {
  AutofillProfile profile(test::GetVerifiedProfile());
  profile.SetRawInfo(EMAIL_ADDRESS, ASCIIToUTF16(".!#$%&'*+/=?^_`-@-.."));
  controller()->GetTestingManager()->AddTestingProfile(&profile);

  controller()->MenuModelForSection(SECTION_EMAIL)->ActivatedAt(0);
  EXPECT_TRUE(
      controller()->SuggestionStateForSection(SECTION_EMAIL).text.empty());
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
  EXPECT_EQ(ADDRESS_BILLING_STATE, form_structure()->field(9)->type());
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure()->field(16)->type());
  string16 billing_state = form_structure()->field(9)->value;
  string16 shipping_state = form_structure()->field(16)->value;
  EXPECT_FALSE(billing_state.empty());
  EXPECT_FALSE(shipping_state.empty());
  EXPECT_NE(billing_state, shipping_state);

  EXPECT_EQ(CREDIT_CARD_NAME, form_structure()->field(1)->type());
  string16 cc_name = form_structure()->field(1)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(6)->type());
  string16 billing_name = form_structure()->field(6)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(13)->type());
  string16 shipping_name = form_structure()->field(13)->value;

  EXPECT_FALSE(cc_name.empty());
  EXPECT_FALSE(billing_name.empty());
  EXPECT_FALSE(shipping_name.empty());
  // Billing name should always be the same as cardholder name.
  // TODO(estade): this currently fails. http://crbug.com/246417
  // EXPECT_EQ(cc_name, billing_name);
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
  EXPECT_EQ(ADDRESS_BILLING_STATE, form_structure()->field(9)->type());
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure()->field(16)->type());
  string16 billing_state = form_structure()->field(9)->value;
  string16 shipping_state = form_structure()->field(16)->value;
  EXPECT_FALSE(billing_state.empty());
  EXPECT_FALSE(shipping_state.empty());
  EXPECT_EQ(billing_state, shipping_state);

  EXPECT_EQ(CREDIT_CARD_NAME, form_structure()->field(1)->type());
  string16 cc_name = form_structure()->field(1)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(6)->type());
  string16 billing_name = form_structure()->field(6)->value;
  EXPECT_EQ(NAME_FULL, form_structure()->field(13)->type());
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
  EXPECT_EQ(PHONE_HOME_WHOLE_NUMBER, form_structure()->field(0)->type());
  EXPECT_EQ(PHONE_BILLING_WHOLE_NUMBER, form_structure()->field(1)->type());
  EXPECT_EQ(shipping_profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER),
            form_structure()->field(0)->value);
  EXPECT_EQ(billing_profile.GetRawInfo(PHONE_BILLING_WHOLE_NUMBER),
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
              SaveAddress(_, _)).Times(1);

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
              SaveInstrument(_, _, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  SubmitWithWalletItems(wallet_items.Pass());
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentWithInvalidInstruments) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrument(_, _, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrumentInvalid());
  SubmitWithWalletItems(wallet_items.Pass());
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentAndAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrumentAndAddress(_, _, _, _)).Times(1);

  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());
  AcceptAndLoadFakeFingerprint();
}

// Tests that editing an address (in wallet mode0 and submitting the dialog
// should update the existing address on the server via WalletClient.
TEST_F(AutofillDialogControllerTest, UpdateAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              UpdateAddress(_, _)).Times(1);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  controller()->EditClickedForSection(SECTION_SHIPPING);
  AcceptAndLoadFakeFingerprint();
}

// Tests that editing an instrument (CC + address) in wallet mode updates an
// existing instrument on the server via WalletClient.
TEST_F(AutofillDialogControllerTest, UpdateInstrument) {
  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());

  controller()->EditClickedForSection(SECTION_CC_BILLING);
  AcceptAndLoadFakeFingerprint();

  EXPECT_TRUE(
      controller()->GetTestingWalletClient()->updated_billing_address());
}

// Test that a user is able to edit their instrument and add a new address in
// the same submission.
TEST_F(AutofillDialogControllerTest, UpdateInstrumentSaveAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveAddress(_, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  controller()->EditClickedForSection(SECTION_CC_BILLING);
  AcceptAndLoadFakeFingerprint();

  EXPECT_TRUE(
      controller()->GetTestingWalletClient()->updated_billing_address());
}

// Test that saving a new instrument and editing an address works.
TEST_F(AutofillDialogControllerTest, SaveInstrumentUpdateAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrument(_, _, _)).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              UpdateAddress(_, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());

  controller()->OnDidGetWalletItems(wallet_items.Pass());

  controller()->EditClickedForSection(SECTION_SHIPPING);
  AcceptAndLoadFakeFingerprint();
}

MATCHER(UsesLocalBillingAddress, "uses the local billing address") {
  return arg.address_line_1() == ASCIIToUTF16(kEditedBillingAddress);
}

// Tests that when using billing address for shipping, and there is no exact
// matched shipping address, then a shipping address should be added.
TEST_F(AutofillDialogControllerTest, BillingForShipping) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveAddress(_, _)).Times(1);

  controller()->OnDidGetWalletItems(CompleteAndValidWalletItems());
  // Select "Same as billing" in the address menu.
  UseBillingForShipping();

  AcceptAndLoadFakeFingerprint();
}

// Tests that when using billing address for shipping, and there is an exact
// matched shipping address, then a shipping address should not be added.
TEST_F(AutofillDialogControllerTest, BillingForShippingHasMatch) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveAddress(_, _)).Times(0);

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

// Tests that adding new instrument and also using billing address for shipping,
// then a shipping address should not be added.
TEST_F(AutofillDialogControllerTest, BillingForShippingNewInstrument) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrumentAndAddress(_, _, _, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
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
              SaveAddress(UsesLocalBillingAddress(), _)).Times(1);
  AcceptAndLoadFakeFingerprint();

  EXPECT_TRUE(
      controller()->GetTestingWalletClient()->updated_billing_address());
}

TEST_F(AutofillDialogControllerTest, CancelNoSave) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrumentAndAddress(_, _, _, _)).Times(0);

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

TEST_F(AutofillDialogControllerTest, EditClickedCancelled) {
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
            controller()->SuggestionStateForSection(SECTION_EMAIL).text);

  // When edited, the initial_value should contain the value.
  controller()->EditClickedForSection(SECTION_EMAIL);
  const DetailInputs& inputs1 =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(kEmail, inputs1[0].initial_value);
  EXPECT_EQ(string16(),
            controller()->SuggestionStateForSection(SECTION_EMAIL).text);

  // When edit is cancelled, the initial_value should be empty.
  controller()->EditCancelledForSection(SECTION_EMAIL);
  const DetailInputs& inputs2 =
      controller()->RequestedFieldsForSection(SECTION_EMAIL);
  EXPECT_EQ(kEmail,
            controller()->SuggestionStateForSection(SECTION_EMAIL).text);
  EXPECT_EQ(string16(), inputs2[0].initial_value);
}

// Tests that editing an autofill profile and then submitting works.
TEST_F(AutofillDialogControllerTest, EditAutofillProfile) {
  SwitchToAutofill();

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
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
              edited_profile.GetInfo(input.type, "en-US"));
  }
}

// Tests that adding an autofill profile and then submitting works.
TEST_F(AutofillDialogControllerTest, AddAutofillProfile) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

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
    outputs[&input] = full_profile2.GetInfo(input.type, "en-US");
  }
  controller()->GetView()->SetUserInput(SECTION_BILLING, outputs);

  // Fill in some CC info. The name field will be used to fill in the billing
  // address name in the newly minted AutofillProfile.
  DetailOutputMap cc_outputs;
  const DetailInputs& cc_inputs =
      controller()->RequestedFieldsForSection(SECTION_CC);
  for (size_t i = 0; i < cc_inputs.size(); ++i) {
    cc_outputs[&cc_inputs[i]] = cc_inputs[i].type == CREDIT_CARD_NAME ?
        ASCIIToUTF16("Bill Money") : ASCIIToUTF16("111");
  }
  controller()->GetView()->SetUserInput(SECTION_CC, cc_outputs);

  controller()->OnAccept();
  const AutofillProfile& added_profile =
      controller()->GetTestingManager()->imported_profile();

  const DetailInputs& shipping_inputs =
      controller()->RequestedFieldsForSection(SECTION_SHIPPING);
  for (size_t i = 0; i < shipping_inputs.size(); ++i) {
    const DetailInput& input = shipping_inputs[i];
    string16 expected = input.type == NAME_FULL ?
        ASCIIToUTF16("Bill Money") :
        full_profile2.GetInfo(input.type, "en-US");
    EXPECT_EQ(expected, added_profile.GetInfo(input.type, "en-US"));
  }

  // Also, the currently selected email address should get added to the new
  // profile.
  string16 original_email = full_profile.GetInfo(EMAIL_ADDRESS, "en-US");
  EXPECT_FALSE(original_email.empty());
  EXPECT_EQ(original_email,
            added_profile.GetInfo(EMAIL_ADDRESS, "en-US"));
}

// Makes sure that a newly added email address gets added to an existing profile
// (as opposed to creating its own profile). http://crbug.com/240926
TEST_F(AutofillDialogControllerTest, AddEmail) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

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
  full_profile.GetMultiInfo(EMAIL_ADDRESS, "en-US", &email_values);
  ASSERT_EQ(2U, email_values.size());
  EXPECT_EQ(new_email, email_values[1]);
}

TEST_F(AutofillDialogControllerTest, VerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AuthenticateInstrument(_, _, _)).Times(1);

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
  controller()->OnDidSaveAddress(std::string(), required_actions, form_errors);
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

  EXPECT_CALL(*controller()->GetView(), UpdateForErrors()).Times(1);
  controller()->OnDidSaveAddress(std::string(), required_actions, form_errors);

  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
}

// Test Wallet banners are show in the right situations. These banners explain
// where Chrome got the user's data (i.e. "Got details from Wallet") or promote
// saving details into Wallet (i.e. "[x] Save details to Wallet").
TEST_F(AutofillDialogControllerTest, WalletBanners) {
  PrefService* prefs = controller()->profile()->GetPrefs();
  ASSERT_FALSE(prefs->GetBoolean(::prefs::kAutofillDialogHasPaidWithWallet));

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

TEST_F(AutofillDialogControllerTest, OnAutocheckoutError) {
  SwitchToAutofill();
  controller()->set_dialog_type(DIALOG_TYPE_AUTOCHECKOUT);

  // We also have to simulate CC inputs to keep the controller happy.
  FillCreditCardInputs();

  controller()->OnAccept();
  controller()->OnAutocheckoutError();

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::AUTOCHECKOUT_SUCCESS).size());
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::AUTOCHECKOUT_ERROR).size());
}

TEST_F(AutofillDialogControllerTest, OnAutocheckoutSuccess) {
  SwitchToAutofill();
  controller()->set_dialog_type(DIALOG_TYPE_AUTOCHECKOUT);

  // We also have to simulate CC inputs to keep the controller happy.
  FillCreditCardInputs();

  controller()->OnAccept();
  controller()->OnAutocheckoutSuccess();

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::AUTOCHECKOUT_SUCCESS).size());
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::AUTOCHECKOUT_ERROR).size());
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

  // Succesfully choosing wallet does set the pref.
  SetUpControllerWithFormData(DefaultFormData());

  SwitchToWallet();
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());

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

  size_t i = 0;
  for (; i < form_structure()->field_count(); ++i) {
    if (form_structure()->field(i)->type() == EMAIL_ADDRESS) {
      EXPECT_EQ(ASCIIToUTF16(kFakeEmail), form_structure()->field(i)->value);
      break;
    }
  }
  ASSERT_LT(i, form_structure()->field_count());
}

// Test if autofill types of returned form structure are correct for billing
// entries.
TEST_F(AutofillDialogControllerTest, AutofillTypes) {
  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());
  controller()->OnAccept();
  controller()->OnDidGetFullWallet(wallet::GetTestFullWallet());
  ASSERT_EQ(20U, form_structure()->field_count());
  EXPECT_EQ(EMAIL_ADDRESS, form_structure()->field(0)->type());
  EXPECT_EQ(CREDIT_CARD_NUMBER, form_structure()->field(2)->type());
  EXPECT_EQ(ADDRESS_BILLING_STATE, form_structure()->field(9)->type());
  EXPECT_EQ(ADDRESS_HOME_STATE, form_structure()->field(16)->type());
}

TEST_F(AutofillDialogControllerTest, SaveDetailsInChrome) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  AutofillProfile full_profile(test::GetVerifiedProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  CreditCard card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingCreditCard(&card);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->EditClickedForSection(SECTION_EMAIL);
  EXPECT_TRUE(controller()->ShouldOfferToSaveInChrome());

  controller()->EditCancelledForSection(SECTION_EMAIL);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());

  controller()->MenuModelForSection(SECTION_EMAIL)->ActivatedAt(1);
  EXPECT_TRUE(controller()->ShouldOfferToSaveInChrome());

  profile()->set_incognito(true);
  EXPECT_FALSE(controller()->ShouldOfferToSaveInChrome());
}

// Tests that user is prompted when using instrument with minimal address.
TEST_F(AutofillDialogControllerTest, UpgradeMinimalAddress) {
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

  // Use |SetOutputValue()| to put the right AutofillFieldTypes into the map.
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_CC_BILLING);
  DetailOutputMap outputs;
  SetOutputValue(inputs, &outputs, COMPANY_NAME, "Bluth Company");

  ValidityData validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(1U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input year differ from the instrument.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_4_DIGIT_YEAR, "3002");

  validity_data =
      controller()->InputsAreValid(SECTION_CC_BILLING, outputs, VALIDATE_EDIT);
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_MONTH));
  EXPECT_EQ(0U, validity_data.count(CREDIT_CARD_EXP_4_DIGIT_YEAR));

  // Make the local input month differ from the instrument.
  SetOutputValue(inputs, &outputs, CREDIT_CARD_EXP_MONTH, "06");

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

  controller()->OnAccept();
  EXPECT_EQ(0U, NotificationsOfType(
      DialogNotification::REQUIRED_ACTION).size());
}

// Make sure detailed steps for Autocheckout are added
// and updated correctly.
TEST_F(AutofillDialogControllerTest, DetailedSteps) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
            GetFullWallet(_)).Times(1);

  controller()->set_dialog_type(DIALOG_TYPE_AUTOCHECKOUT);

  // Add steps as would normally be done by the AutocheckoutManager.
  controller()->AddAutocheckoutStep(AUTOCHECKOUT_STEP_SHIPPING);
  controller()->AddAutocheckoutStep(AUTOCHECKOUT_STEP_DELIVERY);
  controller()->AddAutocheckoutStep(AUTOCHECKOUT_STEP_BILLING);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  // Initiate flow - should add proxy card step since the user is using wallet
  // data.
  controller()->OnAccept();
  controller()->OnDidLoadRiskFingerprintData(GetFakeFingerprint().Pass());

  SuggestionState suggestion_state =
      controller()->SuggestionStateForSection(SECTION_CC_BILLING);
  EXPECT_TRUE(suggestion_state.extra_text.empty());

  // There should be four steps total, with the first being the card generation
  // step added by the dialog controller.
  EXPECT_EQ(4U, controller()->CurrentAutocheckoutSteps().size());
  EXPECT_EQ(AUTOCHECKOUT_STEP_PROXY_CARD,
            controller()->CurrentAutocheckoutSteps()[0].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_STARTED,
            controller()->CurrentAutocheckoutSteps()[0].status());

  // Simulate a wallet error. This should remove the card generation step from
  // the flow, as we will have to proceed with local data.
  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  AutofillProfile shipping_profile(test::GetVerifiedProfile());
  AutofillProfile billing_profile(test::GetVerifiedProfile2());
  CreditCard credit_card(test::GetVerifiedCreditCard());
  controller()->GetTestingManager()->AddTestingProfile(&shipping_profile);
  controller()->GetTestingManager()->AddTestingProfile(&billing_profile);
  controller()->GetTestingManager()->AddTestingCreditCard(&credit_card);
  ui::MenuModel* billing_model =
      controller()->MenuModelForSection(SECTION_BILLING);
  billing_model->ActivatedAt(1);

  // Re-initiate flow.
  controller()->OnAccept();

  // All steps should be initially unstarted.
  EXPECT_EQ(3U, controller()->CurrentAutocheckoutSteps().size());
  EXPECT_EQ(AUTOCHECKOUT_STEP_SHIPPING,
            controller()->CurrentAutocheckoutSteps()[0].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_UNSTARTED,
            controller()->CurrentAutocheckoutSteps()[0].status());
  EXPECT_EQ(AUTOCHECKOUT_STEP_DELIVERY,
            controller()->CurrentAutocheckoutSteps()[1].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_UNSTARTED,
            controller()->CurrentAutocheckoutSteps()[1].status());
  EXPECT_EQ(AUTOCHECKOUT_STEP_BILLING,
            controller()->CurrentAutocheckoutSteps()[2].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_UNSTARTED,
            controller()->CurrentAutocheckoutSteps()[2].status());

  // Update steps in the same manner that we would expect to see from the
  // AutocheckoutManager while progressing through a flow.
  controller()->UpdateAutocheckoutStep(AUTOCHECKOUT_STEP_SHIPPING,
                                       AUTOCHECKOUT_STEP_STARTED);
  controller()->UpdateAutocheckoutStep(AUTOCHECKOUT_STEP_SHIPPING,
                                       AUTOCHECKOUT_STEP_COMPLETED);
  controller()->UpdateAutocheckoutStep(AUTOCHECKOUT_STEP_DELIVERY,
                                       AUTOCHECKOUT_STEP_STARTED);

  // Verify that the steps were appropriately updated.
  EXPECT_EQ(3U, controller()->CurrentAutocheckoutSteps().size());
  EXPECT_EQ(AUTOCHECKOUT_STEP_SHIPPING,
            controller()->CurrentAutocheckoutSteps()[0].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_COMPLETED,
            controller()->CurrentAutocheckoutSteps()[0].status());
  EXPECT_EQ(AUTOCHECKOUT_STEP_DELIVERY,
            controller()->CurrentAutocheckoutSteps()[1].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_STARTED,
            controller()->CurrentAutocheckoutSteps()[1].status());
  EXPECT_EQ(AUTOCHECKOUT_STEP_BILLING,
            controller()->CurrentAutocheckoutSteps()[2].type());
  EXPECT_EQ(AUTOCHECKOUT_STEP_UNSTARTED,
            controller()->CurrentAutocheckoutSteps()[2].status());
}

}  // namespace autofill
