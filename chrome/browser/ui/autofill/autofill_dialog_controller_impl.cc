// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <algorithm>
#include <map>
#include <string>

#include "apps/native_app_window.h"
#include "apps/shell_window.h"
#include "apps/shell_window_registry.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/prefs/scoped_user_pref_update.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_common.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#if !defined(OS_ANDROID)
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#endif
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/risk/fingerprint.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/browser/wallet/form_field_error.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_signin_helper.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/form_data.h"
#include "components/user_prefs/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/component_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/webkit_resources.h"
#include "net/cert/cert_status_flags.h"
#include "ui/base/base_window.h"
#include "ui/base/events/event.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace autofill {

namespace {

const char kAddNewItemKey[] = "add-new-item";
const char kManageItemsKey[] = "manage-items";
const char kSameAsBillingKey[] = "same-as-billing";

// URLs for Wallet error messages.
const char kBuyerLegalAddressStatusUrl[] =
    "https://wallet.google.com/manage/settings";
const char kKnowYourCustomerStatusUrl[] = "https://wallet.google.com/kyc";

// Keys for the kAutofillDialogAutofillDefault pref dictionary (do not change
// these values).
const char kGuidPrefKey[] = "guid";
const char kVariantPrefKey[] = "variant";

// This string is stored along with saved addresses and credit cards in the
// WebDB, and hence should not be modified, so that it remains consistent over
// time.
const char kAutofillDialogOrigin[] = "Chrome Autofill dialog";

// HSL shift to gray out an image.
const color_utils::HSL kGrayImageShift = {-1, 0, 0.8};

// Limit Wallet items refresh rate to at most once per minute.
const int kWalletItemsRefreshRateSeconds = 60;

// A helper class to make sure an AutofillDialogView knows when a series of
// updates is incoming.
class ScopedViewUpdates {
 public:
  explicit ScopedViewUpdates(AutofillDialogView* view) : view_(view) {
    if (view_)
      view_->UpdatesStarted();
  }

  ~ScopedViewUpdates() {
    if (view_)
      view_->UpdatesFinished();
  }

 private:
  AutofillDialogView* view_;

  DISALLOW_COPY_AND_ASSIGN(ScopedViewUpdates);
};

// Returns true if |card_type| is supported by Wallet.
bool IsWalletSupportedCard(const std::string& card_type) {
  return card_type == autofill::kVisaCard ||
         card_type == autofill::kMasterCard ||
         card_type == autofill::kDiscoverCard;
}

// Returns true if |input| should be used to fill a site-requested |field| which
// is notated with a "shipping" tag, for use when the user has decided to use
// the billing address as the shipping address.
bool DetailInputMatchesShippingField(const DetailInput& input,
                                     const AutofillField& field) {
  // Equivalent billing field type is used to support UseBillingAsShipping
  // usecase.
  ServerFieldType field_type =
      AutofillType::GetEquivalentBillingFieldType(
          field.Type().GetStorableType());

  return common::InputTypeMatchesFieldType(input, AutofillType(field_type));
}

// Initializes |form_group| from user-entered data.
void FillFormGroupFromOutputs(const DetailOutputMap& detail_outputs,
                              FormGroup* form_group) {
  for (DetailOutputMap::const_iterator iter = detail_outputs.begin();
       iter != detail_outputs.end(); ++iter) {
    ServerFieldType type = iter->first->type;
    if (!iter->second.empty()) {
      if (type == ADDRESS_HOME_COUNTRY || type == ADDRESS_BILLING_COUNTRY) {
        form_group->SetInfo(AutofillType(type),
                            iter->second,
                            g_browser_process->GetApplicationLocale());
      } else {
        form_group->SetRawInfo(
            AutofillType(type).GetStorableType(), iter->second);
      }
    }
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
      if (cvc)
        cvc->assign(trimmed);
    } else if (it->first->type == ADDRESS_HOME_COUNTRY ||
               it->first->type == ADDRESS_BILLING_COUNTRY) {
      if (profile) {
        profile->SetInfo(AutofillType(it->first->type),
                         trimmed,
                         g_browser_process->GetApplicationLocale());
      }
    } else {
      // Copy the credit card name to |profile| in addition to |card| as
      // wallet::Instrument requires a recipient name for its billing address.
      if (card && it->first->type == NAME_FULL)
        card->SetRawInfo(CREDIT_CARD_NAME, trimmed);

      if (common::IsCreditCardType(it->first->type)) {
        if (card)
          card->SetRawInfo(it->first->type, trimmed);
      } else if (profile) {
        profile->SetRawInfo(
            AutofillType(it->first->type).GetStorableType(), trimmed);
      }
    }
  }
}

// Returns the containing window for the given |web_contents|. The containing
// window might be a browser window for a Chrome tab, or it might be a shell
// window for a platform app.
ui::BaseWindow* GetBaseWindowForWebContents(
    const content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    return browser->window();

  gfx::NativeWindow native_window =
      web_contents->GetView()->GetTopLevelNativeWindow();
  apps::ShellWindow* shell_window =
      apps::ShellWindowRegistry::
          GetShellWindowForNativeWindowAnyProfile(native_window);
  return shell_window->GetBaseWindow();
}

// Extracts the string value of a field with |type| from |output|. This is
// useful when you only need the value of 1 input from a section of view inputs.
string16 GetValueForType(const DetailOutputMap& output,
                         ServerFieldType type) {
  for (DetailOutputMap::const_iterator it = output.begin();
       it != output.end(); ++it) {
    if (it->first->type == type)
      return it->second;
  }
  NOTREACHED();
  return string16();
}

// Returns a string descriptor for a DialogSection, for use with prefs (do not
// change these values).
std::string SectionToPrefString(DialogSection section) {
  switch (section) {
    case SECTION_CC:
      return "cc";

    case SECTION_BILLING:
      return "billing";

    case SECTION_CC_BILLING:
      // The SECTION_CC_BILLING section isn't active when using Autofill.
      NOTREACHED();
      return std::string();

    case SECTION_SHIPPING:
      return "shipping";

    case SECTION_EMAIL:
      return "email";
  }

  NOTREACHED();
  return std::string();
}

// Check if a given MaskedInstrument is allowed for the purchase.
bool IsInstrumentAllowed(
    const wallet::WalletItems::MaskedInstrument& instrument) {
  switch (instrument.status()) {
    case wallet::WalletItems::MaskedInstrument::VALID:
    case wallet::WalletItems::MaskedInstrument::PENDING:
    case wallet::WalletItems::MaskedInstrument::EXPIRED:
    case wallet::WalletItems::MaskedInstrument::BILLING_INCOMPLETE:
      return true;
    default:
      return false;
  }
}

// Signals that the user has opted in to geolocation services.  Factored out
// into a separate method because all interaction with the geolocation provider
// needs to happen on the IO thread, which is not the thread
// AutofillDialogViewDelegate lives on.
void UserDidOptIntoLocationServices() {
  content::GeolocationProvider::GetInstance()->UserDidOptIntoLocationServices();
}

// Returns whether |data_model| is complete, i.e. can fill out all the
// |requested_fields|, and verified, i.e. not just automatically aggregated.
// Incomplete or unverifed data will not be displayed in the dropdown menu.
bool HasCompleteAndVerifiedData(const AutofillDataModel& data_model,
                                const DetailInputs& requested_fields) {
  if (!data_model.IsVerified())
    return false;

  for (size_t i = 0; i < requested_fields.size(); ++i) {
    ServerFieldType type = requested_fields[i].type;
    if (type != ADDRESS_HOME_LINE2 &&
        type != CREDIT_CARD_VERIFICATION_CODE &&
        data_model.GetRawInfo(AutofillType(type).GetStorableType()).empty()) {
      return false;
    }
  }

  return true;
}

// Returns true if |profile| has an invalid address, i.e. an invalid state, zip
// code, or phone number. Otherwise returns false. Profiles with invalid
// addresses are not suggested in the dropdown menu for billing and shipping
// addresses.
bool HasInvalidAddress(const AutofillProfile& profile) {
  return profile.IsPresentButInvalid(ADDRESS_HOME_STATE) ||
         profile.IsPresentButInvalid(ADDRESS_HOME_ZIP) ||
         profile.IsPresentButInvalid(PHONE_HOME_WHOLE_NUMBER);
}

// Loops through |addresses_| comparing to |address| ignoring ID. If a match
// is not found, NULL is returned.
const wallet::Address* FindDuplicateAddress(
    const std::vector<wallet::Address*>& addresses,
    const wallet::Address& address) {
  for (size_t i = 0; i < addresses.size(); ++i) {
    if (addresses[i]->EqualsIgnoreID(address))
      return addresses[i];
  }
  return NULL;
}

bool IsCardHolderNameValidForWallet(const string16& name) {
  base::string16 whitespace_collapsed_name = CollapseWhitespace(name, true);
  std::vector<base::string16> split_name;
  base::SplitString(whitespace_collapsed_name, ' ', &split_name);
  return split_name.size() >= 2;
}

DialogSection SectionFromLocation(wallet::FormFieldError::Location location) {
  switch (location) {
    case wallet::FormFieldError::PAYMENT_INSTRUMENT:
    case wallet::FormFieldError::LEGAL_ADDRESS:
      return SECTION_CC_BILLING;

    case wallet::FormFieldError::SHIPPING_ADDRESS:
      return SECTION_SHIPPING;

    case wallet::FormFieldError::UNKNOWN_LOCATION:
      NOTREACHED();
      return SECTION_MAX;
  }

  NOTREACHED();
  return SECTION_MAX;
}

scoped_ptr<DialogNotification> GetWalletError(
    wallet::WalletClient::ErrorType error_type) {
  base::string16 text;
  GURL url;

  switch (error_type) {
    case wallet::WalletClient::UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS:
      text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WALLET_UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS);
      url = GURL(kKnowYourCustomerStatusUrl);
      break;

    case wallet::WalletClient::BUYER_LEGAL_ADDRESS_NOT_SUPPORTED:
      text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WALLET_BUYER_COUNTRY_NOT_SUPPORTED);
      url = GURL(kBuyerLegalAddressStatusUrl);
      break;

    default:
      // The notification will not have a link; it's handled in the next
      // switch statement.
      break;
  }

  if (!text.empty()) {
    scoped_ptr<DialogNotification> notification(new DialogNotification(
        DialogNotification::WALLET_ERROR,
        text));
    notification->set_link_url(url);
    return notification.Pass();
  }

  switch (error_type) {
    case wallet::WalletClient::UNSUPPORTED_MERCHANT:
      text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WALLET_UNSUPPORTED_MERCHANT);
      break;

    case wallet::WalletClient::BAD_REQUEST:
      text = l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_WALLET_UPGRADE_CHROME_ERROR,
          ASCIIToUTF16("71"));
      break;

    case wallet::WalletClient::INVALID_PARAMS:
      text = l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_WALLET_UPGRADE_CHROME_ERROR,
          ASCIIToUTF16("42"));
      break;

    case wallet::WalletClient::BUYER_ACCOUNT_ERROR:
      text = l10n_util::GetStringUTF16(IDS_AUTOFILL_WALLET_BUYER_ACCOUNT_ERROR);
      break;

    case wallet::WalletClient::UNSUPPORTED_API_VERSION:
      text = l10n_util::GetStringFUTF16(
          IDS_AUTOFILL_WALLET_UPGRADE_CHROME_ERROR,
          ASCIIToUTF16("43"));
      break;

    case wallet::WalletClient::SERVICE_UNAVAILABLE:
      text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_WALLET_SERVICE_UNAVAILABLE_ERROR);
      break;

    case wallet::WalletClient::INTERNAL_ERROR:
      text = l10n_util::GetStringFUTF16(IDS_AUTOFILL_WALLET_UNKNOWN_ERROR,
                                        ASCIIToUTF16("62"));
      break;

    case wallet::WalletClient::MALFORMED_RESPONSE:
      text = l10n_util::GetStringFUTF16(IDS_AUTOFILL_WALLET_UNKNOWN_ERROR,
                                        ASCIIToUTF16("72"));
      break;

    case wallet::WalletClient::NETWORK_ERROR:
      text = l10n_util::GetStringFUTF16(IDS_AUTOFILL_WALLET_UNKNOWN_ERROR,
                                        ASCIIToUTF16("73"));
      break;

    case wallet::WalletClient::UNKNOWN_ERROR:
      text = l10n_util::GetStringFUTF16(IDS_AUTOFILL_WALLET_UNKNOWN_ERROR,
                                        ASCIIToUTF16("74"));
      break;

    default:
      break;
  }

  DCHECK(!text.empty());

  // The other error types are strings of the form "XXX. You can pay without
  // wallet."
  return make_scoped_ptr(new DialogNotification(
      DialogNotification::WALLET_ERROR,
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_DIALOG_COMPLETE_WITHOUT_WALLET,
                                 text)));
}

