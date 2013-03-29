// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <algorithm>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_manager.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/personal_data_manager.h"
#include "components/autofill/browser/risk/fingerprint.h"
#include "components/autofill/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/browser/validation.h"
#include "components/autofill/browser/wallet/cart.h"
#include "components/autofill/browser/wallet/full_wallet.h"
#include "components/autofill/browser/wallet/instrument.h"
#include "components/autofill/browser/wallet/wallet_address.h"
#include "components/autofill/browser/wallet/wallet_items.h"
#include "components/autofill/browser/wallet/wallet_service_url.h"
#include "components/autofill/browser/wallet/wallet_signin_helper.h"
#include "components/autofill/common/form_data.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/webkit_resources.h"
#include "net/cert/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skbitmap_operations.h"

namespace autofill {

namespace {

const bool kPayWithoutWalletDefault = false;

// This is a pseudo-scientifically chosen maximum amount we want a fronting
// (proxy) card to be able to charge. The current actual max is $2000. Using
// only $1850 leaves some room for tax and shipping, etc. TODO(dbeam): send a
// special value to the server to just ask for the maximum so we don't need to
// hardcode it here (http://crbug.com/180731). TODO(dbeam): also maybe allow
// users to give us this number via an <input> (http://crbug.com/180733).
const int kCartMax = 1850;
const char kCartCurrency[] = "USD";

// Returns true if |input| should be shown when |field| has been requested.
bool InputTypeMatchesFieldType(const DetailInput& input,
                               const AutofillField& field) {
  // If any credit card expiration info is asked for, show both month and year
  // inputs.
  if (field.type() == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_MONTH) {
    return input.type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
           input.type == CREDIT_CARD_EXP_MONTH;
  }

  if (field.type() == CREDIT_CARD_TYPE)
    return input.type == CREDIT_CARD_NUMBER;

  return input.type == field.type();
}

// Returns true if |input| should be used for a site-requested |field|.
bool DetailInputMatchesField(const DetailInput& input,
                             const AutofillField& field) {
  bool right_section = !input.section_suffix ||
      EndsWith(field.section(), input.section_suffix, false);
  return InputTypeMatchesFieldType(input, field) && right_section;
}

bool IsCreditCardType(AutofillFieldType type) {
  return AutofillType(type).group() == AutofillType::CREDIT_CARD;
}

// Returns true if |input| should be used to fill a site-requested |field| which
// is notated with a "shipping" tag, for use when the user has decided to use
// the billing address as the shipping address.
bool DetailInputMatchesShippingField(const DetailInput& input,
                                     const AutofillField& field) {
  if (input.section_suffix &&
      std::string(input.section_suffix) == "billing") {
    return InputTypeMatchesFieldType(input, field);
  }

  if (field.type() == NAME_FULL)
    return input.type == CREDIT_CARD_NAME;

  return DetailInputMatchesField(input, field);
}

// Constructs |inputs| from template data.
void BuildInputs(const DetailInput* input_template,
                 size_t template_size,
                 DetailInputs* inputs) {
  for (size_t i = 0; i < template_size; ++i) {
    const DetailInput* input = &input_template[i];
    inputs->push_back(*input);
  }
}

// Uses |group| to fill in the |autofilled_value| for all inputs in |all_inputs|
// (an out-param).
void FillInputFromFormGroup(FormGroup* group, DetailInputs* inputs) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t j = 0; j < inputs->size(); ++j) {
    (*inputs)[j].autofilled_value =
        group->GetInfo((*inputs)[j].type, app_locale);
  }
}

// Initializes |form_group| from user-entered data.
void FillFormGroupFromOutputs(const DetailOutputMap& detail_outputs,
                              FormGroup* form_group) {
  for (DetailOutputMap::const_iterator iter = detail_outputs.begin();
       iter != detail_outputs.end(); ++iter) {
    if (!iter->second.empty())
      form_group->SetRawInfo(iter->first->type, iter->second);
  }
}

// Get billing info from |output| and put it into |card|, |cvc|, and |profile|.
// These outparams are required because |card|/|profile| accept different types
// of raw info, and CreditCard doesn't save CVCs.
void GetBillingInfoFromOutputs(const DetailOutputMap& output,
                               CreditCard* card,
                               string16* cvc,
                               AutofillProfile* profile) {
  for (DetailOutputMap::const_iterator it = output.begin();
       it != output.end(); ++it) {
    string16 trimmed;
    TrimWhitespace(it->second, TRIM_ALL, &trimmed);

    // Special case CVC as CreditCard just swallows it.
    if (it->first->type == CREDIT_CARD_VERIFICATION_CODE) {
      cvc->assign(trimmed);
    } else {
      // Copy the credit card name to |profile| in addition to |card| as
      // wallet::Instrument requires a recipient name for its billing address.
      if (it->first->type == CREDIT_CARD_NAME)
        profile->SetRawInfo(NAME_FULL, trimmed);

      if (IsCreditCardType(it->first->type))
        card->SetRawInfo(it->first->type, trimmed);
      else
        profile->SetRawInfo(it->first->type, trimmed);
    }
  }
}

// Returns the containing window for the given |web_contents|.  The containing
// window might be a browser window for a Chrome tab, or it might be a shell
// window for a platform app.
BaseWindow* GetBaseWindowForWebContents(
    const content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    return browser->window();

  gfx::NativeWindow native_window =
      web_contents->GetView()->GetTopLevelNativeWindow();
  ShellWindow* shell_window =
      extensions::ShellWindowRegistry::
          GetShellWindowForNativeWindowAnyProfile(native_window);
  return shell_window->GetBaseWindow();
}

// Extracts the string value of a field with |type| from |output|. This is
// useful when you only need the value of 1 input from a section of view inputs.
string16 GetValueForType(const DetailOutputMap& output,
                         AutofillFieldType type) {
  for (DetailOutputMap::const_iterator it = output.begin();
       it != output.end(); ++it) {
    if (it->first->type == type)
      return it->second;
  }
  NOTREACHED();
  return string16();
}

}  // namespace

AutofillDialogController::~AutofillDialogController() {}

AutofillDialogControllerImpl::AutofillDialogControllerImpl(
    content::WebContents* contents,
    const FormData& form,
    const GURL& source_url,
    const AutofillMetrics& metric_logger,
    DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback)
    : profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      contents_(contents),
      form_structure_(form, std::string()),
      invoked_from_same_origin_(true),
      source_url_(source_url),
      ssl_status_(form.ssl_status),
      callback_(callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          account_chooser_model_(this, profile_->GetPrefs())),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          wallet_client_(profile_->GetRequestContext(), this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_email_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_cc_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_billing_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_cc_billing_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_shipping_(this)),
      section_showing_popup_(SECTION_BILLING),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      metric_logger_(metric_logger),
      initial_user_state_(AutofillMetrics::DIALOG_USER_STATE_UNKNOWN),
      dialog_type_(dialog_type),
      did_submit_(false),
      autocheckout_is_running_(false),
      had_autocheckout_error_(false) {
  // TODO(estade): remove duplicates from |form|?
}

AutofillDialogControllerImpl::~AutofillDialogControllerImpl() {
  if (popup_controller_)
    popup_controller_->Hide();

  metric_logger_.LogDialogInitialUserState(dialog_type_, initial_user_state_);
}

// static
void AutofillDialogControllerImpl::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAutofillDialogPayWithoutWallet,
                                kPayWithoutWalletDefault,
                                PrefRegistrySyncable::SYNCABLE_PREF);
}

