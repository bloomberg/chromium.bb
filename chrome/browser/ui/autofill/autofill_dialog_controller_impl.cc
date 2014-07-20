// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <algorithm>
#include <map>
#include <string>

#include "apps/app_window.h"
#include "apps/app_window_registry.h"
#include "apps/ui/native_app_window.h"
#include "base/base64.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/rand_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_common.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/render_messages.h"
#include "chrome/common/url_constants.h"
#include "components/autofill/content/browser/risk/fingerprint.h"
#include "components/autofill/content/browser/risk/proto/fingerprint.pb.h"
#include "components/autofill/content/browser/wallet/form_field_error.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/gaia_account.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/content/browser/wallet/wallet_service_url.h"
#include "components/autofill/content/browser/wallet/wallet_signin_helper.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
#include "components/metrics/metrics_service.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/geolocation_provider.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/component_scaled_resources.h"
#include "grit/components_strings.h"
#include "grit/generated_resources.h"
#include "grit/platform_locale_settings.h"
#include "grit/theme_resources.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/libaddressinput/chromium/chrome_downloader_impl.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_problem.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/base_window.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressProblem;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::Downloader;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::Localization;
using ::i18n::addressinput::MISSING_REQUIRED_FIELD;

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

// This string is stored along with saved addresses and credit cards in the
// WebDB, and hence should not be modified, so that it remains consistent over
// time.
const char kAutofillDialogOrigin[] = "Chrome Autofill dialog";

// HSL shift to gray out an image.
const color_utils::HSL kGrayImageShift = {-1, 0, 0.8};

// Limit Wallet items refresh rate to at most once per minute.
const int64 kWalletItemsRefreshRateSeconds = 60;

// The number of milliseconds to delay enabling the submit button after showing
// the dialog. This delay prevents users from accidentally clicking the submit
// button on startup.
const int kSubmitButtonDelayMs = 1000;

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

base::string16 NullGetInfo(const AutofillType& type) {
  return base::string16();
}

// Extract |type| from |inputs| using |section| to determine whether the info
// should be billing or shipping specific (for sections with address info).
base::string16 GetInfoFromInputs(const FieldValueMap& inputs,
                                 DialogSection section,
                                 const AutofillType& type) {
  ServerFieldType field_type = type.GetStorableType();
  if (section != SECTION_SHIPPING)
    field_type = AutofillType::GetEquivalentBillingFieldType(field_type);

  base::string16 info;
  FieldValueMap::const_iterator it = inputs.find(field_type);
  if (it != inputs.end())
    info = it->second;

  if (!info.empty() && type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    info = base::ASCIIToUTF16(AutofillCountry::GetCountryCode(
        info, g_browser_process->GetApplicationLocale()));
  }

  return info;
}

// Returns true if |input| should be used to fill a site-requested |field| which
// is notated with a "shipping" tag, for use when the user has decided to use
// the billing address as the shipping address.
bool ServerTypeMatchesShippingField(ServerFieldType type,
                                    const AutofillField& field) {
  // Equivalent billing field type is used to support UseBillingAsShipping
  // usecase.
  return common::ServerTypeEncompassesFieldType(
      type,
      AutofillType(AutofillType::GetEquivalentBillingFieldType(
          field.Type().GetStorableType())));
}

// Initializes |form_group| from user-entered data.
void FillFormGroupFromOutputs(const FieldValueMap& detail_outputs,
                              FormGroup* form_group) {
  for (FieldValueMap::const_iterator iter = detail_outputs.begin();
       iter != detail_outputs.end(); ++iter) {
    ServerFieldType type = iter->first;
    if (!iter->second.empty()) {
      form_group->SetInfo(AutofillType(type),
                          iter->second,
                          g_browser_process->GetApplicationLocale());
    }
  }
}

// Get billing info from |output| and put it into |card|, |cvc|, and |profile|.
// These outparams are required because |card|/|profile| accept different types
// of raw info, and CreditCard doesn't save CVCs.
void GetBillingInfoFromOutputs(const FieldValueMap& output,
                               CreditCard* card,
                               base::string16* cvc,
                               AutofillProfile* profile) {
  for (FieldValueMap::const_iterator it = output.begin();
       it != output.end(); ++it) {
    const ServerFieldType type = it->first;
    base::string16 trimmed;
    base::TrimWhitespace(it->second, base::TRIM_ALL, &trimmed);

    // Special case CVC as CreditCard just swallows it.
    if (type == CREDIT_CARD_VERIFICATION_CODE) {
      if (cvc)
        cvc->assign(trimmed);
    } else if (common::IsCreditCardType(type)) {
      card->SetRawInfo(type, trimmed);
    } else {
      // Copy the credit card name to |profile| in addition to |card| as
      // wallet::Instrument requires a recipient name for its billing address.
      if (card && type == NAME_FULL)
        card->SetRawInfo(CREDIT_CARD_NAME, trimmed);

      if (profile) {
        profile->SetInfo(AutofillType(AutofillType(type).GetStorableType()),
                         trimmed,
                         g_browser_process->GetApplicationLocale());
      }
    }
  }
}

// Returns the containing window for the given |web_contents|. The containing
// window might be a browser window for a Chrome tab, or it might be an app
// window for a platform app.
ui::BaseWindow* GetBaseWindowForWebContents(
    content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    return browser->window();

  gfx::NativeWindow native_window = web_contents->GetTopLevelNativeWindow();
  apps::AppWindow* app_window =
      apps::AppWindowRegistry::GetAppWindowForNativeWindowAnyProfile(
          native_window);
  return app_window->GetBaseWindow();
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

bool IsCardHolderNameValidForWallet(const base::string16& name) {
  base::string16 whitespace_collapsed_name =
      base::CollapseWhitespace(name, true);
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

  int error_ids = 0;
  int error_code = 0;

  switch (error_type) {
    case wallet::WalletClient::UNSUPPORTED_MERCHANT:
      error_ids = IDS_AUTOFILL_WALLET_UNSUPPORTED_MERCHANT;
      break;

    case wallet::WalletClient::BAD_REQUEST:
      error_ids = IDS_AUTOFILL_WALLET_UPGRADE_CHROME_ERROR;
      error_code = 71;
      break;

    case wallet::WalletClient::INVALID_PARAMS:
      error_ids = IDS_AUTOFILL_WALLET_BAD_TRANSACTION_AMOUNT;
      error_code = 76;
      break;

    case wallet::WalletClient::BUYER_ACCOUNT_ERROR:
      error_ids = IDS_AUTOFILL_WALLET_BUYER_ACCOUNT_ERROR;
      error_code = 12;
      break;

    case wallet::WalletClient::UNSUPPORTED_API_VERSION:
      error_ids = IDS_AUTOFILL_WALLET_UPGRADE_CHROME_ERROR;
      error_code = 43;
      break;

    case wallet::WalletClient::SERVICE_UNAVAILABLE:
      error_ids = IDS_AUTOFILL_WALLET_SERVICE_UNAVAILABLE_ERROR;
      error_code = 61;
      break;

    case wallet::WalletClient::INTERNAL_ERROR:
      error_ids = IDS_AUTOFILL_WALLET_UPGRADE_CHROME_ERROR;
      error_code = 62;
      break;

    case wallet::WalletClient::MALFORMED_RESPONSE:
      error_ids = IDS_AUTOFILL_WALLET_UNKNOWN_ERROR;
      error_code = 72;
      break;

    case wallet::WalletClient::NETWORK_ERROR:
      error_ids = IDS_AUTOFILL_WALLET_UNKNOWN_ERROR;
      error_code = 73;
      break;

    case wallet::WalletClient::UNKNOWN_ERROR:
      error_ids = IDS_AUTOFILL_WALLET_UNKNOWN_ERROR;
      error_code = 74;
      break;

    case wallet::WalletClient::UNSUPPORTED_USER_AGENT_OR_API_KEY:
      error_ids = IDS_AUTOFILL_WALLET_UNSUPPORTED_AGENT_OR_API_KEY;
      error_code = 75;
      break;

    case wallet::WalletClient::SPENDING_LIMIT_EXCEEDED:
      error_ids = IDS_AUTOFILL_WALLET_BAD_TRANSACTION_AMOUNT;
      break;

    // Handled in the prior switch().
    case wallet::WalletClient::UNVERIFIED_KNOW_YOUR_CUSTOMER_STATUS:
    case wallet::WalletClient::BUYER_LEGAL_ADDRESS_NOT_SUPPORTED:
      NOTREACHED();
      break;
  }

  DCHECK_NE(0, error_ids);

  // The other error types are strings of the form "XXX. You can pay without
  // wallet."
  scoped_ptr<DialogNotification> notification(new DialogNotification(
      DialogNotification::WALLET_ERROR,
      l10n_util::GetStringFUTF16(IDS_AUTOFILL_DIALOG_COMPLETE_WITHOUT_WALLET,
                                 l10n_util::GetStringUTF16(error_ids))));

  if (error_code) {
    notification->set_tooltip_text(
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_WALLET_ERROR_CODE_TOOLTIP,
                                   base::IntToString16(error_code)));
  }

  return notification.Pass();
}

// Returns the ID of the address or instrument that should be selected in the
// UI, given that the |default_id| is currently the default ID on the Wallet
// server, |previous_default_id| was the default ID prior to re-fetching the
// Wallet data, and |previously_selected_id| was the ID of the item selected in
// the dialog prior to re-fetching the Wallet data.
std::string GetIdToSelect(const std::string& default_id,
                          const std::string& previous_default_id,
                          const std::string& previously_selected_id) {
  // If the default ID changed since the last fetch of the Wallet data, select
  // it rather than the previously selected item, as the user's intention in
  // changing the default was probably to use it.
  if (default_id != previous_default_id)
    return default_id;

  // Otherwise, prefer the previously selected item, if there was one.
  return !previously_selected_id.empty() ? previously_selected_id : default_id;
}

// Generate a random card number in a user displayable format.
base::string16 GenerateRandomCardNumber() {
  std::string card_number;
  for (size_t i = 0; i < 4; ++i) {
    int part = base::RandInt(0, 10000);
    base::StringAppendF(&card_number, "%04d ", part);
  }
  return base::ASCIIToUTF16(card_number);
}

gfx::Image CreditCardIconForType(const std::string& credit_card_type) {
  const int input_card_idr = CreditCard::IconResourceId(credit_card_type);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  gfx::Image result = rb.GetImageNamed(input_card_idr);
  if (input_card_idr == IDR_AUTOFILL_CC_GENERIC) {
    // When the credit card type is unknown, no image should be shown. However,
    // to simplify the view code on Mac, save space for the credit card image by
    // returning a transparent image of the appropriate size. Not all credit
    // card images are the same size, but none is larger than the Visa icon.
    result = gfx::Image(gfx::ImageSkiaOperations::CreateTransparentImage(
        rb.GetImageNamed(IDR_AUTOFILL_CC_VISA).AsImageSkia(), 0));
  }
  return result;
}

gfx::Image CvcIconForCreditCardType(const base::string16& credit_card_type) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (credit_card_type == l10n_util::GetStringUTF16(IDS_AUTOFILL_CC_AMEX))
    return rb.GetImageNamed(IDR_CREDIT_CARD_CVC_HINT_AMEX);

  return rb.GetImageNamed(IDR_CREDIT_CARD_CVC_HINT);
}

ServerFieldType CountryTypeForSection(DialogSection section) {
  return section == SECTION_SHIPPING ? ADDRESS_HOME_COUNTRY :
                                       ADDRESS_BILLING_COUNTRY;
}

// Attempts to canonicalize the administrative area name in |profile| using the
// rules in |validator|.
void CanonicalizeState(const AddressValidator* validator,
                       AutofillProfile* profile) {
  base::string16 administrative_area;
  scoped_ptr<AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(
          *profile, g_browser_process->GetApplicationLocale());

  validator->CanonicalizeAdministrativeArea(address_data.get());
  administrative_area = base::UTF8ToUTF16(address_data->administrative_area);

  profile->SetInfo(AutofillType(ADDRESS_HOME_STATE),
                   administrative_area,
                   g_browser_process->GetApplicationLocale());
}

ValidityMessage GetPhoneValidityMessage(const base::string16& country_name,
                                        const base::string16& number) {
  std::string region = AutofillCountry::GetCountryCode(
      country_name,
      g_browser_process->GetApplicationLocale());
  i18n::PhoneObject phone_object(number, region);
  ValidityMessage phone_message(base::string16(), true);

  // Check if the phone number is invalid. Allow valid international
  // numbers that don't match the address's country only if they have an
  // international calling code.
  if (!phone_object.IsValidNumber() ||
      (phone_object.country_code().empty() &&
       phone_object.region() != region)) {
    phone_message.text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_PHONE_NUMBER);
  }

  return phone_message;
}