gfx::Image GetGeneratedCardImage(const string16& card_number) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  const gfx::ImageSkia* card =
      rb.GetImageSkiaNamed(IDR_AUTOFILL_GENERATED_CARD);
  gfx::Canvas canvas(card->size(), ui::SCALE_FACTOR_100P, false);
  canvas.DrawImageInt(*card, 0, 0);

  gfx::Rect display_rect(gfx::Point(), card->size());
  display_rect.Inset(14, 0, 14, 0);
  // TODO(estade): fallback font for systems that don't have Helvetica?
  gfx::Font helvetica("Helvetica", 14);
  gfx::ShadowValues shadows;
  shadows.push_back(gfx::ShadowValue(gfx::Point(0, 1),
                    0.0,
                    SkColorSetARGB(85, 0, 0, 0)));
  canvas.DrawStringWithShadows(
      card_number,
      helvetica,
      SK_ColorWHITE,
      display_rect, 0, 0, shadows);

  gfx::ImageSkia skia(canvas.ExtractImageRep());
  return gfx::Image(skia);
}

}  // namespace

AutofillDialogViewDelegate::~AutofillDialogViewDelegate() {}

AutofillDialogControllerImpl::~AutofillDialogControllerImpl() {
  if (popup_controller_)
    popup_controller_->Hide();

  GetMetricLogger().LogDialogInitialUserState(
      GetDialogType(), initial_user_state_);
}

// static
base::WeakPtr<AutofillDialogControllerImpl>
    AutofillDialogControllerImpl::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback) {
  // AutofillDialogControllerImpl owns itself.
  AutofillDialogControllerImpl* autofill_dialog_controller =
      new AutofillDialogControllerImpl(contents,
                                       form_structure,
                                       source_url,
                                       dialog_type,
                                       callback);
  return autofill_dialog_controller->weak_ptr_factory_.GetWeakPtr();
}

// static
void AutofillDialogControllerImpl::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterIntegerPref(
      ::prefs::kAutofillDialogShowCount,
      0,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      ::prefs::kAutofillDialogHasPaidWithWallet,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      ::prefs::kAutofillDialogPayWithoutWallet,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterDictionaryPref(
      ::prefs::kAutofillDialogAutofillDefault,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
  registry->RegisterBooleanPref(
      ::prefs::kAutofillDialogSaveData,
      true,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
base::WeakPtr<AutofillDialogController> AutofillDialogController::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback) {
  return AutofillDialogControllerImpl::Create(contents,
                                              form_structure,
                                              source_url,
                                              dialog_type,
                                              callback);
}

// static
void AutofillDialogController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  AutofillDialogControllerImpl::RegisterProfilePrefs(registry);
}

void AutofillDialogControllerImpl::Show() {
  dialog_shown_timestamp_ = base::Time::Now();

  content::NavigationEntry* entry =
      web_contents()->GetController().GetActiveEntry();
  const GURL& active_url = entry ? entry->GetURL() : web_contents()->GetURL();
  invoked_from_same_origin_ = active_url.GetOrigin() == source_url_.GetOrigin();

  // Log any relevant UI metrics and security exceptions.
  GetMetricLogger().LogDialogUiEvent(
      GetDialogType(), AutofillMetrics::DIALOG_UI_SHOWN);

  GetMetricLogger().LogDialogSecurityMetric(
      GetDialogType(), AutofillMetrics::SECURITY_METRIC_DIALOG_SHOWN);

  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    GetMetricLogger().LogDialogSecurityMetric(
        GetDialogType(),
        AutofillMetrics::SECURITY_METRIC_CREDIT_CARD_OVER_HTTP);
  }

  if (!invoked_from_same_origin_) {
    GetMetricLogger().LogDialogSecurityMetric(
        GetDialogType(),
        AutofillMetrics::SECURITY_METRIC_CROSS_ORIGIN_FRAME);
  }

  // Determine what field types should be included in the dialog.
  bool has_types = false;
  bool has_sections = false;
  form_structure_.ParseFieldTypesFromAutocompleteAttributes(
      &has_types, &has_sections);

  // Fail if the author didn't specify autocomplete types.
  if (!has_types) {
    callback_.Run(NULL, std::string());
    delete this;
    return;
  }

  common::BuildInputsForSection(SECTION_EMAIL,
                                &requested_email_fields_);
  common::BuildInputsForSection(SECTION_CC,
                                &requested_cc_fields_);
  common::BuildInputsForSection(SECTION_BILLING,
                                &requested_billing_fields_);
  common::BuildInputsForSection(SECTION_CC_BILLING,
                                &requested_cc_billing_fields_);
  common::BuildInputsForSection(SECTION_SHIPPING,
                                &requested_shipping_fields_);

  // Test whether we need to show the shipping section. If filling that section
  // would be a no-op, don't show it.
  const DetailInputs& inputs = RequestedFieldsForSection(SECTION_SHIPPING);
  EmptyDataModelWrapper empty_wrapper;
  cares_about_shipping_ = empty_wrapper.FillFormStructure(
      inputs,
      base::Bind(common::DetailInputMatchesField, SECTION_SHIPPING),
      &form_structure_);

  SuggestionsUpdated();

  int show_count =
      profile_->GetPrefs()->GetInteger(::prefs::kAutofillDialogShowCount);
  profile_->GetPrefs()->SetInteger(::prefs::kAutofillDialogShowCount,
                                   show_count + 1);

  // TODO(estade): don't show the dialog if the site didn't specify the right
  // fields. First we must figure out what the "right" fields are.
  view_.reset(CreateView());
  view_->Show();
  GetManager()->AddObserver(this);

  // Try to see if the user is already signed-in. If signed-in, fetch the user's
  // Wallet data. Otherwise, see if the user could be signed in passively.
  // TODO(aruslan): UMA metrics for sign-in.
  signin_helper_.reset(new wallet::WalletSigninHelper(
      this, profile_->GetRequestContext()));
  signin_helper_->StartWalletCookieValueFetch();

  if (!account_chooser_model_.WalletIsSelected())
    LogDialogLatencyToShow();
}

void AutofillDialogControllerImpl::Hide() {
  if (view_)
    view_->Hide();
}

void AutofillDialogControllerImpl::TabActivated() {
  // If the user switched away from this tab and then switched back, reload the
  // Wallet items, in case they've changed.
  int seconds_elapsed_since_last_refresh =
      (base::TimeTicks::Now() - last_wallet_items_fetch_timestamp_).InSeconds();
  if (IsPayingWithWallet() && wallet_items_ &&
      seconds_elapsed_since_last_refresh >= kWalletItemsRefreshRateSeconds) {
    GetWalletItems();
  }
}

TestableAutofillDialogView* AutofillDialogControllerImpl::GetTestableView() {
  return view_ ? view_->GetTestableView() : NULL;
}

////////////////////////////////////////////////////////////////////////////////
// AutofillDialogViewDelegate implementation.

string16 AutofillDialogControllerImpl::DialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

string16 AutofillDialogControllerImpl::AccountChooserText() const {
  if (!account_chooser_model_.WalletIsSelected())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PAYING_WITHOUT_WALLET);

  if (SignedInState() == SIGNED_IN)
    return account_chooser_model_.active_wallet_account_name();

  // In this case, the account chooser should be showing the signin link.
  return string16();
}

string16 AutofillDialogControllerImpl::SignInLinkText() const {
  return l10n_util::GetStringUTF16(
      signin_registrar_.IsEmpty() ? IDS_AUTOFILL_DIALOG_SIGN_IN :
                                    IDS_AUTOFILL_DIALOG_CANCEL_SIGN_IN);
}

string16 AutofillDialogControllerImpl::SpinnerText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_LOADING);
}

string16 AutofillDialogControllerImpl::EditSuggestionText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EDIT);
}

string16 AutofillDialogControllerImpl::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

string16 AutofillDialogControllerImpl::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IsSubmitPausedOn(wallet::VERIFY_CVV) ?
      IDS_AUTOFILL_DIALOG_VERIFY_BUTTON : IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

string16 AutofillDialogControllerImpl::SaveLocallyText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_CHECKBOX);
}

string16 AutofillDialogControllerImpl::SaveLocallyTooltip() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_TOOLTIP);
}

string16 AutofillDialogControllerImpl::LegalDocumentsText() {
  if (!IsPayingWithWallet())
    return string16();

  EnsureLegalDocumentsText();
  return legal_documents_text_;
}

DialogSignedInState AutofillDialogControllerImpl::SignedInState() const {
  if (wallet_error_notification_)
    return SIGN_IN_DISABLED;

  if (signin_helper_ || !wallet_items_)
    return REQUIRES_RESPONSE;

  if (wallet_items_->HasRequiredAction(wallet::GAIA_AUTH))
    return REQUIRES_SIGN_IN;

  if (wallet_items_->HasRequiredAction(wallet::PASSIVE_GAIA_AUTH))
    return REQUIRES_PASSIVE_SIGN_IN;

  return SIGNED_IN;
}

bool AutofillDialogControllerImpl::ShouldShowSpinner() const {
  return account_chooser_model_.WalletIsSelected() &&
         SignedInState() == REQUIRES_RESPONSE;
}

bool AutofillDialogControllerImpl::ShouldOfferToSaveInChrome() const {
  return !IsPayingWithWallet() &&
      !profile_->IsOffTheRecord() &&
      IsManuallyEditingAnySection() &&
      !ShouldShowSpinner();
}

bool AutofillDialogControllerImpl::ShouldSaveInChrome() const {
  return profile_->GetPrefs()->GetBoolean(::prefs::kAutofillDialogSaveData);
}

int AutofillDialogControllerImpl::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool AutofillDialogControllerImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (IsSubmitPausedOn(wallet::VERIFY_CVV))
      return true;

    if (ShouldShowSpinner() || is_submitting_)
      return false;

    return true;
  }

  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  return !is_submitting_ || IsSubmitPausedOn(wallet::VERIFY_CVV);
}

DialogOverlayState AutofillDialogControllerImpl::GetDialogOverlay() const {
  bool show_wallet_interstitial = IsPayingWithWallet() && is_submitting_ &&
      !IsSubmitPausedOn(wallet::VERIFY_CVV) &&
      GetDialogType() == DIALOG_TYPE_REQUEST_AUTOCOMPLETE;
  if (!show_wallet_interstitial)
    return DialogOverlayState();

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  DialogOverlayState state;

  state.strings.push_back(DialogOverlayString());
  DialogOverlayString& string = state.strings.back();
  string.font = rb.GetFont(ui::ResourceBundle::BaseFont).DeriveFont(4);

  // First-run, post-submit, Wallet expository page.
  if (full_wallet_ && full_wallet_->required_actions().empty()) {
    string16 cc_number =
        full_wallet_->GetInfo(AutofillType(CREDIT_CARD_NUMBER));
    DCHECK_EQ(16U, cc_number.size());
    state.image = GetGeneratedCardImage(
        ASCIIToUTF16("xxxx xxxx xxxx ") +
        cc_number.substr(cc_number.size() - 4));
    string.text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_CARD_GENERATION_DONE);
    string.text_color = SK_ColorBLACK;
    string.alignment = gfx::ALIGN_CENTER;

    state.strings.push_back(DialogOverlayString());
    DialogOverlayString& subtext = state.strings.back();
    subtext.font = rb.GetFont(ui::ResourceBundle::BaseFont);
    subtext.text_color = SkColorSetRGB(102, 102, 102);
    subtext.text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_CARD_GENERATION_EXPLANATION);
    subtext.alignment = gfx::ALIGN_CENTER;

    state.button_text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_CARD_GENERATION_OK_BUTTON);
  } else {
    // TODO(estade): fix this (animation?).
    state.image =
        GetGeneratedCardImage(ASCIIToUTF16("xxxx xxxx xx..."));

    // "Submitting" waiting page.
    string.text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_CARD_GENERATION_IN_PROGRESS);
    string.text_color = SK_ColorBLACK;
    string.alignment = gfx::ALIGN_CENTER;
  }

  return state;
}

const std::vector<ui::Range>& AutofillDialogControllerImpl::
    LegalDocumentLinks() {
  EnsureLegalDocumentsText();
  return legal_document_link_ranges_;
}