void AutofillDialogControllerImpl::Show() {
  dialog_shown_timestamp_ = base::Time::Now();

  content::NavigationEntry* entry = contents_->GetController().GetActiveEntry();
  const GURL& active_url = entry ? entry->GetURL() : contents_->GetURL();
  invoked_from_same_origin_ = active_url.GetOrigin() == source_url_.GetOrigin();

  // Log any relevant security exceptions.
  metric_logger_.LogDialogSecurityMetric(
      dialog_type_,
      AutofillMetrics::SECURITY_METRIC_DIALOG_SHOWN);

  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    metric_logger_.LogDialogSecurityMetric(
        dialog_type_,
        AutofillMetrics::SECURITY_METRIC_CREDIT_CARD_OVER_HTTP);
  }

  if (!invoked_from_same_origin_) {
    metric_logger_.LogDialogSecurityMetric(
        dialog_type_,
        AutofillMetrics::SECURITY_METRIC_CROSS_ORIGIN_FRAME);
  }

  // Determine what field types should be included in the dialog.
  bool has_types = false;
  bool has_sections = false;
  form_structure_.ParseFieldTypesFromAutocompleteAttributes(&has_types,
                                                            &has_sections);
  // Fail if the author didn't specify autocomplete types.
  if (!has_types) {
    callback_.Run(NULL, std::string());
    delete this;
    return;
  }

  const DetailInput kEmailInputs[] = {
    { 1, EMAIL_ADDRESS, IDS_AUTOFILL_DIALOG_PLACEHOLDER_EMAIL },
  };

  const DetailInput kCCInputs[] = {
    { 2, CREDIT_CARD_NUMBER, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_NUMBER },
    { 3, CREDIT_CARD_EXP_MONTH },
    { 3, CREDIT_CARD_EXP_4_DIGIT_YEAR },
    { 3, CREDIT_CARD_VERIFICATION_CODE, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC },
    { 4, CREDIT_CARD_NAME, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CARDHOLDER_NAME },
  };

  const DetailInput kBillingInputs[] = {
    { 5, ADDRESS_HOME_LINE1, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_1,
      "billing" },
    { 6, ADDRESS_HOME_LINE2, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_2,
      "billing" },
    { 7, ADDRESS_HOME_CITY, IDS_AUTOFILL_DIALOG_PLACEHOLDER_LOCALITY,
      "billing" },
    // TODO(estade): state placeholder should depend on locale.
    { 8, ADDRESS_HOME_STATE, IDS_AUTOFILL_FIELD_LABEL_STATE, "billing" },
    { 8, ADDRESS_HOME_ZIP, IDS_AUTOFILL_DIALOG_PLACEHOLDER_POSTAL_CODE,
      "billing", 0.5 },
    // TODO(estade): this should have a default based on the locale.
    { 9, ADDRESS_HOME_COUNTRY, 0, "billing" },
    { 10, PHONE_HOME_WHOLE_NUMBER,
      IDS_AUTOFILL_DIALOG_PLACEHOLDER_PHONE_NUMBER, "billing" },
  };

  const DetailInput kShippingInputs[] = {
    { 11, NAME_FULL, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESSEE_NAME,
      "shipping" },
    { 12, ADDRESS_HOME_LINE1, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_1,
      "shipping" },
    { 13, ADDRESS_HOME_LINE2, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_2,
      "shipping" },
    { 14, ADDRESS_HOME_CITY, IDS_AUTOFILL_DIALOG_PLACEHOLDER_LOCALITY,
      "shipping" },
    { 15, ADDRESS_HOME_STATE, IDS_AUTOFILL_FIELD_LABEL_STATE, "shipping" },
    { 15, ADDRESS_HOME_ZIP, IDS_AUTOFILL_DIALOG_PLACEHOLDER_POSTAL_CODE,
      "shipping", 0.5 },
    { 16, ADDRESS_HOME_COUNTRY, 0, "shipping" },
    { 17, PHONE_HOME_WHOLE_NUMBER,
      IDS_AUTOFILL_DIALOG_PLACEHOLDER_PHONE_NUMBER, "shipping" },
  };

  BuildInputs(kEmailInputs,
              arraysize(kEmailInputs),
              &requested_email_fields_);

  BuildInputs(kCCInputs,
              arraysize(kCCInputs),
              &requested_cc_fields_);

  BuildInputs(kBillingInputs,
              arraysize(kBillingInputs),
              &requested_billing_fields_);

  BuildInputs(kCCInputs,
              arraysize(kCCInputs),
              &requested_cc_billing_fields_);
  BuildInputs(kBillingInputs,
              arraysize(kBillingInputs),
              &requested_cc_billing_fields_);

  BuildInputs(kShippingInputs,
              arraysize(kShippingInputs),
              &requested_shipping_fields_);

  GenerateSuggestionsModels();

  // TODO(estade): don't show the dialog if the site didn't specify the right
  // fields. First we must figure out what the "right" fields are.
  view_.reset(CreateView());
  view_->Show();
  GetManager()->AddObserver(this);

  // Try to see if the user is already signed-in.
  // If signed-in, fetch the user's Wallet data.
  // Otherwise, see if the user could be signed in passively.
  // TODO(aruslan): UMA metrics for sign-in.
  if (IsPayingWithWallet())
    StartFetchingWalletItems();
}

void AutofillDialogControllerImpl::Hide() {
  if (view_)
    view_->Hide();
}

void AutofillDialogControllerImpl::UpdateProgressBar(double value) {
  view_->UpdateProgressBar(value);
}

void AutofillDialogControllerImpl::OnAutocheckoutError() {
  had_autocheckout_error_ = true;
  autocheckout_is_running_ = false;
  view_->UpdateNotificationArea();
  view_->UpdateButtonStrip();
}

////////////////////////////////////////////////////////////////////////////////
// AutofillDialogController implementation.

string16 AutofillDialogControllerImpl::DialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

string16 AutofillDialogControllerImpl::EditSuggestionText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EDIT);
}

string16 AutofillDialogControllerImpl::UseBillingForShippingText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING);
}

string16 AutofillDialogControllerImpl::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

string16 AutofillDialogControllerImpl::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

string16 AutofillDialogControllerImpl::CancelSignInText() const {
  // TODO(abodenha): real strings and l10n.
  return ASCIIToUTF16("Don't sign in.");
}

string16 AutofillDialogControllerImpl::SaveLocallyText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_CHECKBOX);
}

string16 AutofillDialogControllerImpl::ProgressBarText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_AUTOCHECKOUT_PROGRESS_BAR);
}

string16 AutofillDialogControllerImpl::LegalDocumentsText() {
  EnsureLegalDocumentsText();
  return legal_documents_text_;
}

DialogSignedInState AutofillDialogControllerImpl::SignedInState() const {
  if (signin_helper_ || !wallet_items_)
    return REQUIRES_RESPONSE;

  if (wallet_items_->HasRequiredAction(wallet::GAIA_AUTH))
    return REQUIRES_SIGN_IN;

  if (wallet_items_->HasRequiredAction(wallet::PASSIVE_GAIA_AUTH))
    return REQUIRES_PASSIVE_SIGN_IN;

  return SIGNED_IN;
}

bool AutofillDialogControllerImpl::ShouldShowSpinner() const {
  return IsPayingWithWallet() && SignedInState() == REQUIRES_RESPONSE;
}

string16 AutofillDialogControllerImpl::AccountChooserText() const {
  if (!IsPayingWithWallet())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PAY_WITHOUT_WALLET);

  // TODO(dbeam): real strings and l10n.
  if (SignedInState() == SIGNED_IN)
    return ASCIIToUTF16(current_username_);

  // In this case, the account chooser should be showing the signin link.
  return string16();
}

string16 AutofillDialogControllerImpl::SignInLinkText() const {
  // TODO(estade): real strings and l10n.
  return ASCIIToUTF16("Sign in to use Google Wallet");
}

bool AutofillDialogControllerImpl::ShouldOfferToSaveInChrome() const {
  return !IsPayingWithWallet();
}

bool AutofillDialogControllerImpl::AutocheckoutIsRunning() const {
  return autocheckout_is_running_;
}

bool AutofillDialogControllerImpl::HadAutocheckoutError() const {
  return had_autocheckout_error_;
}

