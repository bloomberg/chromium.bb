// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/guid.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/prefs/pref_service.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/browser/autofill_common_test.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "components/autofill/browser/test_personal_data_manager.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/wallet_address.h"
#include "components/autofill/browser/wallet/wallet_client.h"
#include "components/autofill/browser/wallet/wallet_test_util.h"
#include "components/autofill/common/form_data.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_browser_thread.h"
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
const char* kFieldsFromPage[] = { "email", "cc-number" };
const char kSettingsOrigin[] = "Chrome settings";

using content::BrowserThread;

class TestAutofillDialogView : public AutofillDialogView {
 public:
  TestAutofillDialogView() {}
  virtual ~TestAutofillDialogView() {}

  virtual void Show() OVERRIDE {}
  virtual void Hide() OVERRIDE {}
  virtual void UpdateNotificationArea() OVERRIDE {}
  virtual void UpdateAccountChooser() OVERRIDE {}
  virtual void UpdateButtonStrip() OVERRIDE {}
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

 private:
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
        is_first_run_(true) {}
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

  void set_is_first_run(bool is_first_run) { is_first_run_ = is_first_run; }

  const GURL& open_tab_url() { return open_tab_url_; }

 protected:
  virtual PersonalDataManager* GetManager() OVERRIDE {
    return &test_manager_;
  }

  virtual wallet::WalletClient* GetWalletClient() OVERRIDE {
    return &test_wallet_client_;
  }

  virtual bool IsFirstRun() const OVERRIDE {
    return is_first_run_;
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
  bool is_first_run_;
  GURL open_tab_url_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillDialogController);
};

class AutofillDialogControllerTest : public testing::Test {
 public:
  AutofillDialogControllerTest()
    : ui_thread_(BrowserThread::UI, &loop_),
      file_thread_(BrowserThread::FILE),
      file_blocking_thread_(BrowserThread::FILE_USER_BLOCKING),
      io_thread_(BrowserThread::IO) {
    file_thread_.Start();
    file_blocking_thread_.Start();
    io_thread_.StartIOThread();
  }

  virtual ~AutofillDialogControllerTest() {}