bool AutofillDialogControllerImpl::SectionIsActive(DialogSection section)
    const {
  if (IsSubmitPausedOn(wallet::VERIFY_CVV))
    return section == SECTION_CC_BILLING;

  if (!FormStructureCaresAboutSection(section))
    return false;

  if (IsPayingWithWallet())
    return section == SECTION_CC_BILLING || section == SECTION_SHIPPING;

  return section != SECTION_CC_BILLING;
}

bool AutofillDialogControllerImpl::HasCompleteWallet() const {
  return wallet_items_.get() != NULL &&
         !wallet_items_->instruments().empty() &&
         !wallet_items_->addresses().empty();
}

bool AutofillDialogControllerImpl::IsSubmitPausedOn(
    wallet::RequiredAction required_action) const {
  return full_wallet_ && full_wallet_->HasRequiredAction(required_action);
}

void AutofillDialogControllerImpl::GetWalletItems() {
  DCHECK(previously_selected_instrument_id_.empty());
  DCHECK(previously_selected_shipping_address_id_.empty());
  ScopedViewUpdates updates(view_.get());

  if (wallet_items_) {
    if (ActiveInstrument())
      previously_selected_instrument_id_ = ActiveInstrument()->object_id();

    const wallet::Address* address = ActiveShippingAddress();
    if (address)
      previously_selected_shipping_address_id_ = address->object_id();
  }

  last_wallet_items_fetch_timestamp_ = base::TimeTicks::Now();
  wallet_items_.reset();

  // The "Loading..." page should be showing now, which should cause the
  // account chooser to hide.
  view_->UpdateAccountChooser();
  GetWalletClient()->GetWalletItems(source_url_);
}

void AutofillDialogControllerImpl::HideSignIn() {
  ScopedViewUpdates updates(view_.get());
  signin_registrar_.RemoveAll();
  view_->HideSignIn();
  view_->UpdateAccountChooser();
}

void AutofillDialogControllerImpl::SignedInStateUpdated() {
  switch (SignedInState()) {
    case SIGNED_IN:
      // Start fetching the user name if we don't know it yet.
      if (account_chooser_model_.active_wallet_account_name().empty()) {
        signin_helper_.reset(new wallet::WalletSigninHelper(
            this, profile_->GetRequestContext()));
        signin_helper_->StartUserNameFetch();
      } else {
        LogDialogLatencyToShow();
      }
      break;

    case REQUIRES_SIGN_IN:
    case SIGN_IN_DISABLED:
      // Switch to the local account and refresh the dialog.
      OnWalletSigninError();
      break;

    case REQUIRES_PASSIVE_SIGN_IN:
      // Attempt to passively sign in the user.
      DCHECK(!signin_helper_);
      account_chooser_model_.ClearActiveWalletAccountName();
      signin_helper_.reset(new wallet::WalletSigninHelper(
          this,
          profile_->GetRequestContext()));
      signin_helper_->StartPassiveSignin();
      break;

    case REQUIRES_RESPONSE:
      break;
  }
}

void AutofillDialogControllerImpl::OnWalletOrSigninUpdate() {
  ScopedViewUpdates updates(view_.get());
  SignedInStateUpdated();
  SuggestionsUpdated();
  UpdateAccountChooserView();

  if (view_)
    view_->UpdateButtonStrip();

  // On the first successful response, compute the initial user state metric.
  if (initial_user_state_ == AutofillMetrics::DIALOG_USER_STATE_UNKNOWN)
    initial_user_state_ = GetInitialUserState();
}

void AutofillDialogControllerImpl::OnWalletFormFieldError(
    const std::vector<wallet::FormFieldError>& form_field_errors) {
  if (form_field_errors.empty())
    return;

  for (std::vector<wallet::FormFieldError>::const_iterator it =
           form_field_errors.begin();
       it != form_field_errors.end(); ++it) {
    if (it->error_type() == wallet::FormFieldError::UNKNOWN_ERROR ||
        it->GetAutofillType() == MAX_VALID_FIELD_TYPE ||
        it->location() == wallet::FormFieldError::UNKNOWN_LOCATION) {
      wallet_server_validation_recoverable_ = false;
      break;
    }
    DialogSection section = SectionFromLocation(it->location());
    wallet_errors_[section][it->GetAutofillType()] =
        std::make_pair(it->GetErrorMessage(),
                       GetValueFromSection(section, it->GetAutofillType()));
  }

  // Unrecoverable validation errors.
  if (!wallet_server_validation_recoverable_)
    DisableWallet(wallet::WalletClient::UNKNOWN_ERROR);

  UpdateForErrors();
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

void AutofillDialogControllerImpl::ResetSectionInput(DialogSection section) {
  SetEditingExistingData(section, false);

  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  for (DetailInputs::iterator it = inputs->begin(); it != inputs->end(); ++it) {
    it->initial_value.clear();
  }
}

void AutofillDialogControllerImpl::ShowEditUiIfBadSuggestion(
    DialogSection section) {
  // |CreateWrapper()| returns an empty wrapper if |IsEditingExistingData()|, so
  // get the wrapper before this potentially happens below.
  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);

  // If the chosen item in |model| yields an empty suggestion text, it is
  // invalid. In this case, show the edit UI and highlight invalid fields.
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  string16 unused, unused2;
  if (IsASuggestionItemKey(model->GetItemKeyForCheckedItem()) &&
      !SuggestionTextForSection(section, &unused, &unused2)) {
    SetEditingExistingData(section, true);
  }

  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  if (wrapper && IsEditingExistingData(section))
    wrapper->FillInputs(inputs);

  for (DetailInputs::iterator it = inputs->begin(); it != inputs->end(); ++it) {
    it->editable = InputIsEditable(*it, section);
  }
}

bool AutofillDialogControllerImpl::InputWasEdited(ServerFieldType type,
                                                  const base::string16& value) {
  if (value.empty())
    return false;

  // If this is a combobox at the default value, don't preserve it.
  ui::ComboboxModel* model = ComboboxModelForAutofillType(type);
  if (model && model->GetItemAt(model->GetDefaultIndex()) == value)
    return false;

  return true;
}

DetailOutputMap AutofillDialogControllerImpl::TakeUserInputSnapshot() {
  DetailOutputMap snapshot;
  if (!view_)
    return snapshot;

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
    if (model->GetItemKeyForCheckedItem() != kAddNewItemKey)
      continue;

    DetailOutputMap outputs;
    view_->GetUserInput(section, &outputs);
    // Remove fields that are empty, at their default values, or invalid.
    for (DetailOutputMap::iterator it = outputs.begin(); it != outputs.end();
         ++it) {
      if (InputWasEdited(it->first->type, it->second) &&
          InputValidityMessage(section, it->first->type, it->second).empty()) {
        snapshot.insert(std::make_pair(it->first, it->second));
      }
    }
  }

  return snapshot;
}

void AutofillDialogControllerImpl::RestoreUserInputFromSnapshot(
    const DetailOutputMap& snapshot) {
  if (snapshot.empty())
    return;

  DetailOutputWrapper wrapper(snapshot);
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    if (!SectionIsActive(section))
      continue;

    DetailInputs* inputs = MutableRequestedFieldsForSection(section);
    wrapper.FillInputs(inputs);

    for (size_t i = 0; i < inputs->size(); ++i) {
      if (InputWasEdited((*inputs)[i].type, (*inputs)[i].initial_value)) {
        SuggestionsMenuModelForSection(section)->SetCheckedItem(kAddNewItemKey);
        break;
      }
    }
  }
}

void AutofillDialogControllerImpl::UpdateSection(DialogSection section) {
  if (view_)
    view_->UpdateSection(section);
}

void AutofillDialogControllerImpl::UpdateForErrors() {
  if (!view_)
    return;

  // Currently, the view should only need to be updated if there are
  // |wallet_errors_| or validating a suggestion that's based on existing data.
  bool should_update = !wallet_errors_.empty();
  if (!should_update) {
    for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
      if (IsEditingExistingData(static_cast<DialogSection>(i))) {
        should_update = true;
        break;
      }
    }
  }

  if (should_update)
    view_->UpdateForErrors();
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
    ServerFieldType type) {
  switch (type) {
    case CREDIT_CARD_EXP_MONTH:
      return &cc_exp_month_combobox_model_;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return &cc_exp_year_combobox_model_;

    case ADDRESS_HOME_COUNTRY:
    case ADDRESS_BILLING_COUNTRY:
      return &country_combobox_model_;

    default:
      return NULL;
  }
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForSection(
    DialogSection section) {
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  // The shipping section menu is special. It will always show because there is
  // a choice between "Use billing" and "enter new".
  if (section == SECTION_SHIPPING)
    return model;

  // For other sections, only show a menu if there's at least one suggestion.
  for (int i = 0; i < model->GetItemCount(); ++i) {
    if (IsASuggestionItemKey(model->GetItemKeyAt(i)))
      return model;
  }

  return NULL;
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForAccountChooser() {
  // If there were unrecoverable Wallet errors, or if there are choices other
  // than "Pay without the wallet", show the full menu.
  if (wallet_error_notification_ ||
      account_chooser_model_.HasAccountsToChoose()) {
    return &account_chooser_model_;
  }

  // Otherwise, there is no menu, just a sign in link.
  return NULL;
}

gfx::Image AutofillDialogControllerImpl::AccountChooserImage() {
  if (!MenuModelForAccountChooser()) {
    if (signin_registrar_.IsEmpty()) {
      return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
          IDR_WALLET_ICON);
    }

    return gfx::Image();
  }

  gfx::Image icon;
  account_chooser_model_.GetIconAt(
      account_chooser_model_.GetIndexOfCommandId(
          account_chooser_model_.checked_item()),
      &icon);
  return icon;
}

gfx::Image AutofillDialogControllerImpl::ButtonStripImage() const {
  if (IsPayingWithWallet()) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_WALLET_LOGO);
  }

  return gfx::Image();
}

string16 AutofillDialogControllerImpl::LabelForSection(DialogSection section)
    const {
  switch (section) {
    case SECTION_EMAIL:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_EMAIL);
    case SECTION_CC:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC);
    case SECTION_BILLING:
    case SECTION_CC_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_BILLING);
    case SECTION_SHIPPING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_SHIPPING);
    default:
      NOTREACHED();
      return string16();
  }
}

SuggestionState AutofillDialogControllerImpl::SuggestionStateForSection(
    DialogSection section) {
  string16 vertically_compact, horizontally_compact;
  bool show_suggestion = SuggestionTextForSection(section,
                                                  &vertically_compact,
                                                  &horizontally_compact);
  return SuggestionState(show_suggestion,
                         vertically_compact,
                         horizontally_compact,
                         SuggestionIconForSection(section),
                         ExtraSuggestionTextForSection(section),
                         ExtraSuggestionIconForSection(section));
}

bool AutofillDialogControllerImpl::SuggestionTextForSection(
    DialogSection section,
    base::string16* vertically_compact,
    base::string16* horizontally_compact) {
  base::string16 action_text = RequiredActionTextForSection(section);
  if (!action_text.empty()) {
    *vertically_compact = *horizontally_compact = action_text;
    return true;
  }

  // When the user has clicked 'edit' or a suggestion is somehow invalid (e.g. a
  // user selects a credit card that has expired), don't show a suggestion (even
  // though there is a profile selected in the model).
  if (IsEditingExistingData(section))
    return false;

  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  if (item_key == kSameAsBillingKey) {
    *vertically_compact = *horizontally_compact = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_USING_BILLING_FOR_SHIPPING);
    return true;
  }

  if (!IsASuggestionItemKey(item_key))
    return false;

  if (section == SECTION_EMAIL) {
    *vertically_compact = *horizontally_compact =
        model->GetLabelAt(model->checked_item());
    return true;
  }

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  return wrapper->GetDisplayText(vertically_compact, horizontally_compact);
}

string16 AutofillDialogControllerImpl::RequiredActionTextForSection(
    DialogSection section) const {
  if (section == SECTION_CC_BILLING && IsSubmitPausedOn(wallet::VERIFY_CVV)) {
    const wallet::WalletItems::MaskedInstrument* current_instrument =
        wallet_items_->GetInstrumentById(active_instrument_id_);
    if (current_instrument)
      return current_instrument->TypeAndLastFourDigits();

    DetailOutputMap output;
    view_->GetUserInput(section, &output);
    CreditCard card;
    GetBillingInfoFromOutputs(output, &card, NULL, NULL);
    return card.TypeAndLastFourDigits();
  }

  return string16();
}

string16 AutofillDialogControllerImpl::ExtraSuggestionTextForSection(
    DialogSection section) const {
  if (section == SECTION_CC ||
      (section == SECTION_CC_BILLING && IsSubmitPausedOn(wallet::VERIFY_CVV))) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC);
  }

  return string16();
}