bool AutofillDialogControllerImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return !did_submit_;
  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  // TODO(ahutter): Make it possible for the user to cancel out of the dialog
  // while Autocheckout is in progress.
  return had_autocheckout_error_ || !did_submit_;
}

const std::vector<ui::Range>& AutofillDialogControllerImpl::
    LegalDocumentLinks() {
  EnsureLegalDocumentsText();
  return legal_document_link_ranges_;
}

bool AutofillDialogControllerImpl::SectionIsActive(DialogSection section)
    const {
  if (IsPayingWithWallet())
    return section != SECTION_BILLING && section != SECTION_CC;

  return section != SECTION_CC_BILLING;
}

bool AutofillDialogControllerImpl::HasCompleteWallet() const {
  return wallet_items_.get() != NULL &&
         !wallet_items_->instruments().empty() &&
         !wallet_items_->addresses().empty();
}

void AutofillDialogControllerImpl::StartFetchingWalletItems() {
  DCHECK(IsPayingWithWallet());
  // TODO(dbeam): Add Risk capabilites once the UI supports risk challenges.
  GetWalletClient()->GetWalletItems(
      source_url_,
      std::vector<wallet::WalletClient::RiskCapability>());
}

void AutofillDialogControllerImpl::OnWalletOrSigninUpdate() {
  if (wallet_items_.get()) {
    DCHECK(IsPayingWithWallet());
    DCHECK(!signin_helper_.get());
    switch (SignedInState()) {
      case SIGNED_IN:
        // Start fetching the user name if we don't know it yet.
        if (current_username_.empty()) {
          signin_helper_.reset(new wallet::WalletSigninHelper(
              this,
              profile_->GetRequestContext()));
          signin_helper_->StartUserNameFetch();
        }
        break;

      case REQUIRES_SIGN_IN:
        // TODO(aruslan): automatic sign-in?
        break;

      case REQUIRES_PASSIVE_SIGN_IN:
        // Attempt to passively sign in the user.
        current_username_.clear();
        signin_helper_.reset(new wallet::WalletSigninHelper(
            this,
            profile_->GetRequestContext()));
        signin_helper_->StartPassiveSignin();
        break;

      case REQUIRES_RESPONSE:
        NOTREACHED();
    }
  }

  GenerateSuggestionsModels();
  view_->ModelChanged();
  view_->UpdateAccountChooser();
  view_->UpdateNotificationArea();

  // On the first successful response, compute the initial user state metric.
  if (initial_user_state_ == AutofillMetrics::DIALOG_USER_STATE_UNKNOWN)
    initial_user_state_ = GetInitialUserState();
}

void AutofillDialogControllerImpl::EnsureLegalDocumentsText() {
  if (!wallet_items_ || wallet_items_->legal_documents().empty())
    return;

  // The text has already been constructed, no need to recompute.
  if (!legal_documents_text_.empty())
    return;

  const std::vector<wallet::WalletItems::LegalDocument*>& documents =
      wallet_items_->legal_documents();
  DCHECK_LE(documents.size(), 3U);
  DCHECK_GE(documents.size(), 2U);
  const bool new_user = wallet_items_->HasRequiredAction(wallet::SETUP_WALLET);

  const string16 privacy_policy_display_name =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PRIVACY_POLICY_LINK);
  string16 text;
  if (documents.size() == 2U) {
    text = l10n_util::GetStringFUTF16(
        new_user ? IDS_AUTOFILL_DIALOG_LEGAL_LINKS_NEW_2 :
                   IDS_AUTOFILL_DIALOG_LEGAL_LINKS_UPDATED_2,
        documents[0]->display_name(),
        documents[1]->display_name());
  } else {
    text = l10n_util::GetStringFUTF16(
        new_user ? IDS_AUTOFILL_DIALOG_LEGAL_LINKS_NEW_3 :
                   IDS_AUTOFILL_DIALOG_LEGAL_LINKS_UPDATED_3,
        documents[0]->display_name(),
        documents[1]->display_name(),
        documents[2]->display_name());
  }

  legal_document_link_ranges_.clear();
  for (size_t i = 0; i < documents.size(); ++i) {
    size_t link_start = text.find(documents[i]->display_name());
    legal_document_link_ranges_.push_back(ui::Range(
        link_start, link_start + documents[i]->display_name().size()));
  }
  legal_documents_text_ = text;
}

const DetailInputs& AutofillDialogControllerImpl::RequestedFieldsForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_EMAIL:
      return requested_email_fields_;
    case SECTION_CC:
      return requested_cc_fields_;
    case SECTION_BILLING:
      return requested_billing_fields_;
    case SECTION_CC_BILLING:
      return requested_cc_billing_fields_;
    case SECTION_SHIPPING:
      return requested_shipping_fields_;
  }

  NOTREACHED();
  return requested_billing_fields_;
}

ui::ComboboxModel* AutofillDialogControllerImpl::ComboboxModelForAutofillType(
    AutofillFieldType type) {
  switch (type) {
    case CREDIT_CARD_EXP_MONTH:
      return &cc_exp_month_combobox_model_;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return &cc_exp_year_combobox_model_;

    case ADDRESS_HOME_COUNTRY:
      return &country_combobox_model_;

    default:
      return NULL;
  }
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForSection(
    DialogSection section) {
  return SuggestionsMenuModelForSection(section);
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForAccountChooser() {
  // When paying with wallet, but not signed in, there is no menu, just a
  // sign in link.
  if (IsPayingWithWallet() && SignedInState() != SIGNED_IN)
    return NULL;

  return &account_chooser_model_;
}

gfx::Image AutofillDialogControllerImpl::AccountChooserImage() {
  if (!MenuModelForAccountChooser()) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_WALLET_ICON);
  }

  gfx::Image icon;
  account_chooser_model_.GetIconAt(account_chooser_model_.checked_item(),
                                   &icon);
  return icon;
}

string16 AutofillDialogControllerImpl::LabelForSection(DialogSection section)
    const {
  switch (section) {
    case SECTION_EMAIL:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_EMAIL);
    case SECTION_CC:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC);
    case SECTION_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_BILLING);
    case SECTION_CC_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC_BILLING);
    case SECTION_SHIPPING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_SHIPPING);
    default:
      NOTREACHED();
      return string16();
  }
}

string16 AutofillDialogControllerImpl::SuggestionTextForSection(
    DialogSection section) {
  // When the user has clicked 'edit', don't show a suggestion (even though
  // there is a profile selected in the model).
  if (section_editing_state_[section])
    return string16();

  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  if (item_key.empty())
    return string16();

  if (section == SECTION_EMAIL)
    return model->GetLabelAt(model->checked_item());

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  return wrapper->GetDisplayText();
}

scoped_ptr<DataModelWrapper> AutofillDialogControllerImpl::CreateWrapper(
    DialogSection section) {
  if (IsPayingWithWallet() && full_wallet_) {
    if (section == SECTION_CC_BILLING) {
      return scoped_ptr<DataModelWrapper>(
          new FullWalletBillingWrapper(full_wallet_.get()));
    }
    if (section == SECTION_SHIPPING) {
      return scoped_ptr<DataModelWrapper>(
          new FullWalletShippingWrapper(full_wallet_.get()));
    }
  }

  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  if (item_key.empty())
    return scoped_ptr<DataModelWrapper>();

  if (IsPayingWithWallet()) {
    int index;
    bool success = base::StringToInt(item_key, &index);
    DCHECK(success);

    if (section == SECTION_CC_BILLING) {
      return scoped_ptr<DataModelWrapper>(
          new WalletInstrumentWrapper(wallet_items_->instruments()[index]));
    }
    // TODO(dbeam): should SECTION_EMAIL get here?
    return scoped_ptr<DataModelWrapper>(
        new WalletAddressWrapper(wallet_items_->addresses()[index]));
  }

  if (section == SECTION_CC) {
    CreditCard* card = GetManager()->GetCreditCardByGUID(item_key);
    DCHECK(card);
    return scoped_ptr<DataModelWrapper>(new AutofillCreditCardWrapper(card));
  }

  // Calculate the variant by looking at how many items come from the same
  // FormGroup.
  size_t variant = 0;
  for (int i = model->checked_item() - 1; i >= 0; --i) {
    if (model->GetItemKeyAt(i) == item_key)
      variant++;
    else
      break;
  }

  AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
  DCHECK(profile);
  return scoped_ptr<DataModelWrapper>(
      new AutofillProfileWrapper(profile, variant));
}