// Constructs |inputs| from template data for a given |dialog_section|.
// |country_country| specifies the country code that the inputs should be built
// for. Sets the |language_code| to be used for address formatting, if
// internationalized address input is enabled. The |language_code| parameter can
// be NULL.
void BuildInputsForSection(DialogSection dialog_section,
                           const std::string& country_code,
                           DetailInputs* inputs,
                           std::string* language_code) {
  using l10n_util::GetStringUTF16;

  const DetailInput kCCInputs[] = {
    { DetailInput::LONG,
      CREDIT_CARD_NUMBER,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_NUMBER) },
    { DetailInput::SHORT,
      CREDIT_CARD_EXP_MONTH,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH) },
    { DetailInput::SHORT,
      CREDIT_CARD_EXP_4_DIGIT_YEAR,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR) },
    { DetailInput::SHORT_EOL,
      CREDIT_CARD_VERIFICATION_CODE,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC),
      1.5 },
  };

  const DetailInput kBillingPhoneInputs[] = {
    { DetailInput::LONG,
      PHONE_BILLING_WHOLE_NUMBER,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_PHONE_NUMBER) },
  };

  const DetailInput kEmailInputs[] = {
    { DetailInput::LONG,
      EMAIL_ADDRESS,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EMAIL) },
  };

  const DetailInput kShippingPhoneInputs[] = {
    { DetailInput::LONG,
      PHONE_HOME_WHOLE_NUMBER,
      GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_PHONE_NUMBER) },
  };

  switch (dialog_section) {
    case SECTION_CC: {
      common::BuildInputs(kCCInputs, arraysize(kCCInputs), inputs);
      break;
    }

    case SECTION_BILLING: {
      i18ninput::BuildAddressInputs(common::ADDRESS_TYPE_BILLING,
                                    country_code, inputs, language_code);
      common::BuildInputs(kBillingPhoneInputs, arraysize(kBillingPhoneInputs),
                          inputs);
      common::BuildInputs(kEmailInputs, arraysize(kEmailInputs), inputs);
      break;
    }

    case SECTION_CC_BILLING: {
      common::BuildInputs(kCCInputs, arraysize(kCCInputs), inputs);

      // Wallet only supports US billing addresses.
      const std::string hardcoded_country_code = "US";
      i18ninput::BuildAddressInputs(common::ADDRESS_TYPE_BILLING,
                                    hardcoded_country_code,
                                    inputs,
                                    language_code);
      DCHECK_EQ(inputs->back().type, ADDRESS_BILLING_COUNTRY);
      inputs->back().length = DetailInput::NONE;
      const std::string& app_locale =
          g_browser_process->GetApplicationLocale();
      inputs->back().initial_value =
          AutofillCountry(hardcoded_country_code, app_locale).name();

      common::BuildInputs(kBillingPhoneInputs, arraysize(kBillingPhoneInputs),
                          inputs);
      break;
    }

    case SECTION_SHIPPING: {
      i18ninput::BuildAddressInputs(common::ADDRESS_TYPE_SHIPPING,
                                    country_code, inputs, language_code);
      common::BuildInputs(kShippingPhoneInputs, arraysize(kShippingPhoneInputs),
                          inputs);
      break;
    }
  }
}

}  // namespace

AutofillDialogViewDelegate::~AutofillDialogViewDelegate() {}

AutofillDialogControllerImpl::~AutofillDialogControllerImpl() {
  if (popup_controller_)
    popup_controller_->Hide();

  GetMetricLogger().LogDialogInitialUserState(initial_user_state_);
}

// Checks the country code against the values the form structure enumerates.
bool AutofillCountryFilter(
    const std::set<base::string16>& form_structure_values,
    const std::string& country_code) {
  if (!form_structure_values.empty() &&
      !form_structure_values.count(base::ASCIIToUTF16(country_code))) {
    return false;
  }

  return true;
}

// Checks the country code against the values the form structure enumerates and
// against the ones Wallet allows.
bool WalletCountryFilter(
    const std::set<base::string16>& form_structure_values,
    const std::set<std::string>& wallet_allowed_values,
    const std::string& country_code) {
  if ((!form_structure_values.empty() &&
       !form_structure_values.count(base::ASCIIToUTF16(country_code))) ||
      (!wallet_allowed_values.empty() &&
       !wallet_allowed_values.count(country_code))) {
    return false;
  }

  return true;
}

// static
base::WeakPtr<AutofillDialogControllerImpl>
AutofillDialogControllerImpl::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const AutofillClient::ResultCallback& callback) {
  // AutofillDialogControllerImpl owns itself.
  AutofillDialogControllerImpl* autofill_dialog_controller =
      new AutofillDialogControllerImpl(contents,
                                       form_structure,
                                       source_url,
                                       callback);
  return autofill_dialog_controller->weak_ptr_factory_.GetWeakPtr();
}

// static
void AutofillDialogController::RegisterPrefs(PrefRegistrySimple* registry) {
  registry->RegisterListPref(::prefs::kAutofillDialogWalletLocationAcceptance);
}

// static
void AutofillDialogController::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
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
  registry->RegisterBooleanPref(
      ::prefs::kAutofillDialogWalletShippingSameAsBilling,
      false,
      user_prefs::PrefRegistrySyncable::SYNCABLE_PREF);
}

// static
base::WeakPtr<AutofillDialogController> AutofillDialogController::Create(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const AutofillClient::ResultCallback& callback) {
  return AutofillDialogControllerImpl::Create(contents,
                                              form_structure,
                                              source_url,
                                              callback);
}

void AutofillDialogControllerImpl::Show() {
  dialog_shown_timestamp_ = base::Time::Now();

  // Determine what field types should be included in the dialog.
  bool has_types = false;
  bool has_sections = false;
  form_structure_.ParseFieldTypesFromAutocompleteAttributes(
      &has_types, &has_sections);

  // Fail if the author didn't specify autocomplete types.
  if (!has_types) {
    callback_.Run(
        AutofillClient::AutocompleteResultErrorDisabled,
        base::ASCIIToUTF16("Form is missing autocomplete attributes."),
        NULL);
    delete this;
    return;
  }

  // Fail if the author didn't ask for at least some kind of credit card
  // information.
  bool has_credit_card_field = false;
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillType type = form_structure_.field(i)->Type();
    if (type.html_type() != HTML_TYPE_UNKNOWN && type.group() == CREDIT_CARD) {
      has_credit_card_field = true;
      break;
    }
  }

  if (!has_credit_card_field) {
    callback_.Run(AutofillClient::AutocompleteResultErrorDisabled,
                  base::ASCIIToUTF16(
                      "Form is not a payment form (must contain "
                      "some autocomplete=\"cc-*\" fields). "),
                  NULL);
    delete this;
    return;
  }

  billing_country_combobox_model_.reset(new CountryComboboxModel());
  billing_country_combobox_model_->SetCountries(
      *GetManager(),
      base::Bind(AutofillCountryFilter,
                 form_structure_.PossibleValues(ADDRESS_BILLING_COUNTRY)));
  shipping_country_combobox_model_.reset(new CountryComboboxModel());
  acceptable_shipping_countries_ =
      form_structure_.PossibleValues(ADDRESS_HOME_COUNTRY);
  shipping_country_combobox_model_->SetCountries(
      *GetManager(),
      base::Bind(AutofillCountryFilter,
                 base::ConstRef(acceptable_shipping_countries_)));

  // If the form has a country <select> but none of the options are valid, bail.
  if (billing_country_combobox_model_->GetItemCount() == 0 ||
      shipping_country_combobox_model_->GetItemCount() == 0) {
    callback_.Run(AutofillClient::AutocompleteResultErrorDisabled,
                  base::ASCIIToUTF16("No valid/supported country options"
                                     " found."),
                  NULL);
    delete this;
    return;
  }

  // Log any relevant UI metrics and security exceptions.
  GetMetricLogger().LogDialogUiEvent(AutofillMetrics::DIALOG_UI_SHOWN);

  GetMetricLogger().LogDialogSecurityMetric(
      AutofillMetrics::SECURITY_METRIC_DIALOG_SHOWN);

  // The Autofill dialog is shown in response to a message from the renderer and
  // as such, it can only be made in the context of the current document. A call
  // to GetActiveEntry would return a pending entry, if there was one, which
  // would be a security bug. Therefore, we use the last committed URL for the
  // access checks.
  const GURL& current_url = web_contents()->GetLastCommittedURL();
  invoked_from_same_origin_ =
      current_url.GetOrigin() == source_url_.GetOrigin();

  if (!invoked_from_same_origin_) {
    GetMetricLogger().LogDialogSecurityMetric(
        AutofillMetrics::SECURITY_METRIC_CROSS_ORIGIN_FRAME);
  }

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);

    std::string country_code;
    CountryComboboxModel* model = CountryComboboxModelForSection(section);
    if (model)
      country_code = model->GetDefaultCountryCode();

    DetailInputs* inputs = MutableRequestedFieldsForSection(section);
    BuildInputsForSection(
        section, country_code, inputs,
        MutableAddressLanguageCodeForSection(section));
  }

  // Test whether we need to show the shipping section. If filling that section
  // would be a no-op, don't show it.
  cares_about_shipping_ = form_structure_.FillFields(
      RequestedTypesForSection(SECTION_SHIPPING),
      base::Bind(common::ServerTypeMatchesField, SECTION_SHIPPING),
      base::Bind(NullGetInfo),
      std::string(),
      g_browser_process->GetApplicationLocale());

  transaction_amount_ = form_structure_.GetUniqueValue(
      HTML_TYPE_TRANSACTION_AMOUNT);
  transaction_currency_ = form_structure_.GetUniqueValue(
      HTML_TYPE_TRANSACTION_CURRENCY);

  account_chooser_model_.reset(
      new AccountChooserModel(this,
                              profile_,
                              !ShouldShowAccountChooser(),
                              metric_logger_));

  acceptable_cc_types_ = form_structure_.PossibleValues(CREDIT_CARD_TYPE);
  // Wallet generates MC virtual cards, so we have to disable it if MC is not
  // allowed.
  if (ShouldDisallowCcType(CreditCard::TypeForDisplay(kMasterCard)))
    DisableWallet(wallet::WalletClient::UNSUPPORTED_MERCHANT);

  if (account_chooser_model_->WalletIsSelected())
    FetchWalletCookie();

  validator_.reset(new AddressValidator(
      I18N_ADDRESS_VALIDATION_DATA_URL,
      scoped_ptr<Downloader>(
          new autofill::ChromeDownloaderImpl(profile_->GetRequestContext())),
      ValidationRulesStorageFactory::CreateStorage(),
      this));

  SuggestionsUpdated();
  SubmitButtonDelayBegin();
  view_.reset(CreateView());
  view_->Show();
  GetManager()->AddObserver(this);

  if (!account_chooser_model_->WalletIsSelected())
    LogDialogLatencyToShow();
}

void AutofillDialogControllerImpl::Hide() {
  if (view_)
    view_->Hide();
}

void AutofillDialogControllerImpl::TabActivated() {
  // If the user switched away from this tab and then switched back, reload the
  // Wallet items, in case they've changed.
  int64 seconds_elapsed_since_last_refresh =
      (base::TimeTicks::Now() - last_wallet_items_fetch_timestamp_).InSeconds();
  if (IsPayingWithWallet() && wallet_items_ &&
      seconds_elapsed_since_last_refresh >= kWalletItemsRefreshRateSeconds) {
    GetWalletItems();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AutofillDialogViewDelegate implementation.

base::string16 AutofillDialogControllerImpl::DialogTitle() const {
  if (ShouldShowSpinner())
    return base::string16();

  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

base::string16 AutofillDialogControllerImpl::AccountChooserText() const {
  if (!account_chooser_model_->WalletIsSelected())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PAYING_WITHOUT_WALLET);

  if (SignedInState() == SIGNED_IN)
    return account_chooser_model_->GetActiveWalletAccountName();

  // In this case, the account chooser should be showing the signin link.
  return base::string16();
}

base::string16 AutofillDialogControllerImpl::SignInLinkText() const {
  int ids = SignedInState() == NOT_CHECKED ?
      IDS_AUTOFILL_DIALOG_USE_WALLET_LINK :
      ShouldShowSignInWebView() ? IDS_AUTOFILL_DIALOG_CANCEL_SIGN_IN :
                                  IDS_AUTOFILL_DIALOG_SIGN_IN;

  return l10n_util::GetStringUTF16(ids);
}

base::string16 AutofillDialogControllerImpl::SpinnerText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_LOADING);
}

base::string16 AutofillDialogControllerImpl::EditSuggestionText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EDIT);
}

base::string16 AutofillDialogControllerImpl::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

base::string16 AutofillDialogControllerImpl::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IsSubmitPausedOn(wallet::VERIFY_CVV) ?
      IDS_AUTOFILL_DIALOG_VERIFY_BUTTON : IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

base::string16 AutofillDialogControllerImpl::SaveLocallyText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_CHECKBOX);
}

base::string16 AutofillDialogControllerImpl::SaveLocallyTooltip() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_TOOLTIP);
}

base::string16 AutofillDialogControllerImpl::LegalDocumentsText() {
  if (!IsPayingWithWallet() || ShouldShowSignInWebView())
    return base::string16();

  return legal_documents_text_;
}

bool AutofillDialogControllerImpl::ShouldShowSpinner() const {
  return SignedInState() == REQUIRES_RESPONSE ||
         SignedInState() == REQUIRES_PASSIVE_SIGN_IN;
}

bool AutofillDialogControllerImpl::ShouldShowAccountChooser() const {
  return !ShouldShowSpinner() && GetManager()->IsCountryOfInterest("US");
}

bool AutofillDialogControllerImpl::ShouldShowSignInWebView() const {
  return !signin_registrar_.IsEmpty();
}

GURL AutofillDialogControllerImpl::SignInUrl() const {
  return wallet::GetSignInUrl();
}

bool AutofillDialogControllerImpl::ShouldOfferToSaveInChrome() const {
  return IsAutofillEnabled() &&
      !IsPayingWithWallet() &&
      !profile_->IsOffTheRecord() &&
      IsManuallyEditingAnySection() &&
      !ShouldShowSpinner();
}

bool AutofillDialogControllerImpl::ShouldSaveInChrome() const {
  return profile_->GetPrefs()->GetBoolean(::prefs::kAutofillDialogSaveData);
}

int AutofillDialogControllerImpl::GetDialogButtons() const {
  if (waiting_for_explicit_sign_in_response_)
    return ui::DIALOG_BUTTON_NONE;

  if (ShouldShowSpinner() && !handling_use_wallet_link_click_)
    return ui::DIALOG_BUTTON_CANCEL;

  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool AutofillDialogControllerImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    if (IsSubmitPausedOn(wallet::VERIFY_CVV))
      return true;

    if (ShouldShowSpinner() || is_submitting_)
      return false;

    if (submit_button_delay_timer_.IsRunning())
      return false;

    return true;
  }

  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  return !is_submitting_ || IsSubmitPausedOn(wallet::VERIFY_CVV);
}