const wallet::WalletItems::MaskedInstrument* AutofillDialogControllerImpl::
    ActiveInstrument() const {
  if (!IsPayingWithWallet())
    return NULL;

  const SuggestionsMenuModel* model =
      SuggestionsMenuModelForSection(SECTION_CC_BILLING);
  const std::string item_key = model->GetItemKeyForCheckedItem();
  if (!IsASuggestionItemKey(item_key))
    return NULL;

  int index;
  if (!base::StringToInt(item_key, &index) || index < 0 ||
      static_cast<size_t>(index) >= wallet_items_->instruments().size()) {
    NOTREACHED();
    return NULL;
  }

  return wallet_items_->instruments()[index];
}

const wallet::Address* AutofillDialogControllerImpl::
    ActiveShippingAddress() const {
  if (!IsPayingWithWallet() || !IsShippingAddressRequired())
    return NULL;

  const SuggestionsMenuModel* model =
      SuggestionsMenuModelForSection(SECTION_SHIPPING);
  const std::string item_key = model->GetItemKeyForCheckedItem();
  if (!IsASuggestionItemKey(item_key))
    return NULL;

  int index;
  if (!base::StringToInt(item_key, &index) || index < 0 ||
      static_cast<size_t>(index) >= wallet_items_->addresses().size()) {
    NOTREACHED();
    return NULL;
  }

  return wallet_items_->addresses()[index];
}

scoped_ptr<DataModelWrapper> AutofillDialogControllerImpl::CreateWrapper(
    DialogSection section) {
  if (IsPayingWithWallet() && full_wallet_ &&
      full_wallet_->required_actions().empty()) {
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
  if (!IsASuggestionItemKey(item_key) || IsManuallyEditingSection(section))
    return scoped_ptr<DataModelWrapper>();

  if (IsPayingWithWallet()) {
    if (section == SECTION_CC_BILLING) {
      return scoped_ptr<DataModelWrapper>(
          new WalletInstrumentWrapper(ActiveInstrument()));
    }

    if (section == SECTION_SHIPPING) {
      return scoped_ptr<DataModelWrapper>(
          new WalletAddressWrapper(ActiveShippingAddress()));
    }

    return scoped_ptr<DataModelWrapper>();
  }

  if (section == SECTION_CC) {
    CreditCard* card = GetManager()->GetCreditCardByGUID(item_key);
    DCHECK(card);
    return scoped_ptr<DataModelWrapper>(new AutofillCreditCardWrapper(card));
  }

  AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
  DCHECK(profile);
  size_t variant = GetSelectedVariantForModel(*model);
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

gfx::Image AutofillDialogControllerImpl::ExtraSuggestionIconForSection(
    DialogSection section) const {
  if (section == SECTION_CC || section == SECTION_CC_BILLING)
    return IconForField(CREDIT_CARD_VERIFICATION_CODE, string16());

  return gfx::Image();
}

void AutofillDialogControllerImpl::EditClickedForSection(
    DialogSection section) {
  ScopedViewUpdates updates(view_.get());
  scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
  SetEditingExistingData(section, true);

  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  for (DetailInputs::iterator it = inputs->begin(); it != inputs->end(); ++it) {
    it->editable = InputIsEditable(*it, section);
  }
  model->FillInputs(inputs);

  UpdateSection(section);

  GetMetricLogger().LogDialogUiEvent(
      GetDialogType(), common::DialogSectionToUiEditEvent(section));
}

gfx::Image AutofillDialogControllerImpl::IconForField(
    ServerFieldType type, const string16& user_input) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (type == CREDIT_CARD_VERIFICATION_CODE)
    return rb.GetImageNamed(IDR_CREDIT_CARD_CVC_HINT);

  if (type == CREDIT_CARD_NUMBER) {
    const int input_card_idr = CreditCard::IconResourceId(
        CreditCard::GetCreditCardType(user_input));
    if (input_card_idr != IDR_AUTOFILL_CC_GENERIC)
      return rb.GetImageNamed(input_card_idr);

    // When the credit card type is unknown, no image should be shown. However,
    // to simplify the view code on Mac, save space for the credit card image by
    // returning a transparent image of the appropriate size.
    gfx::ImageSkia image = *rb.GetImageSkiaNamed(input_card_idr);
    return
        gfx::Image(gfx::ImageSkiaOperations::CreateTransparentImage(image, 0));
  }

  return gfx::Image();
}

// TODO(estade): Replace all the error messages here with more helpful and
// translateable ones. TODO(groby): Also add tests.
string16 AutofillDialogControllerImpl::InputValidityMessage(
    DialogSection section,
    ServerFieldType type,
    const string16& value) {
  // If the field is edited, clear any Wallet errors.
  if (IsPayingWithWallet()) {
    WalletValidationErrors::iterator it = wallet_errors_.find(section);
    if (it != wallet_errors_.end()) {
      TypeErrorInputMap::const_iterator iter = it->second.find(type);
      if (iter != it->second.end()) {
        if (iter->second.second == value)
          return iter->second.first;
        it->second.erase(type);
      }
    }
  }

  switch (AutofillType(type).GetStorableType()) {
    case EMAIL_ADDRESS:
      if (!value.empty() && !IsValidEmailAddress(value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_EMAIL_ADDRESS);
      }
      break;

    case CREDIT_CARD_NUMBER: {
      if (!value.empty()) {
        base::string16 message = CreditCardNumberValidityMessage(value);
        if (!message.empty())
          return message;
      }
      break;
    }

    case CREDIT_CARD_EXP_MONTH:
      if (!InputWasEdited(CREDIT_CARD_EXP_MONTH, value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_MISSING_VALUE);
      }
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      if (!InputWasEdited(CREDIT_CARD_EXP_4_DIGIT_YEAR, value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_MISSING_VALUE);
      }
      break;

    case CREDIT_CARD_VERIFICATION_CODE:
      if (!value.empty() && !autofill::IsValidCreditCardSecurityCode(value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_SECURITY_CODE);
      }
      break;

    case ADDRESS_HOME_LINE1:
      break;

    case ADDRESS_HOME_LINE2:
      return base::string16();  // Line 2 is optional - always valid.

    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_COUNTRY:
      break;

    case ADDRESS_HOME_STATE:
      if (!value.empty() && !autofill::IsValidState(value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_REGION);
      }
      break;

    case ADDRESS_HOME_ZIP:
      if (!value.empty() && !autofill::IsValidZip(value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_ZIP_CODE);
      }
      break;

    case NAME_FULL:
      // Wallet requires a first and last billing name.
      if (section == SECTION_CC_BILLING && !value.empty() &&
          !IsCardHolderNameValidForWallet(value)) {
        DCHECK(IsPayingWithWallet());
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_WALLET_REQUIRES_TWO_NAMES);
      }
      break;

    case PHONE_HOME_WHOLE_NUMBER:  // Used in shipping section.
      break;

    case PHONE_BILLING_WHOLE_NUMBER:  // Used in billing section.
      break;

    default:
      NOTREACHED();  // Trying to validate unknown field.
      break;
  }

  return value.empty() ?
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_VALIDATION_MISSING_VALUE) :
      base::string16();
}

// TODO(estade): Replace all the error messages here with more helpful and
// translateable ones. TODO(groby): Also add tests.
ValidityData AutofillDialogControllerImpl::InputsAreValid(
    DialogSection section,
    const DetailOutputMap& inputs,
    ValidationType validation_type) {
  ValidityData invalid_messages;
  std::map<ServerFieldType, string16> field_values;
  for (DetailOutputMap::const_iterator iter = inputs.begin();
       iter != inputs.end(); ++iter) {
    const ServerFieldType type = iter->first->type;

    // Skip empty/unchanged fields in edit mode. Ignore country code as it
    // always has a value.
    if (validation_type == VALIDATE_EDIT &&
        !InputWasEdited(type, iter->second) &&
        ComboboxModelForAutofillType(type) != &country_combobox_model_) {
      continue;
    }

    string16 message = InputValidityMessage(section, type, iter->second);
    if (!message.empty())
      invalid_messages[type] = message;
    else
      field_values[type] = iter->second;
  }

  // Validate the date formed by month and year field. (Autofill dialog is
  // never supposed to have 2-digit years, so not checked).
  if (field_values.count(CREDIT_CARD_EXP_4_DIGIT_YEAR) &&
      field_values.count(CREDIT_CARD_EXP_MONTH) &&
      InputWasEdited(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                     field_values[CREDIT_CARD_EXP_4_DIGIT_YEAR]) &&
      InputWasEdited(CREDIT_CARD_EXP_MONTH,
                     field_values[CREDIT_CARD_EXP_MONTH]) &&
      !IsCreditCardExpirationValid(field_values[CREDIT_CARD_EXP_4_DIGIT_YEAR],
                                   field_values[CREDIT_CARD_EXP_MONTH])) {
    // The dialog shows the same error message for the month and year fields.
    invalid_messages[CREDIT_CARD_EXP_4_DIGIT_YEAR] = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_DATE);
    invalid_messages[CREDIT_CARD_EXP_MONTH] = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_DATE);
  }

  // If there is a credit card number and a CVC, validate them together.
  if (field_values.count(CREDIT_CARD_NUMBER) &&
      field_values.count(CREDIT_CARD_VERIFICATION_CODE) &&
      !invalid_messages.count(CREDIT_CARD_NUMBER) &&
      !autofill::IsValidCreditCardSecurityCode(
          field_values[CREDIT_CARD_VERIFICATION_CODE],
          field_values[CREDIT_CARD_NUMBER])) {
    invalid_messages[CREDIT_CARD_VERIFICATION_CODE] =
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_SECURITY_CODE);
  }

  // Validate the shipping phone number against the country code of the address.
  if (field_values.count(ADDRESS_HOME_COUNTRY) &&
      field_values.count(PHONE_HOME_WHOLE_NUMBER)) {
    i18n::PhoneObject phone_object(
        field_values[PHONE_HOME_WHOLE_NUMBER],
        AutofillCountry::GetCountryCode(
            field_values[ADDRESS_HOME_COUNTRY],
            g_browser_process->GetApplicationLocale()));
    if (!phone_object.IsValidNumber()) {
      invalid_messages[PHONE_HOME_WHOLE_NUMBER] = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_PHONE_NUMBER);
    }
  }

  // Validate the billing phone number against the country code of the address.
  if (field_values.count(ADDRESS_BILLING_COUNTRY) &&
      field_values.count(PHONE_BILLING_WHOLE_NUMBER)) {
    i18n::PhoneObject phone_object(
        field_values[PHONE_BILLING_WHOLE_NUMBER],
        AutofillCountry::GetCountryCode(
            field_values[ADDRESS_BILLING_COUNTRY],
            g_browser_process->GetApplicationLocale()));
    if (!phone_object.IsValidNumber()) {
      invalid_messages[PHONE_BILLING_WHOLE_NUMBER] = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_PHONE_NUMBER);
    }
  }

  return invalid_messages;
}

void AutofillDialogControllerImpl::UserEditedOrActivatedInput(
    DialogSection section,
    const DetailInput* input,
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
  if (!was_edit && popup_controller_.get()) {
    HidePopup();
    return;
  }

  std::vector<string16> popup_values, popup_labels, popup_icons;
  if (common::IsCreditCardType(input->type)) {
    GetManager()->GetCreditCardSuggestions(AutofillType(input->type),
                                           field_contents,
                                           &popup_values,
                                           &popup_labels,
                                           &popup_icons,
                                           &popup_guids_);
  } else {
    std::vector<ServerFieldType> field_types;
    field_types.push_back(EMAIL_ADDRESS);
    for (DetailInputs::const_iterator iter = requested_shipping_fields_.begin();
         iter != requested_shipping_fields_.end(); ++iter) {
      field_types.push_back(iter->type);
    }
    GetManager()->GetProfileSuggestions(AutofillType(input->type),
                                        field_contents,
                                        false,
                                        field_types,
                                        &popup_values,
                                        &popup_labels,
                                        &popup_icons,
                                        &popup_guids_);
  }

  if (popup_values.empty()) {
    HidePopup();
    return;
  }

  // TODO(estade): do we need separators and control rows like 'Clear
  // Form'?
  std::vector<int> popup_ids;
  for (size_t i = 0; i < popup_guids_.size(); ++i) {
    popup_ids.push_back(i);
  }

  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_,
      weak_ptr_factory_.GetWeakPtr(),
      parent_view,
      content_bounds,
      base::i18n::IsRTL() ?
          base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT);
  popup_controller_->set_hide_on_outside_click(true);
  popup_controller_->Show(popup_values,
                          popup_labels,
                          popup_icons,
                          popup_ids);
  input_showing_popup_ = input;
}