gfx::Image AutofillDialogControllerImpl::SuggestionIconForSection(
    DialogSection section) {
  scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
  if (!model.get())
    return gfx::Image();

  return model->GetIcon();
}

void AutofillDialogControllerImpl::EditClickedForSection(
    DialogSection section) {
  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
  model->FillInputs(inputs);
  section_editing_state_[section] = true;
  view_->UpdateSection(section);
}

void AutofillDialogControllerImpl::EditCancelledForSection(
    DialogSection section) {
  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  for (size_t i = 0; i < inputs->size(); ++i)
    (*inputs)[i].autofilled_value.clear();
  section_editing_state_[section] = false;
  view_->UpdateSection(section);
}

gfx::Image AutofillDialogControllerImpl::IconForField(
    AutofillFieldType type, const string16& user_input) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (type == CREDIT_CARD_VERIFICATION_CODE)
    return rb.GetImageNamed(IDR_CREDIT_CARD_CVC_HINT);

  // For the credit card, we show a few grayscale images, and possibly one
  // color image if |user_input| is a valid card number.
  if (type == CREDIT_CARD_NUMBER) {
    const int card_idrs[] = {
      IDR_AUTOFILL_CC_VISA,
      IDR_AUTOFILL_CC_MASTERCARD,
      IDR_AUTOFILL_CC_AMEX,
      IDR_AUTOFILL_CC_DISCOVER
    };
    const int number_of_cards = arraysize(card_idrs);
    // The number of pixels between card icons.
    const int kCardPadding = 2;

    gfx::ImageSkia some_card = *rb.GetImageSkiaNamed(card_idrs[0]);
    const int card_width = some_card.width();
    gfx::Canvas canvas(
        gfx::Size((card_width + kCardPadding) * number_of_cards - kCardPadding,
                  some_card.height()),
        ui::SCALE_FACTOR_100P,
        true);
    CreditCard card;
    card.SetRawInfo(CREDIT_CARD_NUMBER, user_input);

    for (int i = 0; i < number_of_cards; ++i) {
      int idr = card_idrs[i];
      gfx::ImageSkia card_image = *rb.GetImageSkiaNamed(idr);
      if (card.IconResourceId() != idr) {
        color_utils::HSL shift = {-1, 0, 0.8};
        SkBitmap disabled_bitmap =
            SkBitmapOperations::CreateHSLShiftedBitmap(*card_image.bitmap(),
                                                       shift);
        card_image = gfx::ImageSkia::CreateFrom1xBitmap(disabled_bitmap);
      }

      canvas.DrawImageInt(card_image, i * (card_width + kCardPadding), 0);
    }

    gfx::ImageSkia skia(canvas.ExtractImageRep());
    return gfx::Image(skia);
  }

  return gfx::Image();
}

bool AutofillDialogControllerImpl::InputIsValid(AutofillFieldType type,
                                                const string16& value) {
  switch (type) {
    case EMAIL_ADDRESS:
      return IsValidEmailAddress(value);

    case CREDIT_CARD_NUMBER:
      return autofill::IsValidCreditCardNumber(value);
    case CREDIT_CARD_NAME:
      break;
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      break;
    case CREDIT_CARD_VERIFICATION_CODE:
      return autofill::IsValidCreditCardSecurityCode(value);

    case ADDRESS_HOME_LINE1:
      break;
    case ADDRESS_HOME_LINE2:
      return true;  // Line 2 is optional - always valid.
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
      break;

    case NAME_FULL:  // Used for shipping.
      break;

    case PHONE_HOME_WHOLE_NUMBER:  // Used in billing section.
      // TODO(dbeam): validate with libphonenumber.
      break;

    default:
      NOTREACHED();  // Trying to validate unknown field.
      break;
  }

  return !value.empty();
}

std::vector<AutofillFieldType> AutofillDialogControllerImpl::InputsAreValid(
    const DetailOutputMap& inputs, ValidationType validation_type) {
  std::vector<AutofillFieldType> invalid_fields;
  std::map<AutofillFieldType, string16> field_values;
  for (DetailOutputMap::const_iterator iter = inputs.begin();
       iter != inputs.end(); ++iter) {
    // Skip empty fields in edit mode.
    if (validation_type == VALIDATE_EDIT && iter->second.empty())
      continue;

    field_values[iter->first->type] = iter->second;

    if (!InputIsValid(iter->first->type, iter->second))
      invalid_fields.push_back(iter->first->type);
  }

  // Validate the date formed by month and year field. (Autofill dialog is
  // never supposed to have 2-digit years, so not checked).
  if (field_values.count(CREDIT_CARD_EXP_MONTH) &&
      field_values.count(CREDIT_CARD_EXP_4_DIGIT_YEAR)) {
    if (!autofill::IsValidCreditCardExpirationDate(
            field_values[CREDIT_CARD_EXP_4_DIGIT_YEAR],
            field_values[CREDIT_CARD_EXP_MONTH],
            base::Time::Now())) {
      invalid_fields.push_back(CREDIT_CARD_EXP_MONTH);
      invalid_fields.push_back(CREDIT_CARD_EXP_4_DIGIT_YEAR);
    }
  }

  // If there is a credit card number and a CVC, validate them together.
  if (field_values.count(CREDIT_CARD_NUMBER) &&
      field_values.count(CREDIT_CARD_VERIFICATION_CODE) &&
      InputIsValid(CREDIT_CARD_NUMBER, field_values[CREDIT_CARD_NUMBER])) {
    if (!autofill::IsValidCreditCardSecurityCode(
            field_values[CREDIT_CARD_VERIFICATION_CODE],
            field_values[CREDIT_CARD_NUMBER])) {
      invalid_fields.push_back(CREDIT_CARD_VERIFICATION_CODE);
    }
  }

  // De-duplicate invalid fields.
  std::sort(invalid_fields.begin(), invalid_fields.end());
  invalid_fields.erase(std::unique(
      invalid_fields.begin(), invalid_fields.end()), invalid_fields.end());

  return invalid_fields;
}

void AutofillDialogControllerImpl::UserEditedOrActivatedInput(
    const DetailInput* input,
    DialogSection section,
    gfx::NativeView parent_view,
    const gfx::Rect& content_bounds,
    const string16& field_contents,
    bool was_edit) {
  // If the field is edited down to empty, don't show a popup.
  if (was_edit && field_contents.empty()) {
    HidePopup();
    return;
  }

  // If the user clicks while the popup is already showing, be sure to hide
  // it.
  if (!was_edit && popup_controller_) {
    HidePopup();
    return;
  }

  std::vector<string16> popup_values, popup_labels, popup_icons;
  if (section == SECTION_CC) {
    GetManager()->GetCreditCardSuggestions(input->type,
                                           field_contents,
                                           &popup_values,
                                           &popup_labels,
                                           &popup_icons,
                                           &popup_guids_);
  } else {
    // TODO(estade): add field types from email section?
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    std::vector<AutofillFieldType> field_types;
    field_types.reserve(inputs.size());
    for (DetailInputs::const_iterator iter = inputs.begin();
         iter != inputs.end(); ++iter) {
      field_types.push_back(iter->type);
    }
    GetManager()->GetProfileSuggestions(input->type,
                                        field_contents,
                                        false,
                                        field_types,
                                        &popup_values,
                                        &popup_labels,
                                        &popup_icons,
                                        &popup_guids_);
  }

  if (popup_values.empty())
    return;

  // TODO(estade): do we need separators and control rows like 'Clear
  // Form'?
  std::vector<int> popup_ids;
  for (size_t i = 0; i < popup_guids_.size(); ++i) {
    popup_ids.push_back(i);
  }

  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_, this, parent_view, content_bounds);
  popup_controller_->Show(popup_values,
                          popup_labels,
                          popup_icons,
                          popup_ids);
  section_showing_popup_ = section;
}