DialogOverlayState AutofillDialogControllerImpl::GetDialogOverlay() {
  bool show_wallet_interstitial = IsPayingWithWallet() && is_submitting_ &&
      !(full_wallet_ && !full_wallet_->required_actions().empty());
  if (!show_wallet_interstitial) {
    card_scrambling_delay_.Stop();
    card_scrambling_refresher_.Stop();
    return DialogOverlayState();
  }

  ui::ResourceBundle* rb = &ui::ResourceBundle::GetSharedInstance();
  DialogOverlayState state;
  state.string.font_list = rb->GetFontList(ui::ResourceBundle::MediumFont);

  const SkColor start_top_color = SkColorSetRGB(0xD6, 0xD6, 0xD6);
  const SkColor start_bottom_color = SkColorSetRGB(0x98, 0x98, 0x98);
  const SkColor final_top_color = SkColorSetRGB(0x52, 0x9F, 0xF8);
  const SkColor final_bottom_color = SkColorSetRGB(0x22, 0x75, 0xE5);

  if (full_wallet_ && full_wallet_->required_actions().empty()) {
    card_scrambling_delay_.Stop();
    card_scrambling_refresher_.Stop();

    base::string16 cc_number = base::ASCIIToUTF16(full_wallet_->GetPan());
    DCHECK_GE(cc_number.size(), 4U);
    state.image = GetGeneratedCardImage(
        base::ASCIIToUTF16("XXXX XXXX XXXX ") +
            cc_number.substr(cc_number.size() - 4),
        full_wallet_->billing_address()->recipient_name(),
        color_utils::AlphaBlend(
            final_top_color,
            start_top_color,
            255 * card_generated_animation_.GetCurrentValue()),
        color_utils::AlphaBlend(
            final_bottom_color,
            start_bottom_color,
            255 * card_generated_animation_.GetCurrentValue()));

    state.string.text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_CARD_GENERATION_DONE);
  } else {
    // Start the refresher if it isn't running. Wait one second before pumping
    // updates to the view.
    if (!card_scrambling_delay_.IsRunning() &&
        !card_scrambling_refresher_.IsRunning()) {
      scrambled_card_number_ = GenerateRandomCardNumber();
      card_scrambling_delay_.Start(
          FROM_HERE,
          base::TimeDelta::FromSeconds(1),
          this,
          &AutofillDialogControllerImpl::StartCardScramblingRefresher);
    }

    DCHECK(!scrambled_card_number_.empty());
    state.image = GetGeneratedCardImage(
        scrambled_card_number_,
        submitted_cardholder_name_,
        start_top_color,
        start_bottom_color);

    // "Submitting" waiting page.
    state.string.text = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_DIALOG_CARD_GENERATION_IN_PROGRESS);
  }

  return state;
}

const std::vector<gfx::Range>& AutofillDialogControllerImpl::
    LegalDocumentLinks() {
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

void AutofillDialogControllerImpl::GetWalletItems() {
  ScopedViewUpdates updates(view_.get());

  wallet_items_requested_ = true;
  wallet::WalletClient* wallet_client = GetWalletClient();
  wallet_client->CancelRequest();

  previously_selected_instrument_id_.clear();
  previously_selected_shipping_address_id_.clear();
  if (wallet_items_) {
    previous_default_instrument_id_ = wallet_items_->default_instrument_id();
    previous_default_shipping_address_id_ = wallet_items_->default_address_id();

    const wallet::WalletItems::MaskedInstrument* instrument =
        ActiveInstrument();
    if (instrument)
      previously_selected_instrument_id_ = instrument->object_id();

    const wallet::Address* address = ActiveShippingAddress();
    if (address)
      previously_selected_shipping_address_id_ = address->object_id();
  }

  last_wallet_items_fetch_timestamp_ = base::TimeTicks::Now();
  passive_failed_ = false;
  wallet_items_.reset();

  // The "Loading..." page should be showing now, which should cause the
  // account chooser to hide.
  view_->UpdateAccountChooser();
  wallet_client->GetWalletItems(transaction_amount_, transaction_currency_);
}

void AutofillDialogControllerImpl::HideSignIn() {
  ScopedViewUpdates updates(view_.get());
  signin_registrar_.RemoveAll();
  view_->HideSignIn();
  view_->UpdateAccountChooser();
}

AutofillDialogControllerImpl::DialogSignedInState
    AutofillDialogControllerImpl::SignedInState() const {
  if (wallet_error_notification_)
    return SIGN_IN_DISABLED;

  if (signin_helper_ || (wallet_items_requested_ && !wallet_items_))
    return REQUIRES_RESPONSE;

  if (!wallet_items_requested_)
    return NOT_CHECKED;

  if (wallet_items_->HasRequiredAction(wallet::GAIA_AUTH) ||
      passive_failed_) {
    return REQUIRES_SIGN_IN;
  }

  if (wallet_items_->HasRequiredAction(wallet::PASSIVE_GAIA_AUTH))
    return REQUIRES_PASSIVE_SIGN_IN;

  return SIGNED_IN;
}

void AutofillDialogControllerImpl::SignedInStateUpdated() {
  if (!ShouldShowSpinner())
    waiting_for_explicit_sign_in_response_ = false;

  switch (SignedInState()) {
    case SIGNED_IN:
      LogDialogLatencyToShow();
      break;

    case REQUIRES_SIGN_IN:
      if (handling_use_wallet_link_click_)
        SignInLinkClicked();
      // Fall through.
    case SIGN_IN_DISABLED:
      // Switch to the local account and refresh the dialog.
      signin_helper_.reset();
      OnWalletSigninError();
      handling_use_wallet_link_click_ = false;
      break;

    case REQUIRES_PASSIVE_SIGN_IN:
      // Attempt to passively sign in the user.
      DCHECK(!signin_helper_);
      signin_helper_.reset(new wallet::WalletSigninHelper(
          this,
          profile_->GetRequestContext()));
      signin_helper_->StartPassiveSignin(GetWalletClient()->user_index());
      break;

    case NOT_CHECKED:
    case REQUIRES_RESPONSE:
      break;
  }
}

void AutofillDialogControllerImpl::OnWalletOrSigninUpdate() {
  ScopedViewUpdates updates(view_.get());
  SignedInStateUpdated();
  SuggestionsUpdated();
  UpdateAccountChooserView();

  if (view_) {
    view_->UpdateButtonStrip();
    view_->UpdateOverlay();
  }

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

void AutofillDialogControllerImpl::ConstructLegalDocumentsText() {
  legal_documents_text_.clear();
  legal_document_link_ranges_.clear();

  if (!wallet_items_)
    return;

  PrefService* local_state = g_browser_process->local_state();
  // List of users who have accepted location sharing for fraud protection
  // on this device.
  const base::ListValue* accepted =
      local_state->GetList(::prefs::kAutofillDialogWalletLocationAcceptance);
  bool has_accepted_location_sharing =
      accepted->Find(base::StringValue(
          account_chooser_model_->GetActiveWalletAccountName())) !=
      accepted->end();

  if (wallet_items_->legal_documents().empty()) {
    if (!has_accepted_location_sharing) {
      legal_documents_text_ = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_LOCATION_DISCLOSURE);
    }

    return;
  }

  const std::vector<wallet::WalletItems::LegalDocument*>& documents =
      wallet_items_->legal_documents();
  // There should never be just one document because the privacy policy doc gets
  // tacked on the end of other documents.
  DCHECK_GE(documents.size(), 2U);

  std::vector<base::string16> link_names;
  for (size_t i = 0; i < documents.size(); ++i) {
    link_names.push_back(documents[i]->display_name());
  }

  int resource_id = 0;
  switch (documents.size()) {
    case 2U:
      resource_id = IDS_AUTOFILL_DIALOG_LEGAL_LINKS_2;
      break;
    case 3U:
      resource_id = IDS_AUTOFILL_DIALOG_LEGAL_LINKS_3;
      break;
    case 4U:
      resource_id = IDS_AUTOFILL_DIALOG_LEGAL_LINKS_4;
      break;
    case 5U:
      resource_id = IDS_AUTOFILL_DIALOG_LEGAL_LINKS_5;
      break;
    case 6U:
      resource_id = IDS_AUTOFILL_DIALOG_LEGAL_LINKS_6;
      break;
    default:
      // We can only handle so many documents. For lack of a better way of
      // handling document overflow, just error out if there are too many.
      DisableWallet(wallet::WalletClient::UNKNOWN_ERROR);
      return;
  }

  std::vector<size_t> offsets;
  base::string16 text =
      l10n_util::GetStringFUTF16(resource_id, link_names,&offsets);

  // Tack on the location string if need be.
  size_t base_offset = 0;
  if (!has_accepted_location_sharing) {
    text = l10n_util::GetStringFUTF16(
        IDS_AUTOFILL_DIALOG_LOCATION_DISCLOSURE_WITH_LEGAL_DOCS,
        text,
        &base_offset);
  }

  for (size_t i = 0; i < documents.size(); ++i) {
    size_t link_start = offsets[i] + base_offset;
    legal_document_link_ranges_.push_back(gfx::Range(
        link_start, link_start + documents[i]->display_name().size()));
  }
  legal_documents_text_ = text;
}

void AutofillDialogControllerImpl::ResetSectionInput(DialogSection section) {
  SetEditingExistingData(section, false);
  needs_validation_.erase(section);

  CountryComboboxModel* model = CountryComboboxModelForSection(section);
  if (model) {
    base::string16 country = model->GetItemAt(model->GetDefaultIndex());
    RebuildInputsForCountry(section, country, false);
  }

  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  for (DetailInputs::iterator it = inputs->begin();
       it != inputs->end(); ++it) {
    if (it->length != DetailInput::NONE) {
      it->initial_value.clear();
    } else if (!it->initial_value.empty() &&
               (it->type == ADDRESS_BILLING_COUNTRY ||
                it->type == ADDRESS_HOME_COUNTRY)) {
      GetValidator()->LoadRules(AutofillCountry::GetCountryCode(
          it->initial_value, g_browser_process->GetApplicationLocale()));
    }
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
  base::string16 unused, unused2;
  if (IsASuggestionItemKey(model->GetItemKeyForCheckedItem()) &&
      !SuggestionTextForSection(section, &unused, &unused2)) {
    SetEditingExistingData(section, true);
  }

  if (wrapper && IsEditingExistingData(section)) {
    base::string16 country =
        wrapper->GetInfo(AutofillType(CountryTypeForSection(section)));
    if (!country.empty()) {
      // There's no user input to restore here as this is only called after
      // resetting all section input.
      if (RebuildInputsForCountry(section, country, false))
        UpdateSection(section);
    }
    wrapper->FillInputs(MutableRequestedFieldsForSection(section));
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

FieldValueMap AutofillDialogControllerImpl::TakeUserInputSnapshot() {
  FieldValueMap snapshot;
  if (!view_)
    return snapshot;

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
    if (model->GetItemKeyForCheckedItem() != kAddNewItemKey)
      continue;

    FieldValueMap outputs;
    view_->GetUserInput(section, &outputs);
    // Remove fields that are empty, at their default values, or invalid.
    for (FieldValueMap::iterator it = outputs.begin(); it != outputs.end();
         ++it) {
      if (InputWasEdited(it->first, it->second) &&
          InputValidityMessage(section, it->first, it->second).empty()) {
        snapshot.insert(std::make_pair(it->first, it->second));
      }
    }
  }

  return snapshot;
}

void AutofillDialogControllerImpl::RestoreUserInputFromSnapshot(
    const FieldValueMap& snapshot) {
  if (snapshot.empty())
    return;

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    if (!SectionIsActive(section))
      continue;

    DetailInputs* inputs = MutableRequestedFieldsForSection(section);
    for (size_t i = 0; i < inputs->size(); ++i) {
      DetailInput* input = &(*inputs)[i];
      if (input->length != DetailInput::NONE) {
        input->initial_value =
            GetInfoFromInputs(snapshot, section, AutofillType(input->type));
      }
      if (InputWasEdited(input->type, input->initial_value))
        SuggestionsMenuModelForSection(section)->SetCheckedItem(kAddNewItemKey);
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

gfx::Image AutofillDialogControllerImpl::GetGeneratedCardImage(
    const base::string16& card_number,
    const base::string16& name,
    const SkColor& gradient_top,
    const SkColor& gradient_bottom) {
  const int kCardWidthPx = 300;
  const int kCardHeightPx = 190;
  const gfx::Size size(kCardWidthPx, kCardHeightPx);
  float scale_factor = ui::GetScaleFactorForNativeView(
      web_contents()->GetNativeView());
  gfx::Canvas canvas(size, scale_factor, false);

  gfx::Rect display_rect(size);

  skia::RefPtr<SkShader> shader = gfx::CreateGradientShader(
      0, size.height(), gradient_top, gradient_bottom);
  SkPaint paint;
  paint.setShader(shader.get());
  canvas.DrawRoundRect(display_rect, 8, paint);

  display_rect.Inset(20, 0, 0, 0);
  gfx::Font font(l10n_util::GetStringUTF8(IDS_FIXED_FONT_FAMILY), 18);
  gfx::FontList font_list(font);
  gfx::ShadowValues shadows;
  shadows.push_back(gfx::ShadowValue(gfx::Point(0, 1), 1.0, SK_ColorBLACK));
  canvas.DrawStringRectWithShadows(
      card_number,
      font_list,
      SK_ColorWHITE,
      display_rect, 0, 0, shadows);

  base::string16 capitalized_name = base::i18n::ToUpper(name);
  display_rect.Inset(0, size.height() / 2, 0, 0);
  canvas.DrawStringRectWithShadows(
      capitalized_name,
      font_list,
      SK_ColorWHITE,
      display_rect, 0, 0, shadows);

  gfx::ImageSkia skia(canvas.ExtractImageRep());
  return gfx::Image(skia);
}

void AutofillDialogControllerImpl::StartCardScramblingRefresher() {
  RefreshCardScramblingOverlay();
  card_scrambling_refresher_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(75),
      this,
      &AutofillDialogControllerImpl::RefreshCardScramblingOverlay);
}

void AutofillDialogControllerImpl::RefreshCardScramblingOverlay() {
  scrambled_card_number_ = GenerateRandomCardNumber();
  PushOverlayUpdate();
}

void AutofillDialogControllerImpl::PushOverlayUpdate() {
  if (view_) {
    ScopedViewUpdates updates(view_.get());
    view_->UpdateOverlay();
  }
}

const DetailInputs& AutofillDialogControllerImpl::RequestedFieldsForSection(
    DialogSection section) const {
  switch (section) {
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

    case ADDRESS_BILLING_COUNTRY:
      return billing_country_combobox_model_.get();

    case ADDRESS_HOME_COUNTRY:
      return shipping_country_combobox_model_.get();

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
  // TODO(estade): this can present a braindead menu (only 1 option) when
  // there's a wallet error.
  if (wallet_error_notification_ ||
      (SignedInState() == SIGNED_IN &&
       account_chooser_model_->HasAccountsToChoose() &&
       !ShouldShowSignInWebView())) {
    return account_chooser_model_.get();
  }

  // Otherwise, there is no menu, just a sign in link.
  return NULL;
}

gfx::Image AutofillDialogControllerImpl::AccountChooserImage() {
  if (!MenuModelForAccountChooser() && !ShouldShowSignInWebView()) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_WALLET_ICON);
  }

  return gfx::Image();
}

gfx::Image AutofillDialogControllerImpl::ButtonStripImage() const {
  if (IsPayingWithWallet()) {
    return ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_WALLET_LOGO);
  }

  return gfx::Image();
}

base::string16 AutofillDialogControllerImpl::LabelForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_CC:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC);
    case SECTION_BILLING:
    case SECTION_CC_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_BILLING);
    case SECTION_SHIPPING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_SHIPPING);
  }
  NOTREACHED();
  return base::string16();
}