void AutofillDialogControllerImpl::FocusMoved() {
  HidePopup();
}

gfx::Image AutofillDialogControllerImpl::SplashPageImage() const {
  // Only show the splash page the first few times the dialog is opened.
  int show_count =
      profile_->GetPrefs()->GetInteger(::prefs::kAutofillDialogShowCount);
  if (show_count <= 4) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_PRODUCT_LOGO_NAME_48);
  }

  return gfx::Image();
}

void AutofillDialogControllerImpl::ViewClosed() {
  GetManager()->RemoveObserver(this);

  // Called from here rather than in ~AutofillDialogControllerImpl as this
  // relies on virtual methods that change to their base class in the dtor.
  MaybeShowCreditCardBubble();

  delete this;
}

std::vector<DialogNotification> AutofillDialogControllerImpl::
    CurrentNotifications() {
  std::vector<DialogNotification> notifications;

  if (IsPayingWithWallet() && !wallet::IsUsingProd()) {
    notifications.push_back(DialogNotification(
        DialogNotification::DEVELOPER_WARNING,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_NOT_PROD_WARNING)));
  }

  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    notifications.push_back(DialogNotification(
        DialogNotification::SECURITY_WARNING,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECURITY_WARNING)));
  }

  if (!invoked_from_same_origin_) {
    notifications.push_back(DialogNotification(
        DialogNotification::SECURITY_WARNING,
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_DIALOG_SITE_WARNING,
                                   UTF8ToUTF16(source_url_.host()))));
  }

  // TODO(dbeam): figure out a way to dismiss this error after a while.
  if (wallet_error_notification_)
    notifications.push_back(*wallet_error_notification_);

  if (IsSubmitPausedOn(wallet::VERIFY_CVV)) {
    notifications.push_back(DialogNotification(
        DialogNotification::REQUIRED_ACTION,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_VERIFY_CVV)));
  }

  if (!wallet_server_validation_recoverable_) {
    notifications.push_back(DialogNotification(
        DialogNotification::REQUIRED_ACTION,
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_FAILED_TO_SAVE_WALLET_DATA)));
  }

  if (choose_another_instrument_or_address_) {
    notifications.push_back(DialogNotification(
        DialogNotification::REQUIRED_ACTION,
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_CHOOSE_DIFFERENT_WALLET_INSTRUMENT)));
  }

  if (should_show_wallet_promo_ && notifications.empty()) {
    if (IsPayingWithWallet() && HasCompleteWallet()) {
      notifications.push_back(DialogNotification(
          DialogNotification::EXPLANATORY_MESSAGE,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DIALOG_DETAILS_FROM_WALLET)));
    } else if ((IsPayingWithWallet() && !HasCompleteWallet()) ||
               has_shown_wallet_usage_confirmation_) {
      DialogNotification notification(
          DialogNotification::WALLET_USAGE_CONFIRMATION,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DIALOG_SAVE_DETAILS_IN_WALLET));
      notification.set_tooltip_text(
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DIALOG_SAVE_IN_WALLET_TOOLTIP));
      notification.set_checked(account_chooser_model_.WalletIsSelected());
      notification.set_interactive(!is_submitting_);
      notifications.push_back(notification);
      has_shown_wallet_usage_confirmation_ = true;
    }
  }

  return notifications;
}

void AutofillDialogControllerImpl::SignInLinkClicked() {
  ScopedViewUpdates updates(view_.get());

  if (signin_registrar_.IsEmpty()) {
    // Start sign in.
    DCHECK(!IsPayingWithWallet());

    content::Source<content::NavigationController> source(view_->ShowSignIn());
    signin_registrar_.Add(
        this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);
    view_->UpdateAccountChooser();

    GetMetricLogger().LogDialogUiEvent(
        GetDialogType(), AutofillMetrics::DIALOG_UI_SIGNIN_SHOWN);
  } else {
    HideSignIn();
  }
}

void AutofillDialogControllerImpl::NotificationCheckboxStateChanged(
    DialogNotification::Type type, bool checked) {
  if (type == DialogNotification::WALLET_USAGE_CONFIRMATION) {
    if (checked)
      account_chooser_model_.SelectActiveWalletAccount();
    else
      account_chooser_model_.SelectUseAutofill();
  }
}

void AutofillDialogControllerImpl::LegalDocumentLinkClicked(
    const ui::Range& range) {
  for (size_t i = 0; i < legal_document_link_ranges_.size(); ++i) {
    if (legal_document_link_ranges_[i] == range) {
      OpenTabWithUrl(wallet_items_->legal_documents()[i]->url());
      return;
    }
  }

  NOTREACHED();
}

void AutofillDialogControllerImpl::OverlayButtonPressed() {
  DCHECK(is_submitting_);
  DCHECK(full_wallet_);
  profile_->GetPrefs()->SetBoolean(::prefs::kAutofillDialogHasPaidWithWallet,
                                   true);
  FinishSubmit();
}

bool AutofillDialogControllerImpl::OnCancel() {
  HidePopup();
  if (!is_submitting_)
    LogOnCancelMetrics();
  callback_.Run(NULL, std::string());
  return true;
}

bool AutofillDialogControllerImpl::OnAccept() {
  choose_another_instrument_or_address_ = false;
  wallet_server_validation_recoverable_ = true;
  HidePopup();

  SetIsSubmitting(true);
  if (IsSubmitPausedOn(wallet::VERIFY_CVV)) {
    DCHECK(!active_instrument_id_.empty());
    GetWalletClient()->AuthenticateInstrument(
        active_instrument_id_,
        UTF16ToUTF8(view_->GetCvc()));
  } else if (IsPayingWithWallet()) {
    // TODO(dbeam): disallow interacting with the dialog while submitting.
    // http://crbug.com/230932
    AcceptLegalDocuments();
  } else {
    FinishSubmit();
  }

  return false;
}

Profile* AutofillDialogControllerImpl::profile() {
  return profile_;
}

content::WebContents* AutofillDialogControllerImpl::GetWebContents() {
  return web_contents();
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPopupDelegate implementation.

void AutofillDialogControllerImpl::OnPopupShown(
    content::RenderWidgetHost::KeyPressEventCallback* callback) {
  GetMetricLogger().LogDialogPopupEvent(
      GetDialogType(), AutofillMetrics::DIALOG_POPUP_SHOWN);
}

void AutofillDialogControllerImpl::OnPopupHidden(
    content::RenderWidgetHost::KeyPressEventCallback* callback) {}

bool AutofillDialogControllerImpl::ShouldRepostEvent(
    const ui::MouseEvent& event) {
  // If the event would be reposted inside |input_showing_popup_|, just ignore.
  return !view_->HitTestInput(*input_showing_popup_, event.location());
}

void AutofillDialogControllerImpl::DidSelectSuggestion(int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::DidAcceptSuggestion(const string16& value,
                                                       int identifier) {
  ScopedViewUpdates updates(view_.get());
  const PersonalDataManager::GUIDPair& pair = popup_guids_[identifier];

  scoped_ptr<DataModelWrapper> wrapper;
  if (common::IsCreditCardType(input_showing_popup_->type)) {
    wrapper.reset(new AutofillCreditCardWrapper(
        GetManager()->GetCreditCardByGUID(pair.first)));
  } else {
    wrapper.reset(new AutofillProfileWrapper(
        GetManager()->GetProfileByGUID(pair.first), pair.second));
  }

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    wrapper->FillInputs(MutableRequestedFieldsForSection(section));
    view_->FillSection(section, *input_showing_popup_);
  }

  GetMetricLogger().LogDialogPopupEvent(
      GetDialogType(), AutofillMetrics::DIALOG_POPUP_FORM_FILLED);

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
    should_show_wallet_promo_ = false;
    account_chooser_model_.SelectActiveWalletAccount();
    signin_helper_.reset(new wallet::WalletSigninHelper(
        this, profile_->GetRequestContext()));
    signin_helper_->StartWalletCookieValueFetch();
    HideSignIn();
  }
}

////////////////////////////////////////////////////////////////////////////////
// SuggestionsMenuModelDelegate implementation.

void AutofillDialogControllerImpl::SuggestionItemSelected(
    SuggestionsMenuModel* model,
    size_t index) {
  ScopedViewUpdates updates(view_.get());

  if (model->GetItemKeyAt(index) == kManageItemsKey) {
    GURL url;
    if (!IsPayingWithWallet()) {
      GURL settings_url(chrome::kChromeUISettingsURL);
      url = settings_url.Resolve(chrome::kAutofillSubPage);
    } else {
      // Reset |last_wallet_items_fetch_timestamp_| to ensure that the Wallet
      // data is refreshed as soon as the user switches back to this tab after
      // potentially editing his data.
      last_wallet_items_fetch_timestamp_ = base::TimeTicks();
      url = SectionForSuggestionsMenuModel(*model) == SECTION_SHIPPING ?
          wallet::GetManageAddressesUrl() : wallet::GetManageInstrumentsUrl();
    }

    OpenTabWithUrl(url);
    return;
  }

  model->SetCheckedIndex(index);
  DialogSection section = SectionForSuggestionsMenuModel(*model);
  ResetSectionInput(section);
  ShowEditUiIfBadSuggestion(section);
  UpdateSection(section);
  UpdateForErrors();

  LogSuggestionItemSelectedMetric(*model);
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
  DCHECK(!risk_data_.empty());
  return risk_data_;
}

std::string AutofillDialogControllerImpl::GetWalletCookieValue() const {
  return wallet_cookie_value_;
}

bool AutofillDialogControllerImpl::IsShippingAddressRequired() const {
  return cares_about_shipping_;
}

void AutofillDialogControllerImpl::OnDidAcceptLegalDocuments() {
  DCHECK(is_submitting_ && IsPayingWithWallet());
  has_accepted_legal_documents_ = true;
  LoadRiskFingerprintData();
}

void AutofillDialogControllerImpl::OnDidAuthenticateInstrument(bool success) {
  DCHECK(is_submitting_ && IsPayingWithWallet());

  // TODO(dbeam): use the returned full wallet. b/8332329
  if (success) {
    GetFullWallet();
  } else {
    DisableWallet(wallet::WalletClient::UNKNOWN_ERROR);
    SuggestionsUpdated();
  }
}

void AutofillDialogControllerImpl::OnDidGetFullWallet(
    scoped_ptr<wallet::FullWallet> full_wallet) {
  DCHECK(is_submitting_ && IsPayingWithWallet());
  ScopedViewUpdates updates(view_.get());

  full_wallet_ = full_wallet.Pass();

  if (full_wallet_->required_actions().empty()) {
    FinishSubmit();
    return;
  }

  switch (full_wallet_->required_actions()[0]) {
    case wallet::CHOOSE_ANOTHER_INSTRUMENT_OR_ADDRESS:
      choose_another_instrument_or_address_ = true;
      SetIsSubmitting(false);
      GetWalletItems();
      view_->UpdateNotificationArea();
      view_->UpdateButtonStrip();
      break;

    case wallet::VERIFY_CVV:
      SuggestionsUpdated();
      break;

    default:
      DisableWallet(wallet::WalletClient::UNKNOWN_ERROR);
      break;
  }
}

void AutofillDialogControllerImpl::OnPassiveSigninSuccess(
    const std::string& username) {
  const string16 username16 = UTF8ToUTF16(username);
  signin_helper_->StartWalletCookieValueFetch();
  account_chooser_model_.SetActiveWalletAccountName(username16);
}

void AutofillDialogControllerImpl::OnUserNameFetchSuccess(
    const std::string& username) {
  ScopedViewUpdates updates(view_.get());
  const string16 username16 = UTF8ToUTF16(username);
  signin_helper_.reset();
  account_chooser_model_.SetActiveWalletAccountName(username16);
  OnWalletOrSigninUpdate();
}

void AutofillDialogControllerImpl::OnPassiveSigninFailure(
    const GoogleServiceAuthError& error) {
  // TODO(aruslan): report an error.
  LOG(ERROR) << "failed to passively sign in: " << error.ToString();
  OnWalletSigninError();
}

void AutofillDialogControllerImpl::OnUserNameFetchFailure(
    const GoogleServiceAuthError& error) {
  // TODO(aruslan): report an error.
  LOG(ERROR) << "failed to fetch the user account name: " << error.ToString();
  OnWalletSigninError();
}

void AutofillDialogControllerImpl::OnDidFetchWalletCookieValue(
    const std::string& cookie_value) {
  wallet_cookie_value_ = cookie_value;
  signin_helper_.reset();
  GetWalletItems();
}