void AutofillDialogControllerImpl::FocusMoved() {
  HidePopup();
}

void AutofillDialogControllerImpl::ViewClosed() {
  GetManager()->RemoveObserver(this);

  if (autocheckout_is_running_ || had_autocheckout_error_) {
    AutofillMetrics::AutocheckoutCompletionStatus metric =
        autocheckout_is_running_ ?
            AutofillMetrics::AUTOCHECKOUT_SUCCEEDED :
            AutofillMetrics::AUTOCHECKOUT_FAILED;
    metric_logger_.LogAutocheckoutDuration(
        base::Time::Now() - autocheckout_started_timestamp_,
        metric);
  }

  // On a successful submit, save the payment type for next time. If there
  // was a Wallet server error, try to pay with Wallet again next time.
  // Reset the view so that updates to the pref aren't processed.
  view_.reset();
  bool pay_without_wallet = !IsPayingWithWallet() &&
      !account_chooser_model_.had_wallet_error();
  profile_->GetPrefs()->SetBoolean(prefs::kAutofillDialogPayWithoutWallet,
                                   pay_without_wallet);

  delete this;
}

std::vector<DialogNotification>
    AutofillDialogControllerImpl::CurrentNotifications() const {
  std::vector<DialogNotification> notifications;

  if (IsPayingWithWallet()) {
    // TODO(dbeam): what about REQUIRES_PASSIVE_SIGN_IN?
    if (SignedInState() == SIGNED_IN) {
      // On first run with a complete wallet profile, show a notification
      // explaining where this data came from.
      if (IsFirstRun() && HasCompleteWallet()) {
        notifications.push_back(
            DialogNotification(
                DialogNotification::EXPLANATORY_MESSAGE,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_DIALOG_DETAILS_FROM_WALLET)));
      } else {
        notifications.push_back(
            DialogNotification(
                DialogNotification::WALLET_PROMO,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_DIALOG_SAVE_DETAILS_IN_WALLET)));
      }
    } else if (IsFirstRun()) {
      // If the user is not signed in, show an upsell notification on first run.
      notifications.push_back(
          DialogNotification(
              DialogNotification::WALLET_PROMO,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_DIALOG_SIGN_IN_AND_SAVE_DETAILS)));
    }
  }

  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    notifications.push_back(
        DialogNotification(
            DialogNotification::SECURITY_WARNING,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECURITY_WARNING)));
  }

  if (!invoked_from_same_origin_) {
    notifications.push_back(
        DialogNotification(
            DialogNotification::SECURITY_WARNING,
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_DIALOG_SITE_WARNING,
                UTF8ToUTF16(source_url_.host()))));
  }

  if (had_autocheckout_error_) {
    notifications.push_back(
        DialogNotification(
            DialogNotification::AUTOCHECKOUT_ERROR,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_AUTOCHECKOUT_ERROR)));
  }

  if (account_chooser_model_.had_wallet_error()) {
    // TODO(dbeam): pass along the Wallet error or remove from the translation.
    // TODO(dbeam): figure out a way to dismiss this error after a while.
    notifications.push_back(DialogNotification(
        DialogNotification::WALLET_ERROR,
        l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_DIALOG_COMPLETE_WITHOUT_WALLET,
            ASCIIToUTF16("Oops, [Wallet-Error]."))));
  }

  return notifications;
}

void AutofillDialogControllerImpl::StartSignInFlow() {
  DCHECK(registrar_.IsEmpty());

  content::Source<content::NavigationController> source(view_->ShowSignIn());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);
}

void AutofillDialogControllerImpl::EndSignInFlow() {
  DCHECK(!registrar_.IsEmpty());
  registrar_.RemoveAll();
  view_->HideSignIn();
}

void AutofillDialogControllerImpl::LegalDocumentLinkClicked(
    const ui::Range& range) {
#if !defined(OS_ANDROID)
  for (size_t i = 0; i < legal_document_link_ranges_.size(); ++i) {
    if (legal_document_link_ranges_[i] == range) {
      chrome::NavigateParams params(
          chrome::FindBrowserWithWebContents(web_contents()),
          wallet_items_->legal_documents()[i]->url(),
          content::PAGE_TRANSITION_AUTO_BOOKMARK);
      params.disposition = NEW_BACKGROUND_TAB;
      chrome::Navigate(&params);
      return;
    }
  }

  NOTREACHED();
#else
  // TODO(estade): use TabModelList?
#endif
}

void AutofillDialogControllerImpl::OnCancel() {
  if (!did_submit_) {
    metric_logger_.LogDialogUiDuration(
        base::Time::Now() - dialog_shown_timestamp_,
        dialog_type_,
        AutofillMetrics::DIALOG_CANCELED);
  }

  // If Autocheckout has an error, it's possible that the dialog will be
  // submitted to start the flow and then cancelled to close the dialog after
  // the error.
  if (!callback_.is_null()) {
    callback_.Run(NULL, std::string());
    callback_ = base::Callback<void(const FormStructure*,
                                    const std::string&)>();
  }
}

void AutofillDialogControllerImpl::OnSubmit() {
  did_submit_ = true;
  metric_logger_.LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      dialog_type_,
      AutofillMetrics::DIALOG_ACCEPTED);

  if (dialog_type_ == DIALOG_TYPE_AUTOCHECKOUT) {
    // Stop observing PersonalDataManager to avoid the dialog redrawing while
    // in an Autocheckout flow.
    GetManager()->RemoveObserver(this);
    autocheckout_is_running_ = true;
    autocheckout_started_timestamp_ = base::Time::Now();
    view_->UpdateButtonStrip();
  }

  if (IsPayingWithWallet())
    SubmitWithWallet();
  else
    FinishSubmit();
}

Profile* AutofillDialogControllerImpl::profile() {
  return profile_;
}