  // testing::Test implementation:
  virtual void SetUp() OVERRIDE {
    FormData form_data;
    for (size_t i = 0; i < arraysize(kFieldsFromPage); ++i) {
      FormFieldData field;
      field.autocomplete_attribute = kFieldsFromPage[i];
      form_data.fields.push_back(field);
    }

    profile()->CreateRequestContext();
    test_web_contents_.reset(
        content::WebContentsTester::CreateTestWebContents(profile(), NULL));

    base::Callback<void(const FormStructure*, const std::string&)> callback =
        base::Bind(&AutofillDialogControllerTest::FinishedCallback,
                   base::Unretained(this));
    controller_ = (new TestAutofillDialogController(
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

  virtual void TearDown() OVERRIDE {
    if (controller_)
      controller_->ViewClosed();
  }

 protected:
  static scoped_ptr<wallet::FullWallet> CreateFullWalletWithVerifyCvv() {
    base::DictionaryValue dict;
    scoped_ptr<base::ListValue> list(new base::ListValue());
    list->AppendString("verify_cvv");
    dict.Set("required_action", list.release());
    return wallet::FullWallet::CreateFullWallet(dict);
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

  TestAutofillDialogController* controller() { return controller_; }

  TestingProfile* profile() { return &profile_; }

  const FormStructure* form_structure() { return form_structure_; }

 private:
  void FinishedCallback(const FormStructure* form_structure,
                        const std::string& google_transaction_id) {
    form_structure_ = form_structure;
  }

#if defined(OS_WIN)
   // http://crbug.com/227221
   ui::ScopedOleInitializer ole_initializer_;
#endif

  // A bunch of threads are necessary for classes like TestWebContents and
  // URLRequestContextGetter not to fall over.
  MessageLoopForUI loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  content::TestBrowserThread file_blocking_thread_;
  content::TestBrowserThread io_thread_;
  TestingProfile profile_;

  // The controller owns itself.
  base::WeakPtr<TestAutofillDialogController> controller_;

  scoped_ptr<content::WebContents> test_web_contents_;

  // Must outlive the controller.
  AutofillMetrics metric_logger_;

  // Returned when the dialog closes successfully.
  const FormStructure* form_structure_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogControllerTest);
};

}  // namespace

// This test makes sure nothing falls over when fields are being validity-
// checked.
TEST_F(AutofillDialogControllerTest, ValidityCheck) {
  const DialogSection sections[] = {
    SECTION_EMAIL,
    SECTION_CC,
    SECTION_BILLING,
    SECTION_CC_BILLING,
    SECTION_SHIPPING
  };

  for (size_t i = 0; i < arraysize(sections); ++i) {
    DialogSection section = sections[i];
    const DetailInputs& shipping_inputs =
        controller()->RequestedFieldsForSection(section);
    for (DetailInputs::const_iterator iter = shipping_inputs.begin();
         iter != shipping_inputs.end(); ++iter) {
      controller()->InputIsValid(iter->type, string16());
    }
  }
}

TEST_F(AutofillDialogControllerTest, AutofillProfiles) {
  ui::MenuModel* shipping_model =
      controller()->MenuModelForSection(SECTION_SHIPPING);
  // Since the PersonalDataManager is empty, this should only have the
  // "use billing", "add new" and "manage" menu items.
  EXPECT_EQ(3, shipping_model->GetItemCount());
  // On the other hand, the other models should be NULL when there's no
  // suggestion.
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_CC));
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_BILLING));
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_EMAIL));

  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  // Empty profiles are ignored.
  AutofillProfile empty_profile(base::GenerateGUID(), kSettingsOrigin);
  empty_profile.SetRawInfo(NAME_FULL, ASCIIToUTF16("John Doe"));
  controller()->GetTestingManager()->AddTestingProfile(&empty_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(3, shipping_model->GetItemCount());
  EXPECT_FALSE(controller()->MenuModelForSection(SECTION_EMAIL));

  // A full profile should be picked up.
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  shipping_model = controller()->MenuModelForSection(SECTION_SHIPPING);
  EXPECT_EQ(4, shipping_model->GetItemCount());
  EXPECT_TRUE(!!controller()->MenuModelForSection(SECTION_EMAIL));
}

TEST_F(AutofillDialogControllerTest, AutofillProfileVariants) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  ui::MenuModel* email_model =
      controller()->MenuModelForSection(SECTION_EMAIL);
  EXPECT_FALSE(email_model);

  // Set up some variant data.
  AutofillProfile full_profile(test::GetFullProfile());
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
}

TEST_F(AutofillDialogControllerTest, AcceptLegalDocuments) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AcceptLegalDocuments(_, _, _)).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddLegalDocument(wallet::GetTestLegalDocument());
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
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
  // "use billing", "add", "manage", and 5 suggestions.
  EXPECT_EQ(8,
      controller()->MenuModelForSection(SECTION_SHIPPING)->GetItemCount());
  EXPECT_TRUE(controller()->MenuModelForSection(SECTION_SHIPPING)->
      IsItemCheckedAt(4));
}

TEST_F(AutofillDialogControllerTest, SaveAddress) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveAddress(_, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
}

TEST_F(AutofillDialogControllerTest, SaveInstrument) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrument(_, _, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
}