void AutofillDialogControllerImpl::OnDidGetWalletItems(
    scoped_ptr<wallet::WalletItems> wallet_items) {
  legal_documents_text_.clear();
  legal_document_link_ranges_.clear();
  has_accepted_legal_documents_ = false;

  wallet_items_ = wallet_items.Pass();
  OnWalletOrSigninUpdate();
}

void AutofillDialogControllerImpl::OnDidSaveToWallet(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions,
    const std::vector<wallet::FormFieldError>& form_field_errors) {
  DCHECK(is_submitting_ && IsPayingWithWallet());

  if (required_actions.empty()) {
    if (!address_id.empty())
      active_address_id_ = address_id;
    if (!instrument_id.empty())
      active_instrument_id_ = instrument_id;
    GetFullWallet();
  } else {
    OnWalletFormFieldError(form_field_errors);
    HandleSaveOrUpdateRequiredActions(required_actions);
  }
}

void AutofillDialogControllerImpl::OnWalletError(
    wallet::WalletClient::ErrorType error_type) {
  DisableWallet(error_type);
}

////////////////////////////////////////////////////////////////////////////////
// PersonalDataManagerObserver implementation.

void AutofillDialogControllerImpl::OnPersonalDataChanged() {
  if (is_submitting_)
    return;

  SuggestionsUpdated();
}

////////////////////////////////////////////////////////////////////////////////
// AccountChooserModelDelegate implementation.

void AutofillDialogControllerImpl::AccountChoiceChanged() {
  ScopedViewUpdates updates(view_.get());

  if (is_submitting_)
    GetWalletClient()->CancelRequests();

  SetIsSubmitting(false);

  SuggestionsUpdated();
  UpdateAccountChooserView();
}

void AutofillDialogControllerImpl::UpdateAccountChooserView() {
  if (view_) {
    ScopedViewUpdates updates(view_.get());
    view_->UpdateAccountChooser();
    view_->UpdateNotificationArea();
  }
}

////////////////////////////////////////////////////////////////////////////////

bool AutofillDialogControllerImpl::HandleKeyPressEventInInput(
    const content::NativeWebKeyboardEvent& event) {
  if (popup_controller_.get())
    return popup_controller_->HandleKeyPressEvent(event);

  return false;
}

bool AutofillDialogControllerImpl::RequestingCreditCardInfo() const {
  DCHECK_GT(form_structure_.field_count(), 0U);

  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillType type = form_structure_.field(i)->Type();
    if (common::IsCreditCardType(type.GetStorableType()))
      return true;
  }

  return false;
}

bool AutofillDialogControllerImpl::TransmissionWillBeSecure() const {
  return source_url_.SchemeIs(content::kHttpsScheme);
}

void AutofillDialogControllerImpl::ShowNewCreditCardBubble(
    scoped_ptr<CreditCard> new_card,
    scoped_ptr<AutofillProfile> billing_profile) {
#if !defined(OS_ANDROID)
  NewCreditCardBubbleController::Show(profile(),
                                      new_card.Pass(),
                                      billing_profile.Pass());
#endif
}

AutofillDialogControllerImpl::AutofillDialogControllerImpl(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const DialogType dialog_type,
    const base::Callback<void(const FormStructure*,
                              const std::string&)>& callback)
    : WebContentsObserver(contents),
      profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      initial_user_state_(AutofillMetrics::DIALOG_USER_STATE_UNKNOWN),
      dialog_type_(dialog_type),
      form_structure_(form_structure),
      invoked_from_same_origin_(true),
      source_url_(source_url),
      callback_(callback),
      account_chooser_model_(this, profile_->GetPrefs(), metric_logger_,
                             dialog_type),
      wallet_client_(profile_->GetRequestContext(), this),
      suggested_email_(this),
      suggested_cc_(this),
      suggested_billing_(this),
      suggested_cc_billing_(this),
      suggested_shipping_(this),
      cares_about_shipping_(true),
      input_showing_popup_(NULL),
      weak_ptr_factory_(this),
      should_show_wallet_promo_(!profile_->GetPrefs()->GetBoolean(
          ::prefs::kAutofillDialogHasPaidWithWallet)),
      has_shown_wallet_usage_confirmation_(false),
      has_accepted_legal_documents_(false),
      is_submitting_(false),
      choose_another_instrument_or_address_(false),
      wallet_server_validation_recoverable_(true),
      data_was_passed_back_(false),
      was_ui_latency_logged_(false) {
  // TODO(estade): remove duplicates from |form_structure|?
  DCHECK(!callback_.is_null());
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
  return account_chooser_model_.WalletIsSelected() &&
         SignedInState() == SIGNED_IN;
}

void AutofillDialogControllerImpl::LoadRiskFingerprintData() {
  risk_data_.clear();

  uint64 obfuscated_gaia_id = 0;
  bool success = base::StringToUint64(wallet_items_->obfuscated_gaia_id(),
                                      &obfuscated_gaia_id);
  DCHECK(success);

  gfx::Rect window_bounds;
  window_bounds = GetBaseWindowForWebContents(web_contents())->GetBounds();

  PrefService* user_prefs = profile_->GetPrefs();
  std::string charset = user_prefs->GetString(::prefs::kDefaultCharset);
  std::string accept_languages =
      user_prefs->GetString(::prefs::kAcceptLanguages);
  base::Time install_time = base::Time::FromTimeT(
      g_browser_process->local_state()->GetInt64(::prefs::kInstallDate));

  risk::GetFingerprint(
      obfuscated_gaia_id, window_bounds, *web_contents(),
      chrome::VersionInfo().Version(), charset, accept_languages, install_time,
      dialog_type_, g_browser_process->GetApplicationLocale(),
      base::Bind(&AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData(
    scoped_ptr<risk::Fingerprint> fingerprint) {
  DCHECK(AreLegalDocumentsCurrent());

  std::string proto_data;
  fingerprint->SerializeToString(&proto_data);
  bool success = base::Base64Encode(proto_data, &risk_data_);
  DCHECK(success);

  SubmitWithWallet();
}

void AutofillDialogControllerImpl::OpenTabWithUrl(const GURL& url) {
  chrome::NavigateParams params(
      chrome::FindBrowserWithWebContents(web_contents()),
      url,
      content::PAGE_TRANSITION_AUTO_BOOKMARK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

bool AutofillDialogControllerImpl::IsEditingExistingData(
    DialogSection section) const {
  return section_editing_state_.count(section) > 0;
}

bool AutofillDialogControllerImpl::IsManuallyEditingSection(
    DialogSection section) const {
  return IsEditingExistingData(section) ||
         SuggestionsMenuModelForSection(section)->
             GetItemKeyForCheckedItem() == kAddNewItemKey;
}

void AutofillDialogControllerImpl::OnWalletSigninError() {
  signin_helper_.reset();
  account_chooser_model_.SetHadWalletSigninError();
  GetWalletClient()->CancelRequests();
  LogDialogLatencyToShow();
}

void AutofillDialogControllerImpl::DisableWallet(
    wallet::WalletClient::ErrorType error_type) {
  signin_helper_.reset();
  wallet_items_.reset();
  wallet_errors_.clear();
  GetWalletClient()->CancelRequests();
  SetIsSubmitting(false);
  wallet_error_notification_ = GetWalletError(error_type);
  account_chooser_model_.SetHadWalletError();
}

void AutofillDialogControllerImpl::SuggestionsUpdated() {
  ScopedViewUpdates updates(view_.get());

  const DetailOutputMap snapshot = TakeUserInputSnapshot();

  suggested_email_.Reset();
  suggested_cc_.Reset();
  suggested_billing_.Reset();
  suggested_cc_billing_.Reset();
  suggested_shipping_.Reset();
  HidePopup();

  suggested_shipping_.AddKeyedItem(
      kSameAsBillingKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING));

  if (IsPayingWithWallet()) {
    if (!account_chooser_model_.active_wallet_account_name().empty()) {
      suggested_email_.AddKeyedItem(
          base::IntToString(0),
          account_chooser_model_.active_wallet_account_name());
    }

    const std::vector<wallet::Address*>& addresses =
        wallet_items_->addresses();
    for (size_t i = 0; i < addresses.size(); ++i) {
      std::string key = base::IntToString(i);
      suggested_shipping_.AddKeyedItemWithSublabel(
          key,
          addresses[i]->DisplayName(),
          addresses[i]->DisplayNameDetail());

      const std::string default_shipping_address_id =
          !previously_selected_shipping_address_id_.empty() ?
              previously_selected_shipping_address_id_ :
              wallet_items_->default_address_id();
      if (addresses[i]->object_id() == default_shipping_address_id)
        suggested_shipping_.SetCheckedItem(key);
    }
    previously_selected_shipping_address_id_.clear();

    if (!IsSubmitPausedOn(wallet::VERIFY_CVV)) {
      const std::vector<wallet::WalletItems::MaskedInstrument*>& instruments =
          wallet_items_->instruments();
      std::string first_active_instrument_key;
      std::string default_instrument_key;
      for (size_t i = 0; i < instruments.size(); ++i) {
        bool allowed = IsInstrumentAllowed(*instruments[i]);
        gfx::Image icon = instruments[i]->CardIcon();
        if (!allowed && !icon.IsEmpty()) {
          // Create a grayed disabled icon.
          SkBitmap disabled_bitmap = SkBitmapOperations::CreateHSLShiftedBitmap(
              *icon.ToSkBitmap(), kGrayImageShift);
          icon = gfx::Image(
              gfx::ImageSkia::CreateFrom1xBitmap(disabled_bitmap));
        }
        std::string key = base::IntToString(i);
        suggested_cc_billing_.AddKeyedItemWithSublabelAndIcon(
            key,
            instruments[i]->DisplayName(),
            instruments[i]->DisplayNameDetail(),
            icon);
        suggested_cc_billing_.SetEnabled(key, allowed);

        if (allowed) {
          if (first_active_instrument_key.empty())
            first_active_instrument_key = key;

          const std::string default_instrument_id =
              !previously_selected_instrument_id_.empty() ?
                  previously_selected_instrument_id_ :
                  wallet_items_->default_instrument_id();
          if (instruments[i]->object_id() == default_instrument_id)
            default_instrument_key = key;
        }
      }
      previously_selected_instrument_id_.clear();

      // TODO(estade): this should have a URL sublabel.
      suggested_cc_billing_.AddKeyedItem(
          kAddNewItemKey,
          l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_DETAILS));
      if (!wallet_items_->HasRequiredAction(wallet::SETUP_WALLET)) {
        suggested_cc_billing_.AddKeyedItemWithSublabel(
            kManageItemsKey,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DIALOG_MANAGE_BILLING_DETAILS),
                UTF8ToUTF16(wallet::GetManageInstrumentsUrl().host()));
      }

      // Determine which instrument item should be selected.
      if (!default_instrument_key.empty())
        suggested_cc_billing_.SetCheckedItem(default_instrument_key);
      else if (!first_active_instrument_key.empty())
        suggested_cc_billing_.SetCheckedItem(first_active_instrument_key);
      else
        suggested_cc_billing_.SetCheckedItem(kAddNewItemKey);
    }
  } else {
    PersonalDataManager* manager = GetManager();
    const std::vector<CreditCard*>& cards = manager->GetCreditCards();
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    for (size_t i = 0; i < cards.size(); ++i) {
      if (!HasCompleteAndVerifiedData(*cards[i], requested_cc_fields_))
        continue;

      suggested_cc_.AddKeyedItemWithIcon(
          cards[i]->guid(),
          cards[i]->Label(),
          rb.GetImageNamed(CreditCard::IconResourceId(cards[i]->type())));
    }

    const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
    const std::string app_locale = g_browser_process->GetApplicationLocale();
    for (size_t i = 0; i < profiles.size(); ++i) {
      if (!HasCompleteAndVerifiedData(*profiles[i],
                                      requested_shipping_fields_) ||
          HasInvalidAddress(*profiles[i])) {
        continue;
      }

      // Add all email addresses.
      std::vector<string16> values;
      profiles[i]->GetMultiInfo(
          AutofillType(EMAIL_ADDRESS), app_locale, &values);
      for (size_t j = 0; j < values.size(); ++j) {
        if (IsValidEmailAddress(values[j]))
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
        kAddNewItemKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_CREDIT_CARD));
    suggested_cc_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_CREDIT_CARD));
    suggested_billing_.AddKeyedItem(
        kAddNewItemKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_ADDRESS));
    suggested_billing_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_BILLING_ADDRESS));
  }

  suggested_email_.AddKeyedItem(
      kAddNewItemKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_EMAIL_ADDRESS));
  if (!IsPayingWithWallet()) {
    suggested_email_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_EMAIL_ADDRESS));
  }

  suggested_shipping_.AddKeyedItem(
      kAddNewItemKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_SHIPPING_ADDRESS));
  if (!IsPayingWithWallet()) {
    suggested_shipping_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_SHIPPING_ADDRESS));
  } else if (!wallet_items_->HasRequiredAction(wallet::SETUP_WALLET)) {
    suggested_shipping_.AddKeyedItemWithSublabel(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_SHIPPING_ADDRESS),
        UTF8ToUTF16(wallet::GetManageAddressesUrl().host()));
  }

  if (!IsPayingWithWallet()) {
    for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
      DialogSection section = static_cast<DialogSection>(i);
      if (!SectionIsActive(section))
        continue;

      // Set the starting choice for the menu. First set to the default in case
      // the GUID saved in prefs refers to a profile that no longer exists.
      std::string guid;
      int variant;
      GetDefaultAutofillChoice(section, &guid, &variant);
      SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
      model->SetCheckedItemNthWithKey(guid, variant + 1);
      if (GetAutofillChoice(section, &guid, &variant))
        model->SetCheckedItemNthWithKey(guid, variant + 1);
    }
  }

  if (view_)
    view_->ModelChanged();

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    ResetSectionInput(static_cast<DialogSection>(i));
  }

  RestoreUserInputFromSnapshot(snapshot);

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    ShowEditUiIfBadSuggestion(section);
    UpdateSection(section);
  }

  UpdateForErrors();
}