content::WebContents* AutofillDialogControllerImpl::web_contents() {
  return contents_;
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPopupDelegate implementation.

void AutofillDialogControllerImpl::OnPopupShown(
    content::KeyboardListener* listener) {
  metric_logger_.LogDialogPopupEvent(
      dialog_type_, AutofillMetrics::DIALOG_POPUP_SHOWN);
}

void AutofillDialogControllerImpl::OnPopupHidden(
    content::KeyboardListener* listener) {}

void AutofillDialogControllerImpl::DidSelectSuggestion(int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::DidAcceptSuggestion(const string16& value,
                                                       int identifier) {
  const PersonalDataManager::GUIDPair& pair = popup_guids_[identifier];

  FormGroup* form_group = section_showing_popup_ == SECTION_CC ?
      static_cast<FormGroup*>(GetManager()->GetCreditCardByGUID(pair.first)) :
      // TODO(estade): need to use the variant, |pair.second|.
      static_cast<FormGroup*>(GetManager()->GetProfileByGUID(pair.first));
  DCHECK(form_group);

  FillInputFromFormGroup(
      form_group,
      MutableRequestedFieldsForSection(section_showing_popup_));
  view_->UpdateSection(section_showing_popup_);

  metric_logger_.LogDialogPopupEvent(
      dialog_type_, AutofillMetrics::DIALOG_POPUP_FORM_FILLED);

  // TODO(estade): not sure why it's necessary to do this explicitly.
  HidePopup();
}

void AutofillDialogControllerImpl::RemoveSuggestion(const string16& value,
                                                    int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::ClearPreviewedForm() {
  // TODO(estade): implement.
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver implementation.

void AutofillDialogControllerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_NAV_ENTRY_COMMITTED);
  content::LoadCommittedDetails* load_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  if (wallet::IsSignInContinueUrl(load_details->entry->GetVirtualURL())) {
    EndSignInFlow();
    if (IsPayingWithWallet())
      StartFetchingWalletItems();
  }
}

////////////////////////////////////////////////////////////////////////////////
// SuggestionsMenuModelDelegate implementation.

void AutofillDialogControllerImpl::SuggestionItemSelected(
    const SuggestionsMenuModel& model) {
  const DialogSection section = SectionForSuggestionsMenuModel(model);
  EditCancelledForSection(section);
}

////////////////////////////////////////////////////////////////////////////////
// wallet::WalletClientDelegate implementation.

const AutofillMetrics& AutofillDialogControllerImpl::GetMetricLogger() const {
  return metric_logger_;
}

DialogType AutofillDialogControllerImpl::GetDialogType() const {
  return dialog_type_;
}

std::string AutofillDialogControllerImpl::GetRiskData() const {
  // TODO(dbeam): Implement this.
  return "risky business";
}

void AutofillDialogControllerImpl::OnDidAcceptLegalDocuments() {
  // TODO(dbeam): Don't send risk params until legal documents are accepted:
  // http://crbug.com/173505
}

void AutofillDialogControllerImpl::OnDidAuthenticateInstrument(bool success) {
  // TODO(dbeam): use the returned full wallet. b/8332329
  if (success)
    GetFullWallet();
  else
    DisableWallet();
}

void AutofillDialogControllerImpl::OnDidGetFullWallet(
    scoped_ptr<wallet::FullWallet> full_wallet) {
  // TODO(dbeam): handle more required actions.
  full_wallet_ = full_wallet.Pass();

  if (full_wallet_->HasRequiredAction(wallet::VERIFY_CVV))
    DisableWallet();
  else
    FinishSubmit();
}

void AutofillDialogControllerImpl::OnPassiveSigninSuccess(
    const std::string& username) {
  DCHECK(IsPayingWithWallet());
  current_username_ = username;
  signin_helper_.reset();
  wallet_items_.reset();
  StartFetchingWalletItems();
}

void AutofillDialogControllerImpl::OnUserNameFetchSuccess(
    const std::string& username) {
  DCHECK(IsPayingWithWallet());
  current_username_ = username;
  signin_helper_.reset();
  OnWalletOrSigninUpdate();
}

void AutofillDialogControllerImpl::OnAutomaticSigninSuccess(
    const std::string& username) {
  // TODO(aruslan): automatic sign-in.
  NOTIMPLEMENTED();
}

void AutofillDialogControllerImpl::OnPassiveSigninFailure(
    const GoogleServiceAuthError& error) {
  // TODO(aruslan): report an error.
  LOG(ERROR) << "failed to passively sign-in: " << error.ToString();
  OnWalletSigninError();
}

void AutofillDialogControllerImpl::OnUserNameFetchFailure(
    const GoogleServiceAuthError& error) {
  // TODO(aruslan): report an error.
  LOG(ERROR) << "failed to fetch the user account name: " << error.ToString();
  OnWalletSigninError();
}

void AutofillDialogControllerImpl::OnAutomaticSigninFailure(
    const GoogleServiceAuthError& error) {
  // TODO(aruslan): automatic sign-in failure.
  NOTIMPLEMENTED();
}

void AutofillDialogControllerImpl::OnDidGetWalletItems(
    scoped_ptr<wallet::WalletItems> wallet_items) {
  legal_documents_text_.clear();
  legal_document_link_ranges_.clear();
  // TODO(dbeam): verify all items support kCartCurrency?
  wallet_items_ = wallet_items.Pass();
  OnWalletOrSigninUpdate();
}

void AutofillDialogControllerImpl::OnDidSaveAddress(
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  // TODO(dbeam): handle required actions.
  active_address_id_ = address_id;
  GetFullWallet();
}

void AutofillDialogControllerImpl::OnDidSaveInstrument(
    const std::string& instrument_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  // TODO(dbeam): handle required actions.
  active_instrument_id_ = instrument_id;
  GetFullWallet();
}

void AutofillDialogControllerImpl::OnDidSaveInstrumentAndAddress(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  // TODO(dbeam): handle required actions.
  active_instrument_id_ = instrument_id;
  active_address_id_ = address_id;
  GetFullWallet();
}

void AutofillDialogControllerImpl::OnDidUpdateAddress(
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  // TODO(dbeam): Handle this callback.
  NOTIMPLEMENTED() << " address_id=" << address_id;
}

void AutofillDialogControllerImpl::OnDidUpdateInstrument(
    const std::string& instrument_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  // TODO(dbeam): handle required actions.
}

void AutofillDialogControllerImpl::OnWalletError(
    wallet::WalletClient::ErrorType error_type) {
  // TODO(dbeam): Do something with |error_type|.
  DisableWallet();
}

void AutofillDialogControllerImpl::OnMalformedResponse() {
  DisableWallet();
}

void AutofillDialogControllerImpl::OnNetworkError(int response_code) {
  DisableWallet();
}

////////////////////////////////////////////////////////////////////////////////
// PersonalDataManagerObserver implementation.

void AutofillDialogControllerImpl::OnPersonalDataChanged() {
  HidePopup();
  GenerateSuggestionsModels();
  view_->ModelChanged();
}

void AutofillDialogControllerImpl::AccountChoiceChanged() {
  if (!view_)
    return;

  // Whenever the user changes the current account, the Wallet data should be
  // cleared. If the user has chosen a Wallet account, an attempt to fetch
  // the Wallet data is made to see if the user is still signed in.
  // This will trigger a passive sign-in if required.
  // TODO(aruslan): integrate an automatic sign-in.
  wallet_items_.reset();
  if (IsPayingWithWallet())
    StartFetchingWalletItems();

  GenerateSuggestionsModels();
  view_->ModelChanged();
  view_->UpdateAccountChooser();
  view_->UpdateNotificationArea();
}

////////////////////////////////////////////////////////////////////////////////

bool AutofillDialogControllerImpl::HandleKeyPressEventInInput(
    const content::NativeWebKeyboardEvent& event) {
  if (popup_controller_)
    return popup_controller_->HandleKeyPressEvent(event);

  return false;
}

bool AutofillDialogControllerImpl::RequestingCreditCardInfo() const {
  DCHECK_GT(form_structure_.field_count(), 0U);

  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    if (IsCreditCardType(form_structure_.field(i)->type()))
      return true;
  }

  return false;
}

bool AutofillDialogControllerImpl::TransmissionWillBeSecure() const {
  return source_url_.SchemeIs(chrome::kHttpsScheme) &&
         !net::IsCertStatusError(ssl_status_.cert_status) &&
         !net::IsCertStatusMinorError(ssl_status_.cert_status);
}

AutofillDialogView* AutofillDialogControllerImpl::CreateView() {
  return AutofillDialogView::Create(this);
}

PersonalDataManager* AutofillDialogControllerImpl::GetManager() {
  return PersonalDataManagerFactory::GetForProfile(profile_);
}

wallet::WalletClient* AutofillDialogControllerImpl::GetWalletClient() {
  return &wallet_client_;
}

bool AutofillDialogControllerImpl::IsPayingWithWallet() const {
  return account_chooser_model_.WalletIsSelected();
}

void AutofillDialogControllerImpl::DisableWallet() {
  signin_helper_.reset();
  current_username_.clear();
  account_chooser_model_.SetHadWalletError();
  GetWalletClient()->CancelPendingRequests();
}

void AutofillDialogControllerImpl::OnWalletSigninError() {
  signin_helper_.reset();
  current_username_.clear();
  account_chooser_model_.SetHadWalletSigninError();
  GetWalletClient()->CancelPendingRequests();
}

bool AutofillDialogControllerImpl::IsFirstRun() const {
  PrefService* prefs = profile_->GetPrefs();
  return !prefs->HasPrefPath(prefs::kAutofillDialogPayWithoutWallet);
}

void AutofillDialogControllerImpl::GenerateSuggestionsModels() {
  suggested_email_.Reset();
  suggested_cc_.Reset();
  suggested_billing_.Reset();
  suggested_cc_billing_.Reset();
  suggested_shipping_.Reset();

  if (IsPayingWithWallet()) {
    if (wallet_items_.get()) {
      // TODO(estade): seems we need to hardcode the email address.

      const std::vector<wallet::Address*>& addresses =
          wallet_items_->addresses();
      for (size_t i = 0; i < addresses.size(); ++i) {
        // TODO(dbeam): respect wallet_items_->default_instrument_id().
        suggested_shipping_.AddKeyedItemWithSublabel(
            base::IntToString(i),
            addresses[i]->DisplayName(),
            addresses[i]->DisplayNameDetail());
      }

      const std::vector<wallet::WalletItems::MaskedInstrument*>& instruments =
          wallet_items_->instruments();
      for (size_t i = 0; i < instruments.size(); ++i) {
        // TODO(dbeam): respect wallet_items_->default_address_id().
        suggested_cc_billing_.AddKeyedItemWithSublabelAndIcon(
            base::IntToString(i),
            instruments[i]->DisplayName(),
            instruments[i]->DisplayNameDetail(),
            instruments[i]->CardIcon());
      }
    }

    suggested_cc_billing_.AddKeyedItem(
        std::string(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_DETAILS));
  } else {
    PersonalDataManager* manager = GetManager();
    const std::vector<CreditCard*>& cards = manager->credit_cards();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    for (size_t i = 0; i < cards.size(); ++i) {
      suggested_cc_.AddKeyedItemWithIcon(
          cards[i]->guid(),
          cards[i]->Label(),
          rb.GetImageNamed(cards[i]->IconResourceId()));
    }

    const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
    const std::string app_locale = AutofillCountry::ApplicationLocale();
    for (size_t i = 0; i < profiles.size(); ++i) {
      if (!IsCompleteProfile(*profiles[i]))
        continue;

      // Add all email addresses.
      std::vector<string16> values;
      profiles[i]->GetMultiInfo(EMAIL_ADDRESS, app_locale, &values);
      for (size_t j = 0; j < values.size(); ++j) {
        if (!values[j].empty())
          suggested_email_.AddKeyedItem(profiles[i]->guid(), values[j]);
      }

      // Don't add variants for addresses: the email variants are handled above,
      // name is part of credit card and we'll just ignore phone number
      // variants.
      suggested_billing_.AddKeyedItem(profiles[i]->guid(),
                                      profiles[i]->Label());
      suggested_shipping_.AddKeyedItem(profiles[i]->guid(),
                                       profiles[i]->Label());
    }

    suggested_cc_.AddKeyedItem(
        std::string(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_CREDIT_CARD));
    suggested_billing_.AddKeyedItem(
        std::string(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_ADDRESS));
  }

  suggested_email_.AddKeyedItem(
      std::string(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_EMAIL_ADDRESS));
  suggested_shipping_.AddKeyedItem(
      std::string(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_SHIPPING_ADDRESS));
}

bool AutofillDialogControllerImpl::IsCompleteProfile(
    const AutofillProfile& profile) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t i = 0; i < requested_shipping_fields_.size(); ++i) {
    AutofillFieldType type = requested_shipping_fields_[i].type;
    if (type != ADDRESS_HOME_LINE2 &&
        profile.GetInfo(type, app_locale).empty()) {
      return false;
    }
  }

  return true;
}