TEST_F(AutofillDialogControllerTest, SaveInstrumentAndAddress) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              SaveInstrumentAndAddress(_, _, _, _)).Times(1);

  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());
  controller()->OnAccept();
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
  AutofillProfile full_profile(test::GetFullProfile());
  full_profile.set_origin(kSettingsOrigin);
  full_profile.SetRawInfo(ADDRESS_HOME_LINE2, string16());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);
  SwitchToAutofill();

  SuggestionsMenuModel* model = static_cast<SuggestionsMenuModel*>(
      controller()->MenuModelForSection(SECTION_SHIPPING));
  model->ExecuteCommand(model->GetItemCount() - 1, 0);
  GURL autofill_manage_url = controller()->open_tab_url();
  EXPECT_EQ("chrome", autofill_manage_url.scheme());

  SwitchToWallet();
  controller()->OnDidGetWalletItems(wallet::GetTestWalletItems());
  controller()->SuggestionItemSelected(model, model->GetItemCount() - 1);
  GURL wallet_manage_url = controller()->open_tab_url();
  EXPECT_EQ("https", wallet_manage_url.scheme());

  EXPECT_NE(autofill_manage_url, wallet_manage_url);
}

TEST_F(AutofillDialogControllerTest, EditClickedCancelled) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(1);

  AutofillProfile full_profile(test::GetFullProfile());
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

  AutofillProfile full_profile(test::GetFullProfile());
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

  AutofillProfile full_profile(test::GetFullProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  ui::MenuModel* model = controller()->MenuModelForSection(SECTION_BILLING);
  // Activate the "Add billing address" menu item.
  model->ActivatedAt(model->GetItemCount() - 2);

  // Fill in the inputs from the profile.
  DetailOutputMap outputs;
  const DetailInputs& inputs =
      controller()->RequestedFieldsForSection(SECTION_BILLING);
  AutofillProfile full_profile2(test::GetFullProfile2());
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
}

TEST_F(AutofillDialogControllerTest, VerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              AuthenticateInstrument(_, _, _)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();

  EXPECT_TRUE(NotificationsOfType(DialogNotification::REQUIRED_ACTION).empty());
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_SHIPPING));
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_CC_BILLING));
  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SuggestionState suggestion_state =
      controller()->SuggestionStateForSection(SECTION_CC_BILLING);
  EXPECT_TRUE(suggestion_state.extra_text.empty());

  controller()->OnDidGetFullWallet(CreateFullWalletWithVerifyCvv());

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

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();

  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// TODO(dbeam): disallow changing accounts instead and remove this test.
TEST_F(AutofillDialogControllerTest, ChangeAccountDuringSubmit) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();

  EXPECT_FALSE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SwitchToWallet();
  SwitchToAutofill();

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

TEST_F(AutofillDialogControllerTest, ErrorDuringVerifyCvv) {
  EXPECT_CALL(*controller()->GetTestingWalletClient(),
              GetFullWallet(_)).Times(1);

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
  controller()->OnDidGetFullWallet(CreateFullWalletWithVerifyCvv());

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

  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());
  controller()->OnAccept();
  controller()->OnDidGetFullWallet(CreateFullWalletWithVerifyCvv());

  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  ASSERT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));

  SwitchToWallet();
  SwitchToAutofill();

  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_OK));
  EXPECT_TRUE(controller()->IsDialogButtonEnabled(ui::DIALOG_BUTTON_CANCEL));
}

// Test that when a wallet error happens only an error is shown (and no other
// Wallet-related notifications).
TEST_F(AutofillDialogControllerTest, WalletErrorNotification) {
  controller()->OnWalletError(wallet::WalletClient::UNKNOWN_ERROR);

  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::WALLET_ERROR).size());

  // No other wallet notifications should show on Wallet error.
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::WALLET_SIGNIN_PROMO).empty());
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).empty());
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).empty());
}

// Test that only on first run an explanation of where Chrome got the user's
// data is shown (i.e. "Got these details from Wallet").
TEST_F(AutofillDialogControllerTest, WalletDetailsExplanation) {
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  EXPECT_EQ(1U, NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).size());

  // Wallet notifications are mutually exclusive.
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).empty());
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::WALLET_SIGNIN_PROMO).empty());

  // Switch to using Autofill, no explanatory message should show.
  SwitchToAutofill();
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).empty());

  // Switch to Wallet, pretend this isn't first run. No message should show.
  SwitchToWallet();
  controller()->set_is_first_run(false);
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).empty());
}