SuggestionState AutofillDialogControllerImpl::SuggestionStateForSection(
    DialogSection section) {
  base::string16 vertically_compact, horizontally_compact;
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

  if (!IsPayingWithWallet() &&
      (section == SECTION_BILLING || section == SECTION_SHIPPING)) {
    // Also check if the address is invalid (rules may have loaded since
    // the dialog was shown).
    if (HasInvalidAddress(*GetManager()->GetProfileByGUID(item_key)))
      return false;
  }

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  return wrapper->GetDisplayText(vertically_compact, horizontally_compact);
}

base::string16 AutofillDialogControllerImpl::RequiredActionTextForSection(
    DialogSection section) const {
  if (section == SECTION_CC_BILLING && IsSubmitPausedOn(wallet::VERIFY_CVV)) {
    const wallet::WalletItems::MaskedInstrument* current_instrument =
        wallet_items_->GetInstrumentById(active_instrument_id_);
    if (current_instrument)
      return current_instrument->TypeAndLastFourDigits();

    FieldValueMap output;
    view_->GetUserInput(section, &output);
    CreditCard card;
    GetBillingInfoFromOutputs(output, &card, NULL, NULL);
    return card.TypeAndLastFourDigits();
  }

  return base::string16();
}

base::string16 AutofillDialogControllerImpl::ExtraSuggestionTextForSection(
    DialogSection section) const {
  if (section == SECTION_CC ||
      (section == SECTION_CC_BILLING && IsSubmitPausedOn(wallet::VERIFY_CVV))) {
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC);
  }

  return base::string16();
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
  if (section == SECTION_SHIPPING) {
    return scoped_ptr<DataModelWrapper>(
        new AutofillShippingAddressWrapper(profile));
  }
  DCHECK_EQ(SECTION_BILLING, section);
  return scoped_ptr<DataModelWrapper>(
      new AutofillProfileWrapper(profile));
}

gfx::Image AutofillDialogControllerImpl::SuggestionIconForSection(
    DialogSection section) {
  scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
  if (!model.get())
    return gfx::Image();

  return model->GetIcon();
}

gfx::Image AutofillDialogControllerImpl::ExtraSuggestionIconForSection(
    DialogSection section) {
  if (section != SECTION_CC && section != SECTION_CC_BILLING)
    return gfx::Image();

  scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
  if (!model.get())
    return gfx::Image();

  return CvcIconForCreditCardType(
      model->GetInfo(AutofillType(CREDIT_CARD_TYPE)));
}

FieldIconMap AutofillDialogControllerImpl::IconsForFields(
    const FieldValueMap& user_inputs) const {
  FieldIconMap result;
  base::string16 credit_card_type;

  FieldValueMap::const_iterator credit_card_iter =
      user_inputs.find(CREDIT_CARD_NUMBER);
  if (credit_card_iter != user_inputs.end()) {
    const base::string16& number = credit_card_iter->second;
    const std::string type = CreditCard::GetCreditCardType(number);
    credit_card_type = CreditCard::TypeForDisplay(type);
    result[CREDIT_CARD_NUMBER] = CreditCardIconForType(type);
  }

  if (!user_inputs.count(CREDIT_CARD_VERIFICATION_CODE))
    return result;

  result[CREDIT_CARD_VERIFICATION_CODE] =
      CvcIconForCreditCardType(credit_card_type);

  return result;
}

bool AutofillDialogControllerImpl::FieldControlsIcons(
    ServerFieldType type) const {
  return type == CREDIT_CARD_NUMBER;
}

base::string16 AutofillDialogControllerImpl::TooltipForField(
    ServerFieldType type) const {
  if (type == PHONE_HOME_WHOLE_NUMBER || type == PHONE_BILLING_WHOLE_NUMBER)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TOOLTIP_PHONE_NUMBER);

  return base::string16();
}

bool AutofillDialogControllerImpl::InputIsEditable(
    const DetailInput& input,
    DialogSection section) {
  if (section != SECTION_CC_BILLING || !IsPayingWithWallet())
    return true;

  if (input.type == CREDIT_CARD_NUMBER)
    return !IsEditingExistingData(section);

  // For CVC, only require (allow) input if the user has edited some other
  // aspect of the card.
  if (input.type == CREDIT_CARD_VERIFICATION_CODE &&
      IsEditingExistingData(section)) {
    FieldValueMap output;
    view_->GetUserInput(section, &output);
    WalletInstrumentWrapper wrapper(ActiveInstrument());

    for (FieldValueMap::iterator iter = output.begin(); iter != output.end();
         ++iter) {
      if (iter->first == input.type)
        continue;

      AutofillType type(iter->first);
      if (type.group() == CREDIT_CARD &&
          iter->second != wrapper.GetInfo(type)) {
        return true;
      }
    }

    return false;
  }

  return true;
}

// TODO(groby): Add more tests.
base::string16 AutofillDialogControllerImpl::InputValidityMessage(
    DialogSection section,
    ServerFieldType type,
    const base::string16& value) {
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

  AutofillType autofill_type(type);
  if (autofill_type.group() == ADDRESS_HOME ||
      autofill_type.group() == ADDRESS_BILLING) {
    return base::string16();
  }

  switch (autofill_type.GetStorableType()) {
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
            IDS_LIBADDRESSINPUT_MISSING_REQUIRED_FIELD);
      }
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      if (!InputWasEdited(CREDIT_CARD_EXP_4_DIGIT_YEAR, value)) {
        return l10n_util::GetStringUTF16(
            IDS_LIBADDRESSINPUT_MISSING_REQUIRED_FIELD);
      }
      break;

    case CREDIT_CARD_VERIFICATION_CODE:
      if (!value.empty() && !autofill::IsValidCreditCardSecurityCode(value)) {
        return l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_SECURITY_CODE);
      }
      break;

    case NAME_FULL:
      // Wallet requires a first and last billing name.
      if (IsPayingWithWallet() && !value.empty() &&
          !IsCardHolderNameValidForWallet(value)) {
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

  return value.empty() ? l10n_util::GetStringUTF16(
                             IDS_LIBADDRESSINPUT_MISSING_REQUIRED_FIELD) :
                         base::string16();
}

// TODO(groby): Also add tests.
ValidityMessages AutofillDialogControllerImpl::InputsAreValid(
    DialogSection section,
    const FieldValueMap& inputs) {
  ValidityMessages messages;
  if (inputs.empty())
    return messages;

  AddressValidator::Status status = AddressValidator::SUCCESS;
  if (section != SECTION_CC) {
    AutofillProfile profile;
    FillFormGroupFromOutputs(inputs, &profile);
    scoped_ptr<AddressData> address_data =
        i18n::CreateAddressDataFromAutofillProfile(
            profile, g_browser_process->GetApplicationLocale());
    address_data->language_code = AddressLanguageCodeForSection(section);

    Localization localization;
    localization.SetGetter(l10n_util::GetStringUTF8);
    FieldProblemMap problems;
    status = GetValidator()->ValidateAddress(*address_data, NULL, &problems);
    bool billing = section != SECTION_SHIPPING;

    for (FieldProblemMap::const_iterator iter = problems.begin();
         iter != problems.end(); ++iter) {
      bool sure = iter->second != MISSING_REQUIRED_FIELD;
      base::string16 text = base::UTF8ToUTF16(localization.GetErrorMessage(
          *address_data, iter->first, iter->second, true, false));
      messages.Set(i18n::TypeForField(iter->first, billing),
                   ValidityMessage(text, sure));
    }
  }

  for (FieldValueMap::const_iterator iter = inputs.begin();
       iter != inputs.end(); ++iter) {
    const ServerFieldType type = iter->first;
    base::string16 text = InputValidityMessage(section, type, iter->second);

    // Skip empty/unchanged fields in edit mode. If the individual field does
    // not have validation errors, assume it to be valid unless later proven
    // otherwise.
    bool sure = InputWasEdited(type, iter->second);

    if (sure && status == AddressValidator::RULES_NOT_READY &&
        !ComboboxModelForAutofillType(type) &&
        (AutofillType(type).group() == ADDRESS_HOME ||
         AutofillType(type).group() == ADDRESS_BILLING)) {
      DCHECK(text.empty());
      text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_WAITING_FOR_RULES);
      sure = false;
      needs_validation_.insert(section);
    }

    messages.Set(type, ValidityMessage(text, sure));
  }

  // For the convenience of using operator[].
  FieldValueMap& field_values = const_cast<FieldValueMap&>(inputs);
  // Validate the date formed by month and year field. (Autofill dialog is
  // never supposed to have 2-digit years, so not checked).
  if (field_values.count(CREDIT_CARD_EXP_4_DIGIT_YEAR) &&
      field_values.count(CREDIT_CARD_EXP_MONTH) &&
      InputWasEdited(CREDIT_CARD_EXP_4_DIGIT_YEAR,
                     field_values[CREDIT_CARD_EXP_4_DIGIT_YEAR]) &&
      InputWasEdited(CREDIT_CARD_EXP_MONTH,
                     field_values[CREDIT_CARD_EXP_MONTH])) {
    ValidityMessage year_message(base::string16(), true);
    ValidityMessage month_message(base::string16(), true);
    if (!IsCreditCardExpirationValid(field_values[CREDIT_CARD_EXP_4_DIGIT_YEAR],
                                     field_values[CREDIT_CARD_EXP_MONTH])) {
      // The dialog shows the same error message for the month and year fields.
      year_message.text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_DATE);
      month_message.text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_EXPIRATION_DATE);
    }
    messages.Set(CREDIT_CARD_EXP_4_DIGIT_YEAR, year_message);
    messages.Set(CREDIT_CARD_EXP_MONTH, month_message);
  }

  // If there is a credit card number and a CVC, validate them together.
  if (field_values.count(CREDIT_CARD_NUMBER) &&
      field_values.count(CREDIT_CARD_VERIFICATION_CODE)) {
    ValidityMessage ccv_message(base::string16(), true);
    if (!autofill::IsValidCreditCardSecurityCode(
            field_values[CREDIT_CARD_VERIFICATION_CODE],
            field_values[CREDIT_CARD_NUMBER])) {
      ccv_message.text = l10n_util::GetStringUTF16(
          IDS_AUTOFILL_DIALOG_VALIDATION_INVALID_CREDIT_CARD_SECURITY_CODE);
    }
    messages.Set(CREDIT_CARD_VERIFICATION_CODE, ccv_message);
  }

  // Validate the shipping phone number against the country code of the address.
  if (field_values.count(ADDRESS_HOME_COUNTRY) &&
      field_values.count(PHONE_HOME_WHOLE_NUMBER)) {
    messages.Set(
        PHONE_HOME_WHOLE_NUMBER,
        GetPhoneValidityMessage(field_values[ADDRESS_HOME_COUNTRY],
                                field_values[PHONE_HOME_WHOLE_NUMBER]));
  }

  // Validate the billing phone number against the country code of the address.
  if (field_values.count(ADDRESS_BILLING_COUNTRY) &&
      field_values.count(PHONE_BILLING_WHOLE_NUMBER)) {
    messages.Set(
        PHONE_BILLING_WHOLE_NUMBER,
        GetPhoneValidityMessage(field_values[ADDRESS_BILLING_COUNTRY],
                                field_values[PHONE_BILLING_WHOLE_NUMBER]));
  }

  return messages;
}