void AutofillDialogControllerImpl::FillOutputForSectionWithComparator(
    DialogSection section,
    const InputFieldComparator& compare) {
  const DetailInputs& inputs = RequestedFieldsForSection(section);

  // Email is hidden while using Wallet, special case it.
  if (section == SECTION_EMAIL && IsPayingWithWallet()) {
    AutofillProfile profile;
    profile.SetRawInfo(EMAIL_ADDRESS,
                       account_chooser_model_.active_wallet_account_name());
    AutofillProfileWrapper profile_wrapper(&profile, 0);
    profile_wrapper.FillFormStructure(inputs, compare, &form_structure_);
    return;
  }

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
      card.set_origin(kAutofillDialogOrigin);
      FillFormGroupFromOutputs(output, &card);

      // The card holder name comes from the billing address section.
      card.SetRawInfo(CREDIT_CARD_NAME,
                      GetValueFromSection(SECTION_BILLING, NAME_FULL));

      if (ShouldSaveDetailsLocally()) {
        GetManager()->SaveImportedCreditCard(card);
        DCHECK(!profile()->IsOffTheRecord());
        newly_saved_card_.reset(new CreditCard(card));
      }

      AutofillCreditCardWrapper card_wrapper(&card);
      card_wrapper.FillFormStructure(inputs, compare, &form_structure_);

      // Again, CVC needs special-casing. Fill it in directly from |output|.
      SetCvcResult(GetValueForType(output, CREDIT_CARD_VERIFICATION_CODE));
    } else {
      AutofillProfile profile;
      profile.set_origin(kAutofillDialogOrigin);
      FillFormGroupFromOutputs(output, &profile);

      // For billing, the email address comes from the separate email section.
      if (section == SECTION_BILLING) {
        profile.SetRawInfo(EMAIL_ADDRESS,
                           GetValueFromSection(SECTION_EMAIL, EMAIL_ADDRESS));
      }

      if (ShouldSaveDetailsLocally())
        SaveProfileGleanedFromSection(profile, section);

      AutofillProfileWrapper profile_wrapper(&profile, 0);
      profile_wrapper.FillFormStructure(inputs, compare, &form_structure_);
    }
  }
}

void AutofillDialogControllerImpl::FillOutputForSection(DialogSection section) {
  FillOutputForSectionWithComparator(
      section, base::Bind(common::DetailInputMatchesField, section));
}

bool AutofillDialogControllerImpl::FormStructureCaresAboutSection(
    DialogSection section) const {
  // For now, only SECTION_SHIPPING may be omitted due to a site not asking for
  // any of the fields.
  if (section == SECTION_SHIPPING)
    return cares_about_shipping_;

  return true;
}

void AutofillDialogControllerImpl::SetCvcResult(const string16& cvc) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    if (field->Type().GetStorableType() == CREDIT_CARD_VERIFICATION_CODE) {
      field->value = cvc;
      break;
    }
  }
}

string16 AutofillDialogControllerImpl::GetValueFromSection(
    DialogSection section,
    ServerFieldType type) {
  DCHECK(SectionIsActive(section));

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  if (wrapper)
    return wrapper->GetInfo(AutofillType(type));

  DetailOutputMap output;
  view_->GetUserInput(section, &output);
  for (DetailOutputMap::iterator iter = output.begin(); iter != output.end();
       ++iter) {
    if (iter->first->type == type)
      return iter->second;
  }

  return string16();
}

void AutofillDialogControllerImpl::SaveProfileGleanedFromSection(
    const AutofillProfile& profile,
    DialogSection section) {
  if (section == SECTION_EMAIL) {
    // Save the email address to the existing (suggested) billing profile. If
    // there is no existing profile, the newly created one will pick up this
    // email, so in that case do nothing.
    scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(SECTION_BILLING);
    if (wrapper) {
      std::string item_key = SuggestionsMenuModelForSection(SECTION_BILLING)->
          GetItemKeyForCheckedItem();
      AutofillProfile* billing_profile =
          GetManager()->GetProfileByGUID(item_key);
      billing_profile->OverwriteWithOrAddTo(
          profile,
          g_browser_process->GetApplicationLocale());
    }
  } else {
    GetManager()->SaveImportedProfile(profile);
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

const SuggestionsMenuModel* AutofillDialogControllerImpl::
    SuggestionsMenuModelForSection(DialogSection section) const {
  return const_cast<AutofillDialogControllerImpl*>(this)->
      SuggestionsMenuModelForSection(section);
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
  if (popup_controller_.get())
    popup_controller_->Hide();
  input_showing_popup_ = NULL;
}

void AutofillDialogControllerImpl::SetEditingExistingData(
    DialogSection section, bool editing) {
  if (editing)
    section_editing_state_.insert(section);
  else
    section_editing_state_.erase(section);
}

bool AutofillDialogControllerImpl::IsASuggestionItemKey(
    const std::string& key) const {
  return !key.empty() &&
      key != kAddNewItemKey &&
      key != kManageItemsKey &&
      key != kSameAsBillingKey;
}

bool AutofillDialogControllerImpl::IsManuallyEditingAnySection() const {
  for (size_t section = SECTION_MIN; section <= SECTION_MAX; ++section) {
    if (IsManuallyEditingSection(static_cast<DialogSection>(section)))
      return true;
  }
  return false;
}

base::string16 AutofillDialogControllerImpl::CreditCardNumberValidityMessage(
    const base::string16& number) const {
  if (!number.empty() && !autofill::IsValidCreditCardNumber(number)) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_NUMBER);
  }

  // Wallet only accepts MasterCard, Visa and Discover. No AMEX.
  if (IsPayingWithWallet() &&
      !IsWalletSupportedCard(CreditCard::GetCreditCardType(number))) {
    return l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_VALIDATION_CREDIT_CARD_NOT_SUPPORTED_BY_WALLET);
  }

  // Card number is good and supported.
  return base::string16();
}

bool AutofillDialogControllerImpl::InputIsEditable(
    const DetailInput& input,
    DialogSection section) const {
  if (input.type != CREDIT_CARD_NUMBER || !IsPayingWithWallet())
    return true;

  if (IsEditingExistingData(section))
    return false;

  return true;
}

bool AutofillDialogControllerImpl::AllSectionsAreValid() {
  for (size_t section = SECTION_MIN; section <= SECTION_MAX; ++section) {
    if (!SectionIsValid(static_cast<DialogSection>(section)))
      return false;
  }
  return true;
}

bool AutofillDialogControllerImpl::SectionIsValid(
    DialogSection section) {
  if (!IsManuallyEditingSection(section))
    return true;

  DetailOutputMap detail_outputs;
  view_->GetUserInput(section, &detail_outputs);
  return InputsAreValid(section, detail_outputs, VALIDATE_EDIT).empty();
}

bool AutofillDialogControllerImpl::IsCreditCardExpirationValid(
    const base::string16& year,
    const base::string16& month) const {
  // If the expiration is in the past as per the local clock, it's invalid.
  base::Time now = base::Time::Now();
  if (!autofill::IsValidCreditCardExpirationDate(year, month, now))
    return false;

  if (IsPayingWithWallet() && IsEditingExistingData(SECTION_CC_BILLING)) {
    const wallet::WalletItems::MaskedInstrument* instrument =
        ActiveInstrument();
    const std::string& locale = g_browser_process->GetApplicationLocale();
    int month_int;
    if (base::StringToInt(month, &month_int) &&
        instrument->status() ==
            wallet::WalletItems::MaskedInstrument::EXPIRED &&
        year ==
            instrument->GetInfo(
                AutofillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), locale) &&
        month_int == instrument->expiration_month()) {
      // Otherwise, if the user is editing an instrument that's deemed expired
      // by the Online Wallet server, mark it invalid on selection.
      return false;
    }
  }

  return true;
}

bool AutofillDialogControllerImpl::ShouldUseBillingForShipping() {
  return SectionIsActive(SECTION_SHIPPING) &&
      suggested_shipping_.GetItemKeyForCheckedItem() == kSameAsBillingKey;
}

bool AutofillDialogControllerImpl::ShouldSaveDetailsLocally() {
  // It's possible that the user checked [X] Save details locally before
  // switching payment methods, so only ask the view whether to save details
  // locally if that checkbox is showing (currently if not paying with wallet).
  // Also, if the user isn't editing any sections, there's no data to save
  // locally.
  return ShouldOfferToSaveInChrome() && view_->SaveDetailsLocally();
}

void AutofillDialogControllerImpl::SetIsSubmitting(bool submitting) {
  is_submitting_ = submitting;

  if (!submitting)
    full_wallet_.reset();

  if (view_) {
    ScopedViewUpdates updates(view_.get());
    view_->UpdateButtonStrip();
    view_->UpdateNotificationArea();
  }
}

bool AutofillDialogControllerImpl::AreLegalDocumentsCurrent() const {
  return has_accepted_legal_documents_ ||
      (wallet_items_ && wallet_items_->legal_documents().empty());
}

void AutofillDialogControllerImpl::AcceptLegalDocuments() {
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::Bind(&UserDidOptIntoLocationServices));

  GetWalletClient()->AcceptLegalDocuments(
      wallet_items_->legal_documents(),
      wallet_items_->google_transaction_id(),
      source_url_);

  if (AreLegalDocumentsCurrent())
    LoadRiskFingerprintData();
}

void AutofillDialogControllerImpl::SubmitWithWallet() {
  active_instrument_id_.clear();
  active_address_id_.clear();
  full_wallet_.reset();

  const wallet::WalletItems::MaskedInstrument* active_instrument =
      ActiveInstrument();
  if (!IsManuallyEditingSection(SECTION_CC_BILLING)) {
    active_instrument_id_ = active_instrument->object_id();
    DCHECK(!active_instrument_id_.empty());
  }

  const wallet::Address* active_address = ActiveShippingAddress();
  if (!IsManuallyEditingSection(SECTION_SHIPPING) &&
      !ShouldUseBillingForShipping() &&
      IsShippingAddressRequired()) {
    active_address_id_ = active_address->object_id();
    DCHECK(!active_address_id_.empty());
  }

  scoped_ptr<wallet::Instrument> inputted_instrument =
      CreateTransientInstrument();
  if (inputted_instrument && IsEditingExistingData(SECTION_CC_BILLING)) {
    inputted_instrument->set_object_id(active_instrument->object_id());
    DCHECK(!inputted_instrument->object_id().empty());
  }

  scoped_ptr<wallet::Address> inputted_address;
  if (active_address_id_.empty() && IsShippingAddressRequired()) {
    if (ShouldUseBillingForShipping()) {
      const wallet::Address& address = inputted_instrument ?
          *inputted_instrument->address() : active_instrument->address();
      // Try to find an exact matched shipping address and use it for shipping,
      // otherwise save it as a new shipping address. http://crbug.com/225442
      const wallet::Address* duplicated_address =
          FindDuplicateAddress(wallet_items_->addresses(), address);
      if (duplicated_address) {
        active_address_id_ = duplicated_address->object_id();
        DCHECK(!active_address_id_.empty());
      } else {
        inputted_address.reset(new wallet::Address(address));
        DCHECK(inputted_address->object_id().empty());
      }
    } else {
      inputted_address = CreateTransientAddress();
      if (IsEditingExistingData(SECTION_SHIPPING)) {
        inputted_address->set_object_id(active_address->object_id());
        DCHECK(!inputted_address->object_id().empty());
      }
    }
  }

  // If there's neither an address nor instrument to save, |GetFullWallet()|
  // is called when the risk fingerprint is loaded.
  if (!active_instrument_id_.empty() &&
      (!active_address_id_.empty() || !IsShippingAddressRequired())) {
    GetFullWallet();
    return;
  }

  GetWalletClient()->SaveToWallet(inputted_instrument.Pass(),
                                  inputted_address.Pass(),
                                  source_url_);
}