// Verifies that the "[X] Save details in wallet" notification shows on first
// run with an incomplete profile, stays showing when switching to Autofill in
// the account chooser, and continues to show on second+ run when a user's
// wallet is incomplete. This also tests that submitting disables interactivity.
TEST_F(AutofillDialogControllerTest, SaveDetailsInWallet) {
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  std::vector<DialogNotification> notifications =
      NotificationsOfType(DialogNotification::WALLET_USAGE_CONFIRMATION);
  EXPECT_EQ(1U, notifications.size());
  EXPECT_TRUE(notifications.front().checked());
  EXPECT_TRUE(notifications.front().interactive());

  // Wallet notifications are mutually exclusive.
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::WALLET_SIGNIN_PROMO).empty());
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).empty());

  // Using Autofill on second run, show an interactive, unchecked checkbox.
  SwitchToAutofill();
  controller()->set_is_first_run(false);

  notifications =
      NotificationsOfType(DialogNotification::WALLET_USAGE_CONFIRMATION);
  EXPECT_EQ(1U, notifications.size());
  EXPECT_FALSE(notifications.front().checked());
  EXPECT_TRUE(notifications.front().interactive());

  // Notifications shouldn't be interactive while submitting.
  SwitchToWallet();
  controller()->OnAccept();
  EXPECT_FALSE(NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).front().interactive());
}

// Verifies that no Wallet notifications are shown after first run (i.e. no
// "[X] Save details to wallet" or "These details are from your Wallet") when
// the user has a complete wallet.
TEST_F(AutofillDialogControllerTest, NoWalletNotifications) {
  controller()->set_is_first_run(false);

  // Simulate a complete wallet.
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());
  controller()->OnDidGetWalletItems(wallet_items.Pass());

  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::EXPLANATORY_MESSAGE).empty());
  EXPECT_TRUE(NotificationsOfType(
      DialogNotification::WALLET_USAGE_CONFIRMATION).empty());
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

TEST_F(AutofillDialogControllerTest, ViewSubmitSetsPref) {
  ASSERT_FALSE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));

  SwitchToAutofill();

  // We also have to simulate CC inputs to keep the controller happy.
  FillCreditCardInputs();

  controller()->OnAccept();

  EXPECT_TRUE(profile()->GetPrefs()->HasPrefPath(
      ::prefs::kAutofillDialogPayWithoutWallet));
  EXPECT_TRUE(profile()->GetPrefs()->GetBoolean(
      ::prefs::kAutofillDialogPayWithoutWallet));
}

TEST_F(AutofillDialogControllerTest, HideWalletEmail) {
  SwitchToAutofill();

  // Email section should be showing when using Autofill.
  EXPECT_TRUE(controller()->SectionIsActive(SECTION_EMAIL));

  SwitchToWallet();

  // Setup some wallet state, submit, and get a full wallet to end the flow.
  scoped_ptr<wallet::WalletItems> wallet_items = wallet::GetTestWalletItems();
  wallet_items->AddInstrument(wallet::GetTestMaskedInstrument());
  wallet_items->AddAddress(wallet::GetTestShippingAddress());

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

TEST_F(AutofillDialogControllerTest, SaveDetailsInChrome) {
  EXPECT_CALL(*controller()->GetView(), ModelChanged()).Times(2);

  AutofillProfile full_profile(test::GetFullProfile());
  controller()->GetTestingManager()->AddTestingProfile(&full_profile);

  CreditCard card;
  test::SetCreditCardInfo(
      &card, "Donald Trump", "4111-1111-1111-1111", "10", "2025");
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

}  // namespace autofill