void AutofillDialogControllerImpl::UserEditedOrActivatedInput(
    DialogSection section,
    ServerFieldType type,
    gfx::NativeView parent_view,
    const gfx::Rect& content_bounds,
    const base::string16& field_contents,
    bool was_edit) {
  ScopedViewUpdates updates(view_.get());

  if (type == ADDRESS_BILLING_COUNTRY || type == ADDRESS_HOME_COUNTRY) {
    const FieldValueMap& snapshot = TakeUserInputSnapshot();

    // Clobber the inputs because the view's already been updated.
    RebuildInputsForCountry(section, field_contents, true);
    RestoreUserInputFromSnapshot(snapshot);
    UpdateSection(section);
  }

  // The rest of this method applies only to textfields while Autofill is
  // enabled. If a combobox or Autofill is disabled, bail.
  if (ComboboxModelForAutofillType(type) || !IsAutofillEnabled())
    return;

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

  std::vector<base::string16> popup_values, popup_labels, popup_icons;
  if (common::IsCreditCardType(type)) {
    GetManager()->GetCreditCardSuggestions(AutofillType(type),
                                           field_contents,
                                           &popup_values,
                                           &popup_labels,
                                           &popup_icons,
                                           &popup_guids_);
  } else {
    GetManager()->GetProfileSuggestions(
        AutofillType(type),
        field_contents,
        false,
        RequestedTypesForSection(section),
        base::Bind(&AutofillDialogControllerImpl::ShouldSuggestProfile,
                   base::Unretained(this), section),
        &popup_values,
        &popup_labels,
        &popup_icons,
        &popup_guids_);

    GetI18nValidatorSuggestions(section, type, &popup_values, &popup_labels,
                                &popup_icons);
  }

  if (popup_values.empty()) {
    HidePopup();
    return;
  }

  // |popup_input_type_| must be set before calling |Show()|.
  popup_input_type_ = type;
  popup_section_ = section;

  // TODO(estade): do we need separators and control rows like 'Clear
  // Form'?
  std::vector<int> popup_ids;
  for (size_t i = 0; i < popup_values.size(); ++i) {
    popup_ids.push_back(i);
  }

  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_,
      weak_ptr_factory_.GetWeakPtr(),
      NULL,
      parent_view,
      content_bounds,
      base::i18n::IsRTL() ?
          base::i18n::RIGHT_TO_LEFT : base::i18n::LEFT_TO_RIGHT);
  popup_controller_->Show(popup_values,
                          popup_labels,
                          popup_icons,
                          popup_ids);
}

void AutofillDialogControllerImpl::FocusMoved() {
  HidePopup();
}

bool AutofillDialogControllerImpl::ShouldShowErrorBubble() const {
  return popup_input_type_ == UNKNOWN_TYPE;
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

  if (notifications.empty() && MenuModelForAccountChooser()) {
    base::string16 text = l10n_util::GetStringUTF16(
        IsManuallyEditingAnySection() ?
            IDS_AUTOFILL_DIALOG_SAVE_DETAILS_IN_WALLET :
            IDS_AUTOFILL_DIALOG_USE_WALLET);
    DialogNotification notification(
        DialogNotification::WALLET_USAGE_CONFIRMATION,
        text);
    notification.set_tooltip_text(
        l10n_util::GetStringUTF16(
            IDS_AUTOFILL_DIALOG_SAVE_IN_WALLET_TOOLTIP));
    notification.set_checked(IsPayingWithWallet());
    notifications.push_back(notification);
  }

  if (IsPayingWithWallet() && !wallet::IsUsingProd()) {
    notifications.push_back(DialogNotification(
        DialogNotification::DEVELOPER_WARNING,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_NOT_PROD_WARNING)));
  }

  if (!invoked_from_same_origin_) {
    notifications.push_back(DialogNotification(
        DialogNotification::SECURITY_WARNING,
        l10n_util::GetStringFUTF16(IDS_AUTOFILL_DIALOG_SITE_WARNING,
                                   base::UTF8ToUTF16(source_url_.host()))));
  }

  return notifications;
}

void AutofillDialogControllerImpl::LinkClicked(const GURL& url) {
  OpenTabWithUrl(url);
}

void AutofillDialogControllerImpl::SignInLinkClicked() {
  ScopedViewUpdates updates(view_.get());

  if (SignedInState() == NOT_CHECKED) {
    handling_use_wallet_link_click_ = true;
    account_chooser_model_->SelectWalletAccount(0);
    FetchWalletCookie();
  } else if (signin_registrar_.IsEmpty()) {
    // Start sign in.
    waiting_for_explicit_sign_in_response_ = true;
    content::Source<content::NavigationController> source(view_->ShowSignIn());
    signin_registrar_.Add(
        this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);

    GetMetricLogger().LogDialogUiEvent(
        AutofillMetrics::DIALOG_UI_SIGNIN_SHOWN);
  } else {
    waiting_for_explicit_sign_in_response_ = false;
    HideSignIn();
  }

  view_->UpdateAccountChooser();
  view_->UpdateButtonStrip();
}

void AutofillDialogControllerImpl::NotificationCheckboxStateChanged(
    DialogNotification::Type type, bool checked) {
  if (type == DialogNotification::WALLET_USAGE_CONFIRMATION) {
    if (checked) {
      account_chooser_model_->SelectWalletAccount(
          GetWalletClient()->user_index());
    } else {
      account_chooser_model_->SelectUseAutofill();
    }

    AccountChoiceChanged();
  }
}

void AutofillDialogControllerImpl::LegalDocumentLinkClicked(
    const gfx::Range& range) {
  for (size_t i = 0; i < legal_document_link_ranges_.size(); ++i) {
    if (legal_document_link_ranges_[i] == range) {
      OpenTabWithUrl(wallet_items_->legal_documents()[i]->url());
      return;
    }
  }

  NOTREACHED();
}

bool AutofillDialogControllerImpl::OnCancel() {
  HidePopup();
  if (!is_submitting_)
    LogOnCancelMetrics();
  callback_.Run(
      AutofillClient::AutocompleteResultErrorCancel, base::string16(), NULL);
  return true;
}

bool AutofillDialogControllerImpl::OnAccept() {
  ScopedViewUpdates updates(view_.get());
  choose_another_instrument_or_address_ = false;
  wallet_server_validation_recoverable_ = true;
  HidePopup();

  // This must come before SetIsSubmitting().
  if (IsPayingWithWallet()) {
    // In the VERIFY_CVV case, hold onto the previously submitted cardholder
    // name.
    if (!IsSubmitPausedOn(wallet::VERIFY_CVV)) {
      submitted_cardholder_name_ =
          GetValueFromSection(SECTION_CC_BILLING, NAME_BILLING_FULL);

      // Snag the last four digits of the backing card now as it could be wiped
      // out if a CVC challenge happens.
      if (ActiveInstrument()) {
        backing_card_last_four_ = ActiveInstrument()->TypeAndLastFourDigits();
      } else {
        FieldValueMap output;
        view_->GetUserInput(SECTION_CC_BILLING, &output);
        CreditCard card;
        GetBillingInfoFromOutputs(output, &card, NULL, NULL);
        backing_card_last_four_ = card.TypeAndLastFourDigits();
      }
    }
    DCHECK(!submitted_cardholder_name_.empty());
    DCHECK(!backing_card_last_four_.empty());
  }

  SetIsSubmitting(true);

  if (IsSubmitPausedOn(wallet::VERIFY_CVV)) {
    DCHECK(!active_instrument_id_.empty());
    full_wallet_.reset();
    GetWalletClient()->AuthenticateInstrument(
        active_instrument_id_,
        base::UTF16ToUTF8(view_->GetCvc()));
    view_->UpdateOverlay();
  } else if (IsPayingWithWallet()) {
    AcceptLegalTerms();
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

void AutofillDialogControllerImpl::OnPopupShown() {
  ScopedViewUpdates update(view_.get());
  view_->UpdateErrorBubble();

  GetMetricLogger().LogDialogPopupEvent(AutofillMetrics::DIALOG_POPUP_SHOWN);
}

void AutofillDialogControllerImpl::OnPopupHidden() {}

void AutofillDialogControllerImpl::DidSelectSuggestion(
    const base::string16& value,
    int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::DidAcceptSuggestion(
    const base::string16& value,
    int identifier) {
  DCHECK_NE(UNKNOWN_TYPE, popup_input_type_);
  // Because |HidePopup()| can be called from |UpdateSection()|, remember the
  // type of the input for later here.
  const ServerFieldType popup_input_type = popup_input_type_;

  ScopedViewUpdates updates(view_.get());
  scoped_ptr<DataModelWrapper> wrapper;

  if (static_cast<size_t>(identifier) < popup_guids_.size()) {
    const PersonalDataManager::GUIDPair& pair = popup_guids_[identifier];
    if (common::IsCreditCardType(popup_input_type)) {
      wrapper.reset(new AutofillCreditCardWrapper(
          GetManager()->GetCreditCardByGUID(pair.first)));
    } else {
      wrapper.reset(new AutofillProfileWrapper(
          GetManager()->GetProfileByGUID(pair.first),
          AutofillType(popup_input_type),
          pair.second));
    }
  } else {
    wrapper.reset(new I18nAddressDataWrapper(
        &i18n_validator_suggestions_[identifier - popup_guids_.size()]));
  }

  // If the user hasn't switched away from the default country and |wrapper|'s
  // country differs from the |view_|'s, rebuild inputs and restore user data.
  const FieldValueMap snapshot = TakeUserInputSnapshot();
  bool billing_rebuilt = false, shipping_rebuilt = false;

  base::string16 billing_country =
      wrapper->GetInfo(AutofillType(ADDRESS_BILLING_COUNTRY));
  if (popup_section_ == ActiveBillingSection() &&
      !snapshot.count(ADDRESS_BILLING_COUNTRY) &&
      !billing_country.empty()) {
    billing_rebuilt = RebuildInputsForCountry(
        ActiveBillingSection(), billing_country, false);
  }

  base::string16 shipping_country =
      wrapper->GetInfo(AutofillType(ADDRESS_HOME_COUNTRY));
  if (popup_section_ == SECTION_SHIPPING &&
      !snapshot.count(ADDRESS_HOME_COUNTRY) &&
      !shipping_country.empty()) {
    shipping_rebuilt = RebuildInputsForCountry(
        SECTION_SHIPPING, shipping_country, false);
  }

  if (billing_rebuilt || shipping_rebuilt) {
    RestoreUserInputFromSnapshot(snapshot);
    if (billing_rebuilt)
      UpdateSection(ActiveBillingSection());
    if (shipping_rebuilt)
      UpdateSection(SECTION_SHIPPING);
  }

  DCHECK(SectionIsActive(popup_section_));
  wrapper->FillInputs(MutableRequestedFieldsForSection(popup_section_));
  view_->FillSection(popup_section_, popup_input_type);

  GetMetricLogger().LogDialogPopupEvent(
      AutofillMetrics::DIALOG_POPUP_FORM_FILLED);

  // TODO(estade): not sure why it's necessary to do this explicitly.
  HidePopup();
}

void AutofillDialogControllerImpl::RemoveSuggestion(
    const base::string16& value,
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
  size_t user_index = 0;
  if (IsSignInContinueUrl(load_details->entry->GetVirtualURL(), &user_index)) {
    GetWalletClient()->SetUserIndex(user_index);
    FetchWalletCookie();

    // NOTE: |HideSignIn()| may delete the WebContents which doesn't expect to
    // be deleted while committing a nav entry. Just call |HideSignIn()| later.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(&AutofillDialogControllerImpl::HideSignIn,
                   base::Unretained(this)));
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
      DCHECK(IsAutofillEnabled());
      GURL settings_url(chrome::kChromeUISettingsURL);
      url = settings_url.Resolve(chrome::kAutofillSubPage);
    } else {
      // Reset |last_wallet_items_fetch_timestamp_| to ensure that the Wallet
      // data is refreshed as soon as the user switches back to this tab after
      // potentially editing his data.
      last_wallet_items_fetch_timestamp_ = base::TimeTicks();
      size_t user_index = GetWalletClient()->user_index();
      url = SectionForSuggestionsMenuModel(*model) == SECTION_SHIPPING ?
          wallet::GetManageAddressesUrl(user_index) :
          wallet::GetManageInstrumentsUrl(user_index);
    }

    OpenTabWithUrl(url);
    return;
  }

  model->SetCheckedIndex(index);
  DialogSection section = SectionForSuggestionsMenuModel(*model);

  ResetSectionInput(section);
  ShowEditUiIfBadSuggestion(section);
  UpdateSection(section);
  view_->UpdateNotificationArea();
  UpdateForErrors();

  LogSuggestionItemSelectedMetric(*model);
}

////////////////////////////////////////////////////////////////////////////////
// wallet::WalletClientDelegate implementation.

const AutofillMetrics& AutofillDialogControllerImpl::GetMetricLogger() const {
  return metric_logger_;
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

  // TODO(dbeam): use the returned full wallet. http://crbug.com/224992
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
      break;

    case wallet::VERIFY_CVV:
      SuggestionsUpdated();
      break;

    default:
      DisableWallet(wallet::WalletClient::UNKNOWN_ERROR);
      return;
  }

  view_->UpdateNotificationArea();
  view_->UpdateButtonStrip();
  view_->UpdateOverlay();
}

void AutofillDialogControllerImpl::OnPassiveSigninSuccess() {
  FetchWalletCookie();
}