scoped_ptr<wallet::Instrument> AutofillDialogControllerImpl::
    CreateTransientInstrument() {
  if (!active_instrument_id_.empty())
    return scoped_ptr<wallet::Instrument>();

  DetailOutputMap output;
  view_->GetUserInput(SECTION_CC_BILLING, &output);

  CreditCard card;
  AutofillProfile profile;
  string16 cvc;
  GetBillingInfoFromOutputs(output, &card, &cvc, &profile);

  return scoped_ptr<wallet::Instrument>(
      new wallet::Instrument(card, cvc, profile));
}

scoped_ptr<wallet::Address>AutofillDialogControllerImpl::
    CreateTransientAddress() {
  // If not using billing for shipping, just scrape the view.
  DetailOutputMap output;
  view_->GetUserInput(SECTION_SHIPPING, &output);

  AutofillProfile profile;
  FillFormGroupFromOutputs(output, &profile);

  return scoped_ptr<wallet::Address>(new wallet::Address(profile));
}

void AutofillDialogControllerImpl::GetFullWallet() {
  DCHECK(is_submitting_);
  DCHECK(IsPayingWithWallet());
  DCHECK(wallet_items_);
  DCHECK(!active_instrument_id_.empty());
  DCHECK(!active_address_id_.empty() || !IsShippingAddressRequired());

  std::vector<wallet::WalletClient::RiskCapability> capabilities;
  capabilities.push_back(wallet::WalletClient::VERIFY_CVC);

  GetWalletClient()->GetFullWallet(wallet::WalletClient::FullWalletRequest(
      active_instrument_id_,
      active_address_id_,
      source_url_,
      wallet_items_->google_transaction_id(),
      capabilities,
      wallet_items_->HasRequiredAction(wallet::SETUP_WALLET)));
}

void AutofillDialogControllerImpl::HandleSaveOrUpdateRequiredActions(
    const std::vector<wallet::RequiredAction>& required_actions) {
  DCHECK(!required_actions.empty());

  // TODO(ahutter): Invesitigate if we need to support more generic actions on
  // this call such as GAIA_AUTH. See crbug.com/243457.
  for (std::vector<wallet::RequiredAction>::const_iterator iter =
           required_actions.begin();
       iter != required_actions.end(); ++iter) {
    if (*iter != wallet::INVALID_FORM_FIELD) {
      // TODO(dbeam): handle this more gracefully.
      DisableWallet(wallet::WalletClient::UNKNOWN_ERROR);
    }
  }
  SetIsSubmitting(false);
}

void AutofillDialogControllerImpl::FinishSubmit() {
  if (IsPayingWithWallet() &&
      !profile_->GetPrefs()->GetBoolean(
          ::prefs::kAutofillDialogHasPaidWithWallet)) {
    if (GetDialogType() == DIALOG_TYPE_REQUEST_AUTOCOMPLETE) {
      // To get past this point, the view must call back OverlayButtonPressed.
#if defined(TOOLKIT_VIEWS)
      ScopedViewUpdates updates(view_.get());
      view_->UpdateButtonStrip();
#else
      // TODO(estade): implement overlays on other platforms.
      OverlayButtonPressed();
#endif
      return;
    } else {
      profile_->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogHasPaidWithWallet, true);
    }
  }

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
    FillOutputForSectionWithComparator(
        SECTION_CC_BILLING,
        base::Bind(DetailInputMatchesShippingField));
  } else {
    FillOutputForSection(SECTION_SHIPPING);
  }

  if (!IsPayingWithWallet()) {
    for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
      DialogSection section = static_cast<DialogSection>(i);
      if (!SectionIsActive(section))
        continue;

      SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
      std::string item_key = model->GetItemKeyForCheckedItem();
      if (IsASuggestionItemKey(item_key) || item_key == kSameAsBillingKey) {
        int variant = GetSelectedVariantForModel(*model);
        PersistAutofillChoice(section, item_key, variant);
      }
    }

    profile_->GetPrefs()->SetBoolean(::prefs::kAutofillDialogSaveData,
                                     view_->SaveDetailsLocally());
  }

  // On a successful submit, if the user manually selected "pay without wallet",
  // stop trying to pay with Wallet on future runs of the dialog. On the other
  // hand, if there was an error that prevented the user from having the choice
  // of using Wallet, leave the pref alone.
  if (!wallet_error_notification_ &&
      account_chooser_model_.HasAccountsToChoose()) {
    profile_->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet,
        !account_chooser_model_.WalletIsSelected());
  }

  LogOnFinishSubmitMetrics();

  // Callback should be called as late as possible.
  callback_.Run(&form_structure_, !wallet_items_ ? std::string() :
      wallet_items_->google_transaction_id());
  data_was_passed_back_ = true;

  // This might delete us.
  if (GetDialogType() == DIALOG_TYPE_REQUEST_AUTOCOMPLETE)
    Hide();
}

void AutofillDialogControllerImpl::PersistAutofillChoice(
    DialogSection section,
    const std::string& guid,
    int variant) {
  DCHECK(!IsPayingWithWallet());
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(kGuidPrefKey, guid);
  value->SetInteger(kVariantPrefKey, variant);

  DictionaryPrefUpdate updater(profile()->GetPrefs(),
                               ::prefs::kAutofillDialogAutofillDefault);
  base::DictionaryValue* autofill_choice = updater.Get();
  autofill_choice->Set(SectionToPrefString(section), value.release());
}

void AutofillDialogControllerImpl::GetDefaultAutofillChoice(
    DialogSection section,
    std::string* guid,
    int* variant) {
  DCHECK(!IsPayingWithWallet());
  // The default choice is the first thing in the menu that is a suggestion
  // item.
  *variant = 0;
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  for (int i = 0; i < model->GetItemCount(); ++i) {
    if (IsASuggestionItemKey(model->GetItemKeyAt(i))) {
      *guid = model->GetItemKeyAt(i);
      break;
    }
  }
}

bool AutofillDialogControllerImpl::GetAutofillChoice(DialogSection section,
                                                     std::string* guid,
                                                     int* variant) {
  DCHECK(!IsPayingWithWallet());
  const base::DictionaryValue* choices = profile()->GetPrefs()->GetDictionary(
      ::prefs::kAutofillDialogAutofillDefault);
  if (!choices)
    return false;

  const base::DictionaryValue* choice = NULL;
  if (!choices->GetDictionary(SectionToPrefString(section), &choice))
    return false;

  choice->GetString(kGuidPrefKey, guid);
  choice->GetInteger(kVariantPrefKey, variant);
  return true;
}

size_t AutofillDialogControllerImpl::GetSelectedVariantForModel(
    const SuggestionsMenuModel& model) {
  size_t variant = 0;
  // Calculate the variant by looking at how many items come from the same
  // data model.
  for (int i = model.checked_item() - 1; i >= 0; --i) {
    if (model.GetItemKeyAt(i) == model.GetItemKeyForCheckedItem())
      variant++;
    else
      break;
  }
  return variant;
}

void AutofillDialogControllerImpl::LogOnFinishSubmitMetrics() {
  GetMetricLogger().LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      GetDialogType(),
      AutofillMetrics::DIALOG_ACCEPTED);

  GetMetricLogger().LogDialogUiEvent(
      GetDialogType(), AutofillMetrics::DIALOG_UI_ACCEPTED);

  AutofillMetrics::DialogDismissalState dismissal_state;
  if (!IsManuallyEditingAnySection())
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_EXISTING_DATA;
  else if (IsPayingWithWallet())
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_WALLET;
  else if (ShouldSaveDetailsLocally())
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_AUTOFILL;
  else
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_NO_SAVE;

  GetMetricLogger().LogDialogDismissalState(GetDialogType(), dismissal_state);
}

void AutofillDialogControllerImpl::LogOnCancelMetrics() {
  GetMetricLogger().LogDialogUiEvent(
      GetDialogType(), AutofillMetrics::DIALOG_UI_CANCELED);

  AutofillMetrics::DialogDismissalState dismissal_state;
  if (!signin_registrar_.IsEmpty())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_DURING_SIGNIN;
  else if (!IsManuallyEditingAnySection())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_NO_EDITS;
  else if (AllSectionsAreValid())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS;
  else
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_WITH_INVALID_FIELDS;

  GetMetricLogger().LogDialogDismissalState(GetDialogType(), dismissal_state);

  GetMetricLogger().LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      GetDialogType(),
      AutofillMetrics::DIALOG_CANCELED);
}

void AutofillDialogControllerImpl::LogSuggestionItemSelectedMetric(
    const SuggestionsMenuModel& model) {
  DialogSection section = SectionForSuggestionsMenuModel(model);

  AutofillMetrics::DialogUiEvent dialog_ui_event;
  if (model.GetItemKeyForCheckedItem() == kAddNewItemKey) {
    // Selected to add a new item.
    dialog_ui_event = common::DialogSectionToUiItemAddedEvent(section);
  } else if (IsASuggestionItemKey(model.GetItemKeyForCheckedItem())) {
    // Selected an existing item.
    dialog_ui_event = common::DialogSectionToUiSelectionChangedEvent(section);
  } else {
    // TODO(estade): add logging for "Manage items" or "Use billing for
    // shipping"?
    return;
  }

  GetMetricLogger().LogDialogUiEvent(GetDialogType(), dialog_ui_event);
}

void AutofillDialogControllerImpl::LogDialogLatencyToShow() {
  if (was_ui_latency_logged_)
    return;

  GetMetricLogger().LogDialogLatencyToShow(
      GetDialogType(),
      base::Time::Now() - dialog_shown_timestamp_);
  was_ui_latency_logged_ = true;
}

AutofillMetrics::DialogInitialUserStateMetric
    AutofillDialogControllerImpl::GetInitialUserState() const {
  // Consider a user to be an Autofill user if the user has any credit cards
  // or addresses saved. Check that the item count is greater than 2 because
  // an "empty" menu still has the "add new" menu item and "manage" menu item.
  const bool has_autofill_profiles =
      suggested_cc_.GetItemCount() > 2 ||
      suggested_billing_.GetItemCount() > 2;

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

void AutofillDialogControllerImpl::MaybeShowCreditCardBubble() {
  if (!data_was_passed_back_)
    return;

  if (newly_saved_card_) {
    scoped_ptr<AutofillProfile> billing_profile;
    if (IsManuallyEditingSection(SECTION_BILLING)) {
      // Scrape the view as the user's entering or updating information.
      DetailOutputMap outputs;
      view_->GetUserInput(SECTION_BILLING, &outputs);
      billing_profile.reset(new AutofillProfile);
      FillFormGroupFromOutputs(outputs, billing_profile.get());
    } else {
      // Just snag the currently suggested profile.
      std::string item_key = SuggestionsMenuModelForSection(SECTION_BILLING)->
          GetItemKeyForCheckedItem();
      AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
      billing_profile.reset(new AutofillProfile(*profile));
    }

    // The bubble also needs the associated email address.
    billing_profile->SetRawInfo(
        EMAIL_ADDRESS,
        GetValueFromSection(SECTION_EMAIL, EMAIL_ADDRESS));

    ShowNewCreditCardBubble(newly_saved_card_.Pass(),
                            billing_profile.Pass());
    return;
  }

  if (!full_wallet_ || !full_wallet_->billing_address())
    return;

  base::string16 backing_last_four;
  if (ActiveInstrument()) {
    backing_last_four = ActiveInstrument()->TypeAndLastFourDigits();
  } else {
    DetailOutputMap output;
    view_->GetUserInput(SECTION_CC_BILLING, &output);
    CreditCard card;
    GetBillingInfoFromOutputs(output, &card, NULL, NULL);
    backing_last_four = card.TypeAndLastFourDigits();
  }
#if !defined(OS_ANDROID)
  GeneratedCreditCardBubbleController::Show(
      web_contents(),
      full_wallet_->TypeAndLastFourDigits(),
      backing_last_four);
#endif
}

}  // namespace autofill