void AutofillDialogControllerImpl::FillOutputForSectionWithComparator(
    DialogSection section,
    const InputFieldComparator& compare) {
  if (!SectionIsActive(section))
    return;

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  if (wrapper) {
    // Only fill in data that is associated with this section.
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    wrapper->FillFormStructure(inputs, compare, &form_structure_);

    // CVC needs special-casing because the CreditCard class doesn't store or
    // handle them. This isn't necessary when filling the combined CC and
    // billing section as CVC comes from |full_wallet_| in this case.
    if (section == SECTION_CC)
      SetCvcResult(view_->GetCvc());
  } else {
    // The user manually input data. If using Autofill, save the info as new or
    // edited data. Always fill local data into |form_structure_|.
    DetailOutputMap output;
    view_->GetUserInput(section, &output);

    if (section == SECTION_CC) {
      CreditCard card;
      FillFormGroupFromOutputs(output, &card);

      if (ShouldSaveDetailsLocally())
        GetManager()->SaveImportedCreditCard(card);

      FillFormStructureForSection(card, 0, section, compare);

      // Again, CVC needs special-casing. Fill it in directly from |output|.
      SetCvcResult(GetValueForType(output, CREDIT_CARD_VERIFICATION_CODE));
    } else {
      AutofillProfile profile;
      FillFormGroupFromOutputs(output, &profile);

      // TODO(estade): we should probably edit the existing profile in the
      // cases where the input data is based on an existing profile (user
      // clicked "Edit" or autofill popup filled in the form).
      if (ShouldSaveDetailsLocally())
        GetManager()->SaveImportedProfile(profile);

      FillFormStructureForSection(profile, 0, section, compare);
    }
  }
}

void AutofillDialogControllerImpl::FillOutputForSection(DialogSection section) {
  FillOutputForSectionWithComparator(section,
                                     base::Bind(DetailInputMatchesField));
}

void AutofillDialogControllerImpl::FillFormStructureForSection(
    const FormGroup& form_group,
    size_t variant,
    DialogSection section,
    const InputFieldComparator& compare) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    // Only fill in data that is associated with this section.
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (compare.Run(inputs[j], *field)) {
        form_group.FillFormField(*field, variant, field);
        break;
      }
    }
  }
}

void AutofillDialogControllerImpl::SetCvcResult(const string16& cvc) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    if (field->type() == CREDIT_CARD_VERIFICATION_CODE) {
      field->value = cvc;
      break;
    }
  }
}

SuggestionsMenuModel* AutofillDialogControllerImpl::
    SuggestionsMenuModelForSection(DialogSection section) {
  switch (section) {
    case SECTION_EMAIL:
      return &suggested_email_;
    case SECTION_CC:
      return &suggested_cc_;
    case SECTION_BILLING:
      return &suggested_billing_;
    case SECTION_SHIPPING:
      return &suggested_shipping_;
    case SECTION_CC_BILLING:
      return &suggested_cc_billing_;
  }

  NOTREACHED();
  return NULL;
}

DialogSection AutofillDialogControllerImpl::SectionForSuggestionsMenuModel(
    const SuggestionsMenuModel& model) {
  if (&model == &suggested_email_)
    return SECTION_EMAIL;

  if (&model == &suggested_cc_)
    return SECTION_CC;

  if (&model == &suggested_billing_)
    return SECTION_BILLING;

  if (&model == &suggested_cc_billing_)
    return SECTION_CC_BILLING;

  DCHECK_EQ(&model, &suggested_shipping_);
  return SECTION_SHIPPING;
}

DetailInputs* AutofillDialogControllerImpl::MutableRequestedFieldsForSection(
    DialogSection section) {
  return const_cast<DetailInputs*>(&RequestedFieldsForSection(section));
}

void AutofillDialogControllerImpl::HidePopup() {
  if (popup_controller_)
    popup_controller_->Hide();
}