void AutofillDialogControllerImpl::OnPassiveSigninFailure(
    const GoogleServiceAuthError& error) {
  signin_helper_.reset();
  passive_failed_ = true;

  if (handling_use_wallet_link_click_ ||
      GetWalletClient()->user_index() != 0) {
    // TODO(estade): When a secondary account is selected and fails passive
    // auth, we show a sign in page. Currently we show the generic add account
    // page, but we should instead show sign in for the selected account.
    // http://crbug.com/323327
    SignInLinkClicked();
    handling_use_wallet_link_click_ = false;
  }

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

  if (wallet_items_ && !wallet_items_->ObfuscatedGaiaId().empty()) {
    // Making sure the user index is in sync shouldn't be necessary, but is an
    // extra precaution. But if there is no active account (such as in the
    // PASSIVE_AUTH case), stick with the old active account.
    GetWalletClient()->SetUserIndex(wallet_items_->active_account_index());

    std::vector<std::string> usernames;
    for (size_t i = 0; i < wallet_items_->gaia_accounts().size(); ++i) {
      usernames.push_back(wallet_items_->gaia_accounts()[i]->email_address());
    }
    account_chooser_model_->SetWalletAccounts(
        usernames, wallet_items_->active_account_index());
  }

  if (wallet_items_) {
    shipping_country_combobox_model_->SetCountries(
        *GetManager(),
        base::Bind(WalletCountryFilter,
                   acceptable_shipping_countries_,
                   wallet_items_->allowed_shipping_countries()));

    // If the site doesn't ship to any of the countries Wallet allows shipping
    // to, the merchant is not supported. (Note we generally shouldn't get here
    // as such a merchant wouldn't make it onto the Wallet whitelist.)
    if (shipping_country_combobox_model_->GetItemCount() == 0)
      DisableWallet(wallet::WalletClient::UNSUPPORTED_MERCHANT);
  }

  ConstructLegalDocumentsText();
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
  wallet::WalletClient* client = GetWalletClient();

  if (is_submitting_)
    client->CancelRequest();

  SetIsSubmitting(false);

  size_t selected_user_index =
      account_chooser_model_->GetActiveWalletAccountIndex();
  if (account_chooser_model_->WalletIsSelected() &&
      client->user_index() != selected_user_index) {
    client->SetUserIndex(selected_user_index);
    // Clear |wallet_items_| so we don't try to restore the selected instrument
    // and address.
    wallet_items_.reset();
    GetWalletItems();
  } else {
    SuggestionsUpdated();
    UpdateAccountChooserView();
  }
}

void AutofillDialogControllerImpl::AddAccount() {
  SignInLinkClicked();
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

bool AutofillDialogControllerImpl::IsSubmitPausedOn(
    wallet::RequiredAction required_action) const {
  return full_wallet_ && full_wallet_->HasRequiredAction(required_action);
}

void AutofillDialogControllerImpl::ShowNewCreditCardBubble(
    scoped_ptr<CreditCard> new_card,
    scoped_ptr<AutofillProfile> billing_profile) {
  NewCreditCardBubbleController::Show(web_contents(),
                                      new_card.Pass(),
                                      billing_profile.Pass());
}

void AutofillDialogControllerImpl::SubmitButtonDelayBegin() {
  submit_button_delay_timer_.Start(
      FROM_HERE,
      base::TimeDelta::FromMilliseconds(kSubmitButtonDelayMs),
      this,
      &AutofillDialogControllerImpl::OnSubmitButtonDelayEnd);
}

void AutofillDialogControllerImpl::SubmitButtonDelayEndForTesting() {
  DCHECK(submit_button_delay_timer_.IsRunning());
  submit_button_delay_timer_.user_task().Run();
  submit_button_delay_timer_.Stop();
}

void AutofillDialogControllerImpl::
    ClearLastWalletItemsFetchTimestampForTesting() {
  last_wallet_items_fetch_timestamp_ = base::TimeTicks();
}

AccountChooserModel* AutofillDialogControllerImpl::
    AccountChooserModelForTesting() {
  return account_chooser_model_.get();
}

bool AutofillDialogControllerImpl::IsSignInContinueUrl(
    const GURL& url,
    size_t* user_index) const {
  return wallet::IsSignInContinueUrl(url, user_index);
}

AutofillDialogControllerImpl::AutofillDialogControllerImpl(
    content::WebContents* contents,
    const FormData& form_structure,
    const GURL& source_url,
    const AutofillClient::ResultCallback& callback)
    : WebContentsObserver(contents),
      profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      initial_user_state_(AutofillMetrics::DIALOG_USER_STATE_UNKNOWN),
      form_structure_(form_structure),
      invoked_from_same_origin_(true),
      source_url_(source_url),
      callback_(callback),
      wallet_client_(profile_->GetRequestContext(), this, source_url),
      wallet_items_requested_(false),
      handling_use_wallet_link_click_(false),
      passive_failed_(false),
      suggested_cc_(this),
      suggested_billing_(this),
      suggested_cc_billing_(this),
      suggested_shipping_(this),
      cares_about_shipping_(true),
      popup_input_type_(UNKNOWN_TYPE),
      popup_section_(SECTION_MIN),
      waiting_for_explicit_sign_in_response_(false),
      has_accepted_legal_documents_(false),
      is_submitting_(false),
      choose_another_instrument_or_address_(false),
      wallet_server_validation_recoverable_(true),
      data_was_passed_back_(false),
      was_ui_latency_logged_(false),
      card_generated_animation_(2000, 60, this),
      weak_ptr_factory_(this) {
  DCHECK(!callback_.is_null());
}

AutofillDialogView* AutofillDialogControllerImpl::CreateView() {
  return AutofillDialogView::Create(this);
}

PersonalDataManager* AutofillDialogControllerImpl::GetManager() const {
  return PersonalDataManagerFactory::GetForProfile(profile_);
}

AddressValidator* AutofillDialogControllerImpl::GetValidator() {
  return validator_.get();
}

const wallet::WalletClient* AutofillDialogControllerImpl::GetWalletClient()
    const {
  return const_cast<AutofillDialogControllerImpl*>(this)->GetWalletClient();
}

wallet::WalletClient* AutofillDialogControllerImpl::GetWalletClient() {
  return &wallet_client_;
}

bool AutofillDialogControllerImpl::IsPayingWithWallet() const {
  return account_chooser_model_->WalletIsSelected() &&
         SignedInState() == SIGNED_IN;
}

void AutofillDialogControllerImpl::LoadRiskFingerprintData() {
  risk_data_.clear();

  uint64 obfuscated_gaia_id = 0;
  bool success = base::StringToUint64(wallet_items_->ObfuscatedGaiaId(),
                                      &obfuscated_gaia_id);
  DCHECK(success);

  gfx::Rect window_bounds;
  window_bounds = GetBaseWindowForWebContents(web_contents())->GetBounds();

  PrefService* user_prefs = profile_->GetPrefs();
  std::string charset = user_prefs->GetString(::prefs::kDefaultCharset);
  std::string accept_languages =
      user_prefs->GetString(::prefs::kAcceptLanguages);
  base::Time install_time = base::Time::FromTimeT(
      g_browser_process->metrics_service()->GetInstallDate());

  risk::GetFingerprint(
      obfuscated_gaia_id, window_bounds, web_contents(),
      chrome::VersionInfo().Version(), charset, accept_languages, install_time,
      g_browser_process->GetApplicationLocale(), GetUserAgent(),
      base::Bind(&AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData(
    scoped_ptr<risk::Fingerprint> fingerprint) {
  DCHECK(AreLegalDocumentsCurrent());

  std::string proto_data;
  fingerprint->SerializeToString(&proto_data);
  base::Base64Encode(proto_data, &risk_data_);

  SubmitWithWallet();
}

void AutofillDialogControllerImpl::OpenTabWithUrl(const GURL& url) {
  chrome::NavigateParams params(
      chrome::FindBrowserWithWebContents(web_contents()),
      url,
      content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

DialogSection AutofillDialogControllerImpl::ActiveBillingSection() const {
  return IsPayingWithWallet() ? SECTION_CC_BILLING : SECTION_BILLING;
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
  account_chooser_model_->SetHadWalletSigninError();
  GetWalletClient()->CancelRequest();
  LogDialogLatencyToShow();
}

void AutofillDialogControllerImpl::DisableWallet(
    wallet::WalletClient::ErrorType error_type) {
  signin_helper_.reset();
  wallet_items_.reset();
  wallet_errors_.clear();
  GetWalletClient()->CancelRequest();
  SetIsSubmitting(false);
  wallet_error_notification_ = GetWalletError(error_type);
  account_chooser_model_->SetHadWalletError();
}

void AutofillDialogControllerImpl::SuggestionsUpdated() {
  ScopedViewUpdates updates(view_.get());

  const FieldValueMap snapshot = TakeUserInputSnapshot();

  suggested_cc_.Reset();
  suggested_billing_.Reset();
  suggested_cc_billing_.Reset();
  suggested_shipping_.Reset();
  HidePopup();

  suggested_shipping_.AddKeyedItem(
      kSameAsBillingKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING));

  if (IsPayingWithWallet()) {
    const std::vector<wallet::Address*>& addresses =
        wallet_items_->addresses();

    bool shipping_same_as_billing = profile_->GetPrefs()->GetBoolean(
        ::prefs::kAutofillDialogWalletShippingSameAsBilling);

    if (shipping_same_as_billing)
      suggested_shipping_.SetCheckedItem(kSameAsBillingKey);

    for (size_t i = 0; i < addresses.size(); ++i) {
      std::string key = base::IntToString(i);
      suggested_shipping_.AddKeyedItemWithMinorText(
          key,
          addresses[i]->DisplayName(),
          addresses[i]->DisplayNameDetail());
      suggested_shipping_.SetEnabled(
          key,
          CanAcceptCountry(SECTION_SHIPPING,
                           addresses[i]->country_name_code()));

      // TODO(scr): Move this assignment outside the loop or comment why it
      // can't be there.
      const std::string default_shipping_address_id =
          GetIdToSelect(wallet_items_->default_address_id(),
                        previous_default_shipping_address_id_,
                        previously_selected_shipping_address_id_);

      if (!shipping_same_as_billing &&
          addresses[i]->object_id() == default_shipping_address_id) {
        suggested_shipping_.SetCheckedItem(key);
      }
    }

    if (!IsSubmitPausedOn(wallet::VERIFY_CVV)) {
      const std::vector<wallet::WalletItems::MaskedInstrument*>& instruments =
          wallet_items_->instruments();
      std::string first_active_instrument_key;
      std::string default_instrument_key;
      for (size_t i = 0; i < instruments.size(); ++i) {
        bool allowed = IsInstrumentAllowed(*instruments[i]) &&
            CanAcceptCountry(SECTION_BILLING,
                             instruments[i]->address().country_name_code());
        gfx::Image icon = instruments[i]->CardIcon();
        if (!allowed && !icon.IsEmpty()) {
          // Create a grayed disabled icon.
          SkBitmap disabled_bitmap = SkBitmapOperations::CreateHSLShiftedBitmap(
              *icon.ToSkBitmap(), kGrayImageShift);
          icon = gfx::Image(
              gfx::ImageSkia::CreateFrom1xBitmap(disabled_bitmap));
        }
        std::string key = base::IntToString(i);
        suggested_cc_billing_.AddKeyedItemWithMinorTextAndIcon(
            key,
            instruments[i]->DisplayName(),
            instruments[i]->DisplayNameDetail(),
            icon);
        suggested_cc_billing_.SetEnabled(key, allowed);

        if (allowed) {
          if (first_active_instrument_key.empty())
            first_active_instrument_key = key;

          const std::string default_instrument_id =
              GetIdToSelect(wallet_items_->default_instrument_id(),
                            previous_default_instrument_id_,
                            previously_selected_instrument_id_);
          if (instruments[i]->object_id() == default_instrument_id)
            default_instrument_key = key;
        }
      }

      suggested_cc_billing_.AddKeyedItem(
          kAddNewItemKey,
          l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_DETAILS));
      if (!wallet_items_->HasRequiredAction(wallet::SETUP_WALLET)) {
        suggested_cc_billing_.AddKeyedItemWithMinorText(
            kManageItemsKey,
            l10n_util::GetStringUTF16(
                IDS_AUTOFILL_DIALOG_MANAGE_BILLING_DETAILS),
                base::UTF8ToUTF16(wallet::GetManageInstrumentsUrl(0U).host()));
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
    shipping_country_combobox_model_->SetCountries(
        *GetManager(),
        base::Bind(AutofillCountryFilter, acceptable_shipping_countries_));

    if (IsAutofillEnabled()) {
      PersonalDataManager* manager = GetManager();
      const std::vector<CreditCard*>& cards = manager->GetCreditCards();
      ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
      for (size_t i = 0; i < cards.size(); ++i) {
        if (!i18ninput::CardHasCompleteAndVerifiedData(*cards[i]))
          continue;

        suggested_cc_.AddKeyedItemWithIcon(
            cards[i]->guid(),
            cards[i]->Label(),
            rb.GetImageNamed(CreditCard::IconResourceId(cards[i]->type())));
        suggested_cc_.SetEnabled(
            cards[i]->guid(),
            !ShouldDisallowCcType(cards[i]->TypeForDisplay()));
      }

      const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
      std::vector<base::string16> labels;
      AutofillProfile::CreateDifferentiatingLabels(
          profiles,
          g_browser_process->GetApplicationLocale(),
          &labels);
      DCHECK_EQ(labels.size(), profiles.size());
      for (size_t i = 0; i < profiles.size(); ++i) {
        const AutofillProfile& profile = *profiles[i];
        if (!i18ninput::AddressHasCompleteAndVerifiedData(
                profile, g_browser_process->GetApplicationLocale())) {
          continue;
        }

        // Don't add variants for addresses: name is part of credit card and
        // we'll just ignore email and phone number variants.
        suggested_shipping_.AddKeyedItem(profile.guid(), labels[i]);
        suggested_shipping_.SetEnabled(
            profile.guid(),
            CanAcceptCountry(
                SECTION_SHIPPING,
                base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))));
        if (!profile.GetRawInfo(EMAIL_ADDRESS).empty() &&
            !profile.IsPresentButInvalid(EMAIL_ADDRESS)) {
          suggested_billing_.AddKeyedItem(profile.guid(), labels[i]);
          suggested_billing_.SetEnabled(
              profile.guid(),
              CanAcceptCountry(
                  SECTION_BILLING,
                  base::UTF16ToUTF8(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))));
        }
      }
    }

    suggested_cc_.AddKeyedItem(
        kAddNewItemKey,
        l10n_util::GetStringUTF16(IsAutofillEnabled() ?
            IDS_AUTOFILL_DIALOG_ADD_CREDIT_CARD :
            IDS_AUTOFILL_DIALOG_ENTER_CREDIT_CARD));
    suggested_cc_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_CREDIT_CARD));
    suggested_billing_.AddKeyedItem(
        kAddNewItemKey,
        l10n_util::GetStringUTF16(IsAutofillEnabled() ?
            IDS_AUTOFILL_DIALOG_ADD_BILLING_ADDRESS :
            IDS_AUTOFILL_DIALOG_ENTER_BILLING_DETAILS));
    suggested_billing_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_BILLING_ADDRESS));
  }

  suggested_shipping_.AddKeyedItem(
      kAddNewItemKey,
      l10n_util::GetStringUTF16(IsPayingWithWallet() || IsAutofillEnabled() ?
          IDS_AUTOFILL_DIALOG_ADD_SHIPPING_ADDRESS :
          IDS_AUTOFILL_DIALOG_USE_DIFFERENT_SHIPPING_ADDRESS));

  if (!IsPayingWithWallet()) {
    if (IsAutofillEnabled()) {
      suggested_shipping_.AddKeyedItem(
          kManageItemsKey,
          l10n_util::GetStringUTF16(
              IDS_AUTOFILL_DIALOG_MANAGE_SHIPPING_ADDRESS));
    }
  } else if (!wallet_items_->HasRequiredAction(wallet::SETUP_WALLET)) {
    suggested_shipping_.AddKeyedItemWithMinorText(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_SHIPPING_ADDRESS),
        base::UTF8ToUTF16(wallet::GetManageAddressesUrl(0U).host()));
  }

  if (!IsPayingWithWallet() && IsAutofillEnabled()) {
    for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
      DialogSection section = static_cast<DialogSection>(i);
      if (!SectionIsActive(section))
        continue;

      // Set the starting choice for the menu. First set to the default in case
      // the GUID saved in prefs refers to a profile that no longer exists.
      std::string guid;
      GetDefaultAutofillChoice(section, &guid);
      SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
      model->SetCheckedItem(guid);
      if (GetAutofillChoice(section, &guid))
        model->SetCheckedItem(guid);
    }
  }

  if (view_)
    view_->ModelChanged();

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    ResetSectionInput(static_cast<DialogSection>(i));
  }

  FieldValueMap::const_iterator billing_it =
      snapshot.find(ADDRESS_BILLING_COUNTRY);
  if (billing_it != snapshot.end())
    RebuildInputsForCountry(ActiveBillingSection(), billing_it->second, true);

  FieldValueMap::const_iterator shipping_it =
      snapshot.find(ADDRESS_HOME_COUNTRY);
  if (shipping_it != snapshot.end())
    RebuildInputsForCountry(SECTION_SHIPPING, shipping_it->second, true);

  RestoreUserInputFromSnapshot(snapshot);

  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    if (!SectionIsActive(section))
      continue;

    ShowEditUiIfBadSuggestion(section);
    UpdateSection(section);
  }

  UpdateForErrors();
}

void AutofillDialogControllerImpl::FillOutputForSectionWithComparator(
    DialogSection section,
    const FormStructure::InputFieldComparator& compare) {
  if (!SectionIsActive(section))
    return;

  DetailInputs inputs;
  std::string country_code = CountryCodeForSection(section);
  BuildInputsForSection(section, country_code, &inputs,
                        MutableAddressLanguageCodeForSection(section));
  std::vector<ServerFieldType> types = common::TypesFromInputs(inputs);

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  if (wrapper) {
    // Only fill in data that is associated with this section.
    wrapper->FillFormStructure(types, compare, &form_structure_);

    // CVC needs special-casing because the CreditCard class doesn't store or
    // handle them. This isn't necessary when filling the combined CC and
    // billing section as CVC comes from |full_wallet_| in this case.
    if (section == SECTION_CC)
      SetOutputForFieldsOfType(CREDIT_CARD_VERIFICATION_CODE, view_->GetCvc());

    // When filling from Wallet data, use the email address associated with the
    // account. There is no other email address stored as part of a Wallet
    // address.
    if (section == SECTION_CC_BILLING) {
      SetOutputForFieldsOfType(
          EMAIL_ADDRESS, account_chooser_model_->GetActiveWalletAccountName());
    }
  } else {
    // The user manually input data. If using Autofill, save the info as new or
    // edited data. Always fill local data into |form_structure_|.
    FieldValueMap output;
    view_->GetUserInput(section, &output);

    if (section == SECTION_CC) {
      CreditCard card;
      FillFormGroupFromOutputs(output, &card);

      // The card holder name comes from the billing address section.
      card.SetRawInfo(CREDIT_CARD_NAME,
                      GetValueFromSection(SECTION_BILLING, NAME_BILLING_FULL));

      if (ShouldSaveDetailsLocally()) {
        card.set_origin(kAutofillDialogOrigin);

        std::string guid = GetManager()->SaveImportedCreditCard(card);
        newly_saved_data_model_guids_[section] = guid;
        DCHECK(!profile()->IsOffTheRecord());
        newly_saved_card_.reset(new CreditCard(card));
      }

      AutofillCreditCardWrapper card_wrapper(&card);
      card_wrapper.FillFormStructure(types, compare, &form_structure_);

      // Again, CVC needs special-casing. Fill it in directly from |output|.
      SetOutputForFieldsOfType(
          CREDIT_CARD_VERIFICATION_CODE,
          output[CREDIT_CARD_VERIFICATION_CODE]);
    } else {
      AutofillProfile profile;
      FillFormGroupFromOutputs(output, &profile);
      profile.set_language_code(AddressLanguageCodeForSection(section));

      if (ShouldSaveDetailsLocally()) {
        profile.set_origin(RulesAreLoaded(section) ?
            kAutofillDialogOrigin : source_url_.GetOrigin().spec());

        std::string guid = GetManager()->SaveImportedProfile(profile);
        newly_saved_data_model_guids_[section] = guid;
      }

      AutofillProfileWrapper profile_wrapper(&profile);
      profile_wrapper.FillFormStructure(types, compare, &form_structure_);
    }
  }
}

void AutofillDialogControllerImpl::FillOutputForSection(DialogSection section) {
  FillOutputForSectionWithComparator(
      section, base::Bind(common::ServerTypeMatchesField, section));
}

bool AutofillDialogControllerImpl::FormStructureCaresAboutSection(
    DialogSection section) const {
  // For now, only SECTION_SHIPPING may be omitted due to a site not asking for
  // any of the fields.
  if (section == SECTION_SHIPPING)
    return cares_about_shipping_;

  return true;
}

void AutofillDialogControllerImpl::SetOutputForFieldsOfType(
    ServerFieldType type,
    const base::string16& output) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    if (field->Type().GetStorableType() == type)
      field->value = output;
  }
}

base::string16 AutofillDialogControllerImpl::GetValueFromSection(
    DialogSection section,
    ServerFieldType type) {
  DCHECK(SectionIsActive(section));

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  if (wrapper)
    return wrapper->GetInfo(AutofillType(type));

  FieldValueMap output;
  view_->GetUserInput(section, &output);
  return output[type];
}

bool AutofillDialogControllerImpl::CanAcceptCountry(
    DialogSection section,
    const std::string& country_code) {
  DCHECK_EQ(2U, country_code.size());

  if (section == SECTION_CC_BILLING)
    return LowerCaseEqualsASCII(country_code, "us");

  CountryComboboxModel* model = CountryComboboxModelForSection(section);
  const std::vector<AutofillCountry*>& countries = model->countries();
  for (size_t i = 0; i < countries.size(); ++i) {
    if (countries[i] && countries[i]->country_code() == country_code)
      return true;
  }

  return false;
}

bool AutofillDialogControllerImpl::ShouldSuggestProfile(
    DialogSection section,
    const AutofillProfile& profile) {
  std::string country_code =
      base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY));
  return country_code.empty() || CanAcceptCountry(section, country_code);
}

SuggestionsMenuModel* AutofillDialogControllerImpl::
    SuggestionsMenuModelForSection(DialogSection section) {
  switch (section) {
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
  if (&model == &suggested_cc_)
    return SECTION_CC;

  if (&model == &suggested_billing_)
    return SECTION_BILLING;

  if (&model == &suggested_cc_billing_)
    return SECTION_CC_BILLING;

  DCHECK_EQ(&model, &suggested_shipping_);
  return SECTION_SHIPPING;
}

CountryComboboxModel* AutofillDialogControllerImpl::
    CountryComboboxModelForSection(DialogSection section) {
  if (section == SECTION_BILLING)
    return billing_country_combobox_model_.get();

  if (section == SECTION_SHIPPING)
    return shipping_country_combobox_model_.get();

  return NULL;
}

void AutofillDialogControllerImpl::GetI18nValidatorSuggestions(
    DialogSection section,
    ServerFieldType type,
    std::vector<base::string16>* popup_values,
    std::vector<base::string16>* popup_labels,
    std::vector<base::string16>* popup_icons) {
  AddressField focused_field;
  if (!i18n::FieldForType(type, &focused_field))
    return;

  FieldValueMap inputs;
  view_->GetUserInput(section, &inputs);

  AutofillProfile profile;
  FillFormGroupFromOutputs(inputs, &profile);

  scoped_ptr<AddressData> user_input =
      i18n::CreateAddressDataFromAutofillProfile(
          profile, g_browser_process->GetApplicationLocale());
  user_input->language_code = AddressLanguageCodeForSection(section);

  static const size_t kSuggestionsLimit = 10;
  AddressValidator::Status status = GetValidator()->GetSuggestions(
      *user_input, focused_field, kSuggestionsLimit,
      &i18n_validator_suggestions_);

  if (status != AddressValidator::SUCCESS)
    return;

  for (size_t i = 0; i < i18n_validator_suggestions_.size(); ++i) {
    popup_values->push_back(base::UTF8ToUTF16(
        i18n_validator_suggestions_[i].GetFieldValue(focused_field)));

    // Disambiguate the suggestion by showing the smallest administrative
    // region of the suggested address:
    //    ADMIN_AREA > LOCALITY > DEPENDENT_LOCALITY
    popup_labels->push_back(base::string16());
    for (int field = DEPENDENT_LOCALITY; field >= ADMIN_AREA; --field) {
      const std::string& field_value =
          i18n_validator_suggestions_[i].GetFieldValue(
              static_cast<AddressField>(field));
      if (focused_field != field && !field_value.empty()) {
        popup_labels->back().assign(base::UTF8ToUTF16(field_value));
        break;
      }
    }
  }
  popup_icons->resize(popup_values->size());
}

DetailInputs* AutofillDialogControllerImpl::MutableRequestedFieldsForSection(
    DialogSection section) {
  return const_cast<DetailInputs*>(&RequestedFieldsForSection(section));
}

std::string* AutofillDialogControllerImpl::MutableAddressLanguageCodeForSection(
    DialogSection section) {
  switch (section) {
    case SECTION_BILLING:
    case SECTION_CC_BILLING:
      return &billing_address_language_code_;
    case SECTION_SHIPPING:
      return &shipping_address_language_code_;
    case SECTION_CC:
      return NULL;
  }
  NOTREACHED();
  return NULL;
}

std::string AutofillDialogControllerImpl::AddressLanguageCodeForSection(
    DialogSection section) {
  std::string* language_code = MutableAddressLanguageCodeForSection(section);
  return language_code != NULL ? *language_code : std::string();
}

std::vector<ServerFieldType> AutofillDialogControllerImpl::
    RequestedTypesForSection(DialogSection section) const {
  return common::TypesFromInputs(RequestedFieldsForSection(section));
}

std::string AutofillDialogControllerImpl::CountryCodeForSection(
    DialogSection section) {
  base::string16 country;

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  if (wrapper) {
    country = wrapper->GetInfo(AutofillType(CountryTypeForSection(section)));
  } else {
    FieldValueMap outputs;
    view_->GetUserInput(section, &outputs);
    country = outputs[CountryTypeForSection(section)];
  }

  return AutofillCountry::GetCountryCode(
      country, g_browser_process->GetApplicationLocale());
}

bool AutofillDialogControllerImpl::RebuildInputsForCountry(
    DialogSection section,
    const base::string16& country_name,
    bool should_clobber) {
  CountryComboboxModel* model = CountryComboboxModelForSection(section);
  if (!model)
    return false;

  std::string country_code = AutofillCountry::GetCountryCode(
      country_name, g_browser_process->GetApplicationLocale());
  DCHECK(CanAcceptCountry(section, country_code));

  if (view_ && !should_clobber) {
    FieldValueMap outputs;
    view_->GetUserInput(section, &outputs);

    // If |country_name| is the same as the view, no-op and let the caller know.
    if (outputs[CountryTypeForSection(section)] == country_name)
      return false;
  }

  DetailInputs* inputs = MutableRequestedFieldsForSection(section);
  inputs->clear();
  BuildInputsForSection(section, country_code, inputs,
                        MutableAddressLanguageCodeForSection(section));

  if (!country_code.empty()) {
    GetValidator()->LoadRules(AutofillCountry::GetCountryCode(
        country_name, g_browser_process->GetApplicationLocale()));
  }

  return true;
}

void AutofillDialogControllerImpl::HidePopup() {
  if (popup_controller_)
    popup_controller_->Hide();
  popup_input_type_ = UNKNOWN_TYPE;
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

bool AutofillDialogControllerImpl::IsAutofillEnabled() const {
  return profile_->GetPrefs()->GetBoolean(prefs::kAutofillEnabled);
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

  if (!IsPayingWithWallet() &&
      ShouldDisallowCcType(CreditCard::TypeForDisplay(
          CreditCard::GetCreditCardType(number)))) {
    int ids = IDS_AUTOFILL_DIALOG_VALIDATION_UNACCEPTED_GENERIC_CARD;
    const char* const type = CreditCard::GetCreditCardType(number);
    if (type == kAmericanExpressCard)
      ids = IDS_AUTOFILL_DIALOG_VALIDATION_UNACCEPTED_AMEX;
    else if (type == kDiscoverCard)
      ids = IDS_AUTOFILL_DIALOG_VALIDATION_UNACCEPTED_DISCOVER;
    else if (type == kMasterCard)
      ids = IDS_AUTOFILL_DIALOG_VALIDATION_UNACCEPTED_MASTERCARD;
    else if (type == kVisaCard)
      ids = IDS_AUTOFILL_DIALOG_VALIDATION_UNACCEPTED_VISA;

    return l10n_util::GetStringUTF16(ids);
  }

  base::string16 message;
  if (IsPayingWithWallet() && !wallet_items_->SupportsCard(number, &message))
    return message;

  // Card number is good and supported.
  return base::string16();
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

  FieldValueMap detail_outputs;
  view_->GetUserInput(section, &detail_outputs);
  return !InputsAreValid(section, detail_outputs).HasSureErrors();
}

bool AutofillDialogControllerImpl::RulesAreLoaded(DialogSection section) {
  AddressData address_data;
  address_data.region_code = CountryCodeForSection(section);
  AddressValidator::Status status = GetValidator()->ValidateAddress(
      address_data, NULL, NULL);
  return status == AddressValidator::SUCCESS;
}

bool AutofillDialogControllerImpl::IsCreditCardExpirationValid(
    const base::string16& year,
    const base::string16& month) const {
  // If the expiration is in the past as per the local clock, it's invalid.
  base::Time now = base::Time::Now();
  if (!autofill::IsValidCreditCardExpirationDate(year, month, now))
    return false;

  const wallet::WalletItems::MaskedInstrument* instrument =
      ActiveInstrument();
  if (instrument) {
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

bool AutofillDialogControllerImpl::ShouldDisallowCcType(
    const base::string16& type) const {
  if (acceptable_cc_types_.empty())
    return false;

  if (acceptable_cc_types_.find(base::i18n::ToUpper(type)) ==
          acceptable_cc_types_.end()) {
    return true;
  }

  return false;
}

bool AutofillDialogControllerImpl::HasInvalidAddress(
    const AutofillProfile& profile) {
  scoped_ptr<AddressData> address_data =
      i18n::CreateAddressDataFromAutofillProfile(
          profile, g_browser_process->GetApplicationLocale());

  FieldProblemMap problems;
  GetValidator()->ValidateAddress(*address_data, NULL, &problems);
  return !problems.empty();
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
    view_->UpdateOverlay();
    view_->UpdateNotificationArea();
  }
}

bool AutofillDialogControllerImpl::AreLegalDocumentsCurrent() const {
  return has_accepted_legal_documents_ ||
      (wallet_items_ && wallet_items_->legal_documents().empty());
}

void AutofillDialogControllerImpl::AcceptLegalTerms() {
  content::GeolocationProvider::GetInstance()->UserDidOptIntoLocationServices();
  PrefService* local_state = g_browser_process->local_state();
  ListPrefUpdate accepted(
      local_state, ::prefs::kAutofillDialogWalletLocationAcceptance);
  accepted->AppendIfNotPresent(new base::StringValue(
      account_chooser_model_->GetActiveWalletAccountName()));

  if (AreLegalDocumentsCurrent()) {
    LoadRiskFingerprintData();
  } else {
    GetWalletClient()->AcceptLegalDocuments(
        wallet_items_->legal_documents(),
        wallet_items_->google_transaction_id());
  }
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
    }
  }

  // If there's neither an address nor instrument to save, |GetFullWallet()|
  // is called when the risk fingerprint is loaded.
  if (!active_instrument_id_.empty() &&
      (!active_address_id_.empty() || !IsShippingAddressRequired())) {
    GetFullWallet();
    return;
  }

  GetWalletClient()->SaveToWallet(
      inputted_instrument.Pass(),
      inputted_address.Pass(),
      IsEditingExistingData(SECTION_CC_BILLING) ? active_instrument : NULL,
      IsEditingExistingData(SECTION_SHIPPING) ? active_address : NULL);
}

scoped_ptr<wallet::Instrument> AutofillDialogControllerImpl::
    CreateTransientInstrument() {
  if (!active_instrument_id_.empty())
    return scoped_ptr<wallet::Instrument>();

  FieldValueMap output;
  view_->GetUserInput(SECTION_CC_BILLING, &output);

  CreditCard card;
  AutofillProfile profile;
  base::string16 cvc;
  GetBillingInfoFromOutputs(output, &card, &cvc, &profile);
  CanonicalizeState(validator_.get(), &profile);

  return scoped_ptr<wallet::Instrument>(
      new wallet::Instrument(card, cvc, profile));
}

scoped_ptr<wallet::Address>AutofillDialogControllerImpl::
    CreateTransientAddress() {
  // If not using billing for shipping, just scrape the view.
  FieldValueMap output;
  view_->GetUserInput(SECTION_SHIPPING, &output);

  AutofillProfile profile;
  FillFormGroupFromOutputs(output, &profile);
  profile.set_language_code(shipping_address_language_code_);
  CanonicalizeState(validator_.get(), &profile);

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
      wallet_items_->google_transaction_id(),
      capabilities,
      wallet_items_->HasRequiredAction(wallet::SETUP_WALLET)));
}