void AutofillDialogControllerImpl::LoadRiskFingerprintData() {
  // TODO(dbeam): Add a CHECK or otherwise strong guarantee that the ToS have
  // been accepted prior to calling into this method.  Also, ensure that the UI
  // contains a clear indication to the user as to what data will be collected.
  // Until then, this code should not be called.

  int64 gaia_id = 0;
  bool success =
      base::StringToInt64(wallet_items_->obfuscated_gaia_id(), &gaia_id);
  DCHECK(success);

  gfx::Rect window_bounds =
      GetBaseWindowForWebContents(web_contents())->GetBounds();

  PrefService* user_prefs = profile_->GetPrefs();
  std::string charset = user_prefs->GetString(prefs::kDefaultCharset);
  std::string accept_languages = user_prefs->GetString(prefs::kAcceptLanguages);
  base::Time install_time = base::Time::FromTimeT(
      g_browser_process->local_state()->GetInt64(prefs::kInstallDate));

  risk::GetFingerprint(
      gaia_id, window_bounds, *web_contents(), chrome::VersionInfo().Version(),
      charset, accept_languages, install_time, dialog_type_,
      base::Bind(&AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData(
    scoped_ptr<risk::Fingerprint> fingerprint) {
  NOTIMPLEMENTED();
}

bool AutofillDialogControllerImpl::IsManuallyEditingSection(
    DialogSection section) {
  return section_editing_state_[section] ||
         SuggestionsMenuModelForSection(section)->
             GetItemKeyForCheckedItem().empty();
}

bool AutofillDialogControllerImpl::ShouldUseBillingForShipping() {
  // If the user is editing or inputting data, ask the view.
  if (IsManuallyEditingSection(SECTION_SHIPPING))
    return view_->UseBillingForShipping();

  // Otherwise, the checkbox should be hidden so its state is irrelevant.
  // Always use the shipping suggestion model.
  return false;
}

bool AutofillDialogControllerImpl::ShouldSaveDetailsLocally() {
  // It's possible that the user checked [X] Save details locally before
  // switching payment methods, so only ask the view whether to save details
  // locally if that checkbox is showing (currently if not paying with wallet).
  return !IsPayingWithWallet() && view_->SaveDetailsLocally();
}

void AutofillDialogControllerImpl::SubmitWithWallet() {
  // TODO(dbeam): disallow interacting with the dialog while submitting.

  active_instrument_id_.clear();
  active_address_id_.clear();

  if (!section_editing_state_[SECTION_CC_BILLING]) {
    SuggestionsMenuModel* billing =
        SuggestionsMenuModelForSection(SECTION_CC_BILLING);
    if (!billing->GetItemKeyForCheckedItem().empty() &&
        billing->checked_item() <
            static_cast<int>(wallet_items_->instruments().size())) {
      const wallet::WalletItems::MaskedInstrument* active_instrument =
          wallet_items_->instruments()[billing->checked_item()];
      active_instrument_id_ = active_instrument->object_id();

      // TODO(dbeam): does re-using instrument address IDs work?
      if (ShouldUseBillingForShipping())
        active_address_id_ = active_instrument->address().object_id();
    }
  }

  if (!section_editing_state_[SECTION_SHIPPING] && active_address_id_.empty()) {
    SuggestionsMenuModel* shipping =
        SuggestionsMenuModelForSection(SECTION_SHIPPING);
    if (!shipping->GetItemKeyForCheckedItem().empty() &&
        shipping->checked_item() <
            static_cast<int>(wallet_items_->addresses().size())) {
      active_address_id_ =
          wallet_items_->addresses()[shipping->checked_item()]->object_id();
    }
  }

  GetWalletClient()->AcceptLegalDocuments(
      wallet_items_->legal_documents(),
      wallet_items_->google_transaction_id(),
      source_url_);

  scoped_ptr<wallet::Instrument> new_instrument;
  if (active_instrument_id_.empty()) {
    DetailOutputMap output;
    view_->GetUserInput(SECTION_CC_BILLING, &output);

    CreditCard card;
    AutofillProfile profile;
    string16 cvc;
    GetBillingInfoFromOutputs(output, &card, &cvc, &profile);

    new_instrument.reset(new wallet::Instrument(card, cvc, profile));
  }

  scoped_ptr<wallet::Address> new_address;
  if (active_address_id_.empty()) {
    if (ShouldUseBillingForShipping() && new_instrument.get()) {
      new_address.reset(new wallet::Address(new_instrument->address()));
    } else {
      DetailOutputMap output;
      view_->GetUserInput(SECTION_SHIPPING, &output);

      AutofillProfile profile;
      FillFormGroupFromOutputs(output, &profile);

      new_address.reset(new wallet::Address(profile));
    }
  }

  if (new_instrument.get() && new_address.get()) {
    GetWalletClient()->SaveInstrumentAndAddress(
        *new_instrument,
        *new_address,
        wallet_items_->obfuscated_gaia_id(),
        source_url_);
  } else if (new_instrument.get()) {
    GetWalletClient()->SaveInstrument(*new_instrument,
                                      wallet_items_->obfuscated_gaia_id(),
                                      source_url_);
  } else if (new_address.get()) {
    GetWalletClient()->SaveAddress(*new_address, source_url_);
  } else {
    GetFullWallet();
  }
}

void AutofillDialogControllerImpl::GetFullWallet() {
  DCHECK(did_submit_);
  DCHECK(IsPayingWithWallet());
  DCHECK(wallet_items_);
  DCHECK(!active_instrument_id_.empty());
  DCHECK(!active_address_id_.empty());

  GetWalletClient()->GetFullWallet(wallet::WalletClient::FullWalletRequest(
      active_instrument_id_,
      active_address_id_,
      source_url_,
      wallet::Cart(base::IntToString(kCartMax), kCartCurrency),
      wallet_items_->google_transaction_id(),
      std::vector<wallet::WalletClient::RiskCapability>()));
}

void AutofillDialogControllerImpl::FinishSubmit() {
  FillOutputForSection(SECTION_EMAIL);
  FillOutputForSection(SECTION_CC);
  FillOutputForSection(SECTION_BILLING);
  FillOutputForSection(SECTION_CC_BILLING);
  if (ShouldUseBillingForShipping()) {
    FillOutputForSectionWithComparator(
        SECTION_BILLING,
        base::Bind(DetailInputMatchesShippingField));
    FillOutputForSectionWithComparator(
        SECTION_CC,
        base::Bind(DetailInputMatchesShippingField));
  } else {
    FillOutputForSection(SECTION_SHIPPING);
  }
  if (wallet_items_)
    callback_.Run(&form_structure_, wallet_items_->google_transaction_id());
  else
    callback_.Run(&form_structure_, std::string());
  callback_ = base::Callback<void(const FormStructure*, const std::string&)>();

  if (dialog_type_ == DIALOG_TYPE_REQUEST_AUTOCOMPLETE) {
    // This may delete us.
    Hide();
  }
}

AutofillMetrics::DialogInitialUserStateMetric
    AutofillDialogControllerImpl::GetInitialUserState() const {
  // Consider a user to be an Autofill user if the user has any credit cards
  // or addresses saved.  Check that the item count is greater than 1 because
  // an "empty" menu still has the "add new" menu item.
  const bool has_autofill_profiles =
      suggested_cc_.GetItemCount() > 1 ||
      suggested_billing_.GetItemCount() > 1;

  if (SignedInState() != SIGNED_IN) {
    // Not signed in.
    return has_autofill_profiles ?
        AutofillMetrics::DIALOG_USER_NOT_SIGNED_IN_HAS_AUTOFILL :
        AutofillMetrics::DIALOG_USER_NOT_SIGNED_IN_NO_AUTOFILL;
  }

  // Signed in.
  if (wallet_items_->instruments().empty()) {
    // No Wallet items.
    return has_autofill_profiles ?
        AutofillMetrics::DIALOG_USER_SIGNED_IN_NO_WALLET_HAS_AUTOFILL :
        AutofillMetrics::DIALOG_USER_SIGNED_IN_NO_WALLET_NO_AUTOFILL;
  }

  // Has Wallet items.
  return has_autofill_profiles ?
      AutofillMetrics::DIALOG_USER_SIGNED_IN_HAS_WALLET_HAS_AUTOFILL :
      AutofillMetrics::DIALOG_USER_SIGNED_IN_HAS_WALLET_NO_AUTOFILL;
}

}  // namespace autofill