void AutofillDialogControllerImpl::HandleSaveOrUpdateRequiredActions(
    const std::vector<wallet::RequiredAction>& required_actions) {
  DCHECK(!required_actions.empty());

  // TODO(ahutter): Investigate if we need to support more generic actions on
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
  if (IsPayingWithWallet()) {
    ScopedViewUpdates updates(view_.get());
    view_->UpdateOverlay();

    card_generated_animation_.Start();
    return;
  }
  DoFinishSubmit();
}

void AutofillDialogControllerImpl::AnimationProgressed(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &card_generated_animation_);
  PushOverlayUpdate();
}

void AutofillDialogControllerImpl::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK_EQ(animation, &card_generated_animation_);
  DoFinishSubmit();
}

void AutofillDialogControllerImpl::OnAddressValidationRulesLoaded(
    const std::string& country_code,
    bool success) {
  // Rules may load instantly (during initialization, before the view is
  // even ready). We'll validate when the view is created.
  if (!view_)
    return;

  ScopedViewUpdates updates(view_.get());

  // TODO(dbeam): should we retry on failure?
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    if (!SectionIsActive(section) ||
        CountryCodeForSection(section) != country_code) {
      continue;
    }

    if (IsManuallyEditingSection(section) &&
        needs_validation_.count(section)) {
      view_->ValidateSection(section);
      needs_validation_.erase(section);
    } else if (success) {
      ShowEditUiIfBadSuggestion(section);
      UpdateSection(section);
    }
  }
}

void AutofillDialogControllerImpl::DoFinishSubmit() {
  FillOutputForSection(SECTION_CC);
  FillOutputForSection(SECTION_BILLING);
  FillOutputForSection(SECTION_CC_BILLING);

  if (ShouldUseBillingForShipping()) {
    FillOutputForSectionWithComparator(
        SECTION_BILLING,
        base::Bind(ServerTypeMatchesShippingField));
    FillOutputForSectionWithComparator(
        SECTION_CC,
        base::Bind(ServerTypeMatchesShippingField));
    FillOutputForSectionWithComparator(
        SECTION_CC_BILLING,
        base::Bind(ServerTypeMatchesShippingField));
  } else {
    FillOutputForSection(SECTION_SHIPPING);
  }

  if (IsPayingWithWallet()) {
    if (SectionIsActive(SECTION_SHIPPING)) {
      profile_->GetPrefs()->SetBoolean(
          ::prefs::kAutofillDialogWalletShippingSameAsBilling,
          suggested_shipping_.GetItemKeyForCheckedItem() == kSameAsBillingKey);
    }
  } else if (ShouldOfferToSaveInChrome()) {
    for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
      DialogSection section = static_cast<DialogSection>(i);
      if (!SectionIsActive(section))
        continue;

      SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
      std::string item_key = model->GetItemKeyForCheckedItem();
      if (IsASuggestionItemKey(item_key) || item_key == kSameAsBillingKey) {
        PersistAutofillChoice(section, item_key);
      } else if (item_key == kAddNewItemKey && ShouldSaveDetailsLocally()) {
        DCHECK(newly_saved_data_model_guids_.count(section));
        PersistAutofillChoice(section, newly_saved_data_model_guids_[section]);
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
      account_chooser_model_->HasAccountsToChoose()) {
    profile_->GetPrefs()->SetBoolean(
        ::prefs::kAutofillDialogPayWithoutWallet,
        !account_chooser_model_->WalletIsSelected());
  }

  LogOnFinishSubmitMetrics();

  // Callback should be called as late as possible.
  callback_.Run(AutofillClient::AutocompleteResultSuccess,
                base::string16(),
                &form_structure_);
  data_was_passed_back_ = true;

  // This might delete us.
  Hide();
}

void AutofillDialogControllerImpl::PersistAutofillChoice(
    DialogSection section,
    const std::string& guid) {
  DCHECK(!IsPayingWithWallet() && ShouldOfferToSaveInChrome());
  scoped_ptr<base::DictionaryValue> value(new base::DictionaryValue());
  value->SetString(kGuidPrefKey, guid);

  DictionaryPrefUpdate updater(profile()->GetPrefs(),
                               ::prefs::kAutofillDialogAutofillDefault);
  base::DictionaryValue* autofill_choice = updater.Get();
  autofill_choice->Set(SectionToPrefString(section), value.release());
}

void AutofillDialogControllerImpl::GetDefaultAutofillChoice(
    DialogSection section,
    std::string* guid) {
  DCHECK(!IsPayingWithWallet() && IsAutofillEnabled());
  // The default choice is the first thing in the menu that is a suggestion
  // item.
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  for (int i = 0; i < model->GetItemCount(); ++i) {
    // Try the first suggestion item that is enabled.
    if (IsASuggestionItemKey(model->GetItemKeyAt(i)) && model->IsEnabledAt(i)) {
      *guid = model->GetItemKeyAt(i);
      return;
    // Fall back to the first non-suggestion key.
    } else if (!IsASuggestionItemKey(model->GetItemKeyAt(i)) && guid->empty()) {
      *guid = model->GetItemKeyAt(i);
    }
  }
}

bool AutofillDialogControllerImpl::GetAutofillChoice(DialogSection section,
                                                     std::string* guid) {
  DCHECK(!IsPayingWithWallet() && IsAutofillEnabled());
  const base::DictionaryValue* choices = profile()->GetPrefs()->GetDictionary(
      ::prefs::kAutofillDialogAutofillDefault);
  if (!choices)
    return false;

  const base::DictionaryValue* choice = NULL;
  if (!choices->GetDictionary(SectionToPrefString(section), &choice))
    return false;

  choice->GetString(kGuidPrefKey, guid);
  return true;
}

void AutofillDialogControllerImpl::LogOnFinishSubmitMetrics() {
  GetMetricLogger().LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      AutofillMetrics::DIALOG_ACCEPTED);

  GetMetricLogger().LogDialogUiEvent(AutofillMetrics::DIALOG_UI_ACCEPTED);

  AutofillMetrics::DialogDismissalState dismissal_state;
  if (!IsManuallyEditingAnySection()) {
    dismissal_state = IsPayingWithWallet() ?
        AutofillMetrics::DIALOG_ACCEPTED_EXISTING_WALLET_DATA :
        AutofillMetrics::DIALOG_ACCEPTED_EXISTING_AUTOFILL_DATA;
  } else if (IsPayingWithWallet()) {
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_WALLET;
  } else if (ShouldSaveDetailsLocally()) {
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_AUTOFILL;
  } else {
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_NO_SAVE;
  }

  GetMetricLogger().LogDialogDismissalState(dismissal_state);
}

void AutofillDialogControllerImpl::LogOnCancelMetrics() {
  GetMetricLogger().LogDialogUiEvent(AutofillMetrics::DIALOG_UI_CANCELED);

  AutofillMetrics::DialogDismissalState dismissal_state;
  if (ShouldShowSignInWebView())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_DURING_SIGNIN;
  else if (!IsManuallyEditingAnySection())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_NO_EDITS;
  else if (AllSectionsAreValid())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS;
  else
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_WITH_INVALID_FIELDS;

  GetMetricLogger().LogDialogDismissalState(dismissal_state);

  GetMetricLogger().LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
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

  GetMetricLogger().LogDialogUiEvent(dialog_ui_event);
}

void AutofillDialogControllerImpl::LogDialogLatencyToShow() {
  if (was_ui_latency_logged_)
    return;

  GetMetricLogger().LogDialogLatencyToShow(
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
      FieldValueMap outputs;
      view_->GetUserInput(SECTION_BILLING, &outputs);
      billing_profile.reset(new AutofillProfile);
      FillFormGroupFromOutputs(outputs, billing_profile.get());
      billing_profile->set_language_code(billing_address_language_code_);
    } else {
      // Just snag the currently suggested profile.
      std::string item_key = SuggestionsMenuModelForSection(SECTION_BILLING)->
          GetItemKeyForCheckedItem();
      AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
      billing_profile.reset(new AutofillProfile(*profile));
    }

    ShowNewCreditCardBubble(newly_saved_card_.Pass(),
                            billing_profile.Pass());
    return;
  }

  if (!full_wallet_ || !full_wallet_->billing_address())
    return;

  GeneratedCreditCardBubbleController::Show(
      web_contents(),
      full_wallet_->TypeAndLastFourDigits(),
      backing_card_last_four_);
}

void AutofillDialogControllerImpl::OnSubmitButtonDelayEnd() {
  if (!view_)
    return;
  ScopedViewUpdates updates(view_.get());
  view_->UpdateButtonStrip();
}

void AutofillDialogControllerImpl::FetchWalletCookie() {
  net::URLRequestContextGetter* request_context = profile_->GetRequestContext();
  signin_helper_.reset(new wallet::WalletSigninHelper(this, request_context));
  signin_helper_->StartWalletCookieValueFetch();
}

}  // namespace autofill
