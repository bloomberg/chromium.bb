// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <algorithm>
#include <map>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/pref_service.h"
#include "base/prefs/scoped_user_pref_update.h"
#include "base/rand_util.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk_util.h"
#include "chrome/browser/autofill/validation_rules_storage_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_common.h"
#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/chromium_strings.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/address_i18n.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_model.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_names.h"
#include "components/autofill/core/browser/detail_input.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/browser/server_field_types_util.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/autofill/core/common/form_data.h"
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
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "grit/platform_locale_settings.h"
#include "grit/theme_resources.h"
#include "net/cert/cert_status_flags.h"
#include "third_party/libaddressinput/chromium/chrome_metadata_source.h"
#include "third_party/libaddressinput/chromium/chrome_storage_impl.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_data.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_field.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_problem.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/combobox_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/skia_util.h"

using ::i18n::addressinput::AddressData;
using ::i18n::addressinput::AddressField;
using ::i18n::addressinput::AddressProblem;
using ::i18n::addressinput::ADMIN_AREA;
using ::i18n::addressinput::DEPENDENT_LOCALITY;
using ::i18n::addressinput::FieldProblemMap;
using ::i18n::addressinput::Localization;
using ::i18n::addressinput::MISSING_REQUIRED_FIELD;

namespace autofill {

namespace {

const char kAddNewItemKey[] = "add-new-item";
const char kManageItemsKey[] = "manage-items";
const char kSameAsBillingKey[] = "same-as-billing";

// Keys for the kAutofillDialogAutofillDefault pref dictionary (do not change
// these values).
const char kGuidPrefKey[] = "guid";

// This string is stored along with saved addresses and credit cards in the
// WebDB, and hence should not be modified, so that it remains consistent over
// time.
const char kAutofillDialogOrigin[] = "Chrome Autofill dialog";

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
    info =
        base::ASCIIToUTF16(CountryNames::GetInstance()->GetCountryCode(info));
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
  return ServerTypeEncompassesFieldType(
      type, AutofillType(AutofillType::GetEquivalentBillingFieldType(
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

// Returns a string descriptor for a DialogSection, for use with prefs (do not
// change these values).
std::string SectionToPrefString(DialogSection section) {
  switch (section) {
    case SECTION_CC:
      return "cc";

    case SECTION_BILLING:
      return "billing";

    case SECTION_SHIPPING:
      return "shipping";
  }

  NOTREACHED();
  return std::string();
}

gfx::Image CreditCardIconForType(const std::string& credit_card_type) {
  const int input_card_idr = CreditCard::IconResourceId(credit_card_type);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (input_card_idr == IDR_AUTOFILL_CC_GENERIC) {
    // When the credit card type is unknown, no image should be shown. However,
    // to simplify the view code on Mac, save space for the credit card image by
    // returning a transparent image of the appropriate size. All credit card
    // icons are the same size.
    return gfx::Image(gfx::ImageSkiaOperations::CreateTransparentImage(
        rb.GetImageNamed(IDR_AUTOFILL_CC_GENERIC).AsImageSkia(), 0));
  }
  return rb.GetImageNamed(input_card_idr);
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

ValidityMessage GetPhoneValidityMessage(const base::string16& country_name,
                                        const base::string16& number) {
  std::string region =
      CountryNames::GetInstance()->GetCountryCode(country_name);
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
      BuildInputs(kCCInputs, arraysize(kCCInputs), inputs);
      break;
    }

    case SECTION_BILLING: {
      i18ninput::BuildAddressInputs(common::ADDRESS_TYPE_BILLING,
                                    country_code, inputs, language_code);
      BuildInputs(kBillingPhoneInputs, arraysize(kBillingPhoneInputs), inputs);
      BuildInputs(kEmailInputs, arraysize(kEmailInputs), inputs);
      break;
    }

    case SECTION_SHIPPING: {
      i18ninput::BuildAddressInputs(common::ADDRESS_TYPE_SHIPPING,
                                    country_code, inputs, language_code);
      BuildInputs(kShippingPhoneInputs, arraysize(kShippingPhoneInputs),
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

  AutofillMetrics::LogDialogInitialUserState(initial_user_state_);
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
  form_structure_.ParseFieldTypesFromAutocompleteAttributes();

  // Fail if the author didn't specify autocomplete types.
  if (!form_structure_.has_author_specified_types()) {
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
    if (type.html_type() != HTML_TYPE_UNSPECIFIED &&
        type.group() == CREDIT_CARD) {
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
  AutofillMetrics::LogDialogUiEvent(AutofillMetrics::DIALOG_UI_SHOWN);

  AutofillMetrics::LogDialogSecurityMetric(
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
    AutofillMetrics::LogDialogSecurityMetric(
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
      base::Bind(ServerTypeMatchesField, SECTION_SHIPPING),
      base::Bind(NullGetInfo), std::string(),
      g_browser_process->GetApplicationLocale());

  transaction_amount_ = form_structure_.GetUniqueValue(
      HTML_TYPE_TRANSACTION_AMOUNT);
  transaction_currency_ = form_structure_.GetUniqueValue(
      HTML_TYPE_TRANSACTION_CURRENCY);
  acceptable_cc_types_ = form_structure_.PossibleValues(CREDIT_CARD_TYPE);

  validator_.reset(new AddressValidator(
      scoped_ptr< ::i18n::addressinput::Source>(
          new autofill::ChromeMetadataSource(I18N_ADDRESS_VALIDATION_DATA_URL,
                                             profile_->GetRequestContext())),
      ValidationRulesStorageFactory::CreateStorage(),
      this));

  SuggestionsUpdated();
  SubmitButtonDelayBegin();
  view_.reset(CreateView());
  view_->Show();
  GetManager()->AddObserver(this);

  LogDialogLatencyToShow();
}

void AutofillDialogControllerImpl::Hide() {
  if (view_)
    view_->Hide();
}

// TODO(estade): remove.
void AutofillDialogControllerImpl::TabActivated() {
}

////////////////////////////////////////////////////////////////////////////////
// AutofillDialogViewDelegate implementation.

base::string16 AutofillDialogControllerImpl::DialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

base::string16 AutofillDialogControllerImpl::EditSuggestionText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EDIT);
}

base::string16 AutofillDialogControllerImpl::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

base::string16 AutofillDialogControllerImpl::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

base::string16 AutofillDialogControllerImpl::SaveLocallyText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_CHECKBOX);
}

base::string16 AutofillDialogControllerImpl::SaveLocallyTooltip() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_TOOLTIP);
}

bool AutofillDialogControllerImpl::ShouldOfferToSaveInChrome() const {
  return IsAutofillEnabled() && !profile_->IsOffTheRecord() &&
         IsManuallyEditingAnySection();
}

bool AutofillDialogControllerImpl::ShouldSaveInChrome() const {
  return profile_->GetPrefs()->GetBoolean(::prefs::kAutofillDialogSaveData);
}

int AutofillDialogControllerImpl::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_OK | ui::DIALOG_BUTTON_CANCEL;
}

bool AutofillDialogControllerImpl::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK)
    return !submit_button_delay_timer_.IsRunning();

  DCHECK_EQ(ui::DIALOG_BUTTON_CANCEL, button);
  return true;
}

bool AutofillDialogControllerImpl::SectionIsActive(DialogSection section)
    const {
  return FormStructureCaresAboutSection(section);
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
      GetValidator()->LoadRules(
          CountryNames::GetInstance()->GetCountryCode(it->initial_value));
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

  // Currently, the view should only need to be updated if validating a
  // suggestion that's based on existing data.
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    if (IsEditingExistingData(static_cast<DialogSection>(i))) {
      view_->UpdateForErrors();
      break;
    }
  }
}

const DetailInputs& AutofillDialogControllerImpl::RequestedFieldsForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_CC:
      return requested_cc_fields_;
    case SECTION_BILLING:
      return requested_billing_fields_;
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

base::string16 AutofillDialogControllerImpl::LabelForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_CC:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC);
    case SECTION_BILLING:
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

  if (section == SECTION_BILLING || section == SECTION_SHIPPING) {
    // Also check if the address is invalid (rules may have loaded since
    // the dialog was shown).
    if (HasInvalidAddress(*GetManager()->GetProfileByGUID(item_key)))
      return false;
  }

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  return wrapper->GetDisplayText(vertically_compact, horizontally_compact);
}

base::string16 AutofillDialogControllerImpl::ExtraSuggestionTextForSection(
    DialogSection section) const {
  if (section == SECTION_CC)
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC);

  return base::string16();
}

scoped_ptr<DataModelWrapper> AutofillDialogControllerImpl::CreateWrapper(
    DialogSection section) {
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  if (!IsASuggestionItemKey(item_key) || IsManuallyEditingSection(section))
    return scoped_ptr<DataModelWrapper>();

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
  if (section != SECTION_CC)
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

// TODO(groby): Add more tests.
base::string16 AutofillDialogControllerImpl::InputValidityMessage(
    DialogSection section,
    ServerFieldType type,
    const base::string16& value) {
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
    if (!autofill::IsValidCreditCardExpirationDate(
            field_values[CREDIT_CARD_EXP_4_DIGIT_YEAR],
            field_values[CREDIT_CARD_EXP_MONTH], base::Time::Now())) {
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

  std::vector<autofill::Suggestion> popup_suggestions;
  popup_suggestion_ids_.clear();
  if (IsCreditCardType(type)) {
    popup_suggestions = GetManager()->GetCreditCardSuggestions(
        AutofillType(type), field_contents);
    for (const auto& suggestion : popup_suggestions)
      popup_suggestion_ids_.push_back(suggestion.backend_id);
  } else {
    popup_suggestions = GetManager()->GetProfileSuggestions(
        AutofillType(type),
        field_contents,
        false,
        RequestedTypesForSection(section));
    // Filter out ones we don't want.
    for (int i = 0; i < static_cast<int>(popup_suggestions.size()); i++) {
      const autofill::AutofillProfile* profile =
          GetManager()->GetProfileByGUID(popup_suggestions[i].backend_id);
      if (!profile || !ShouldSuggestProfile(section, *profile)) {
        popup_suggestions.erase(popup_suggestions.begin() + i);
        i--;
      }
    }

    // Save the IDs.
    for (const auto& suggestion : popup_suggestions)
      popup_suggestion_ids_.push_back(suggestion.backend_id);

    // This will append to the popup_suggestions but not the IDs since there
    // are no backend IDs for the I18N validator suggestions.
    GetI18nValidatorSuggestions(section, type, &popup_suggestions);
  }

  if (popup_suggestions.empty()) {
    HidePopup();
    return;
  }

  // |popup_input_type_| must be set before calling |Show()|.
  popup_input_type_ = type;
  popup_section_ = section;

  // Use our own 0-based IDs for the items.
  // TODO(estade): do we need separators and control rows like 'Clear
  // Form'?
  for (size_t i = 0; i < popup_suggestions.size(); ++i) {
    popup_suggestions[i].frontend_id = i;
  }

  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_, weak_ptr_factory_.GetWeakPtr(), NULL, parent_view,
      gfx::RectF(content_bounds),
      base::i18n::IsRTL() ? base::i18n::RIGHT_TO_LEFT
                          : base::i18n::LEFT_TO_RIGHT);
  popup_controller_->Show(popup_suggestions);
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

void AutofillDialogControllerImpl::OnCancel() {
  HidePopup();
  if (!data_was_passed_back_)
    LogOnCancelMetrics();
  callback_.Run(
      AutofillClient::AutocompleteResultErrorCancel, base::string16(), NULL);
}

void AutofillDialogControllerImpl::OnAccept() {
  ScopedViewUpdates updates(view_.get());
  HidePopup();
  FinishSubmit();
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

  AutofillMetrics::LogDialogPopupEvent(AutofillMetrics::DIALOG_POPUP_SHOWN);
}

void AutofillDialogControllerImpl::OnPopupHidden() {}

void AutofillDialogControllerImpl::DidSelectSuggestion(
    const base::string16& value,
    int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::DidAcceptSuggestion(
    const base::string16& value,
    int identifier,
    int position) {
  DCHECK_NE(UNKNOWN_TYPE, popup_input_type_);
  // Because |HidePopup()| can be called from |UpdateSection()|, remember the
  // type of the input for later here.
  const ServerFieldType popup_input_type = popup_input_type_;

  ScopedViewUpdates updates(view_.get());
  scoped_ptr<DataModelWrapper> wrapper;

  if (static_cast<size_t>(identifier) < popup_suggestion_ids_.size()) {
    const std::string& guid = popup_suggestion_ids_[identifier];
    if (IsCreditCardType(popup_input_type)) {
      wrapper.reset(new AutofillCreditCardWrapper(
          GetManager()->GetCreditCardByGUID(guid)));
    } else {
      wrapper.reset(new AutofillProfileWrapper(
          GetManager()->GetProfileByGUID(guid)));
    }
  } else {
    wrapper.reset(new I18nAddressDataWrapper(
        &i18n_validator_suggestions_[
            identifier - popup_suggestion_ids_.size()]));
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

  AutofillMetrics::LogDialogPopupEvent(
      AutofillMetrics::DIALOG_POPUP_FORM_FILLED);

  // TODO(estade): not sure why it's necessary to do this explicitly.
  HidePopup();
}

bool AutofillDialogControllerImpl::GetDeletionConfirmationText(
    const base::string16& value,
    int identifier,
    base::string16* title,
    base::string16* body) {
  return false;
}

bool AutofillDialogControllerImpl::RemoveSuggestion(const base::string16& value,
                                                    int identifier) {
  // TODO(estade): implement.
  return false;
}

void AutofillDialogControllerImpl::ClearPreviewedForm() {
  // TODO(estade): implement.
}

////////////////////////////////////////////////////////////////////////////////
// SuggestionsMenuModelDelegate implementation.

void AutofillDialogControllerImpl::SuggestionItemSelected(
    SuggestionsMenuModel* model,
    size_t index) {
  ScopedViewUpdates updates(view_.get());

  if (model->GetItemKeyAt(index) == kManageItemsKey) {
    GURL url;
    DCHECK(IsAutofillEnabled());
    GURL settings_url(chrome::kChromeUISettingsURL);
    url = settings_url.Resolve(chrome::kAutofillSubPage);
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
// PersonalDataManagerObserver implementation.

void AutofillDialogControllerImpl::OnPersonalDataChanged() {
  SuggestionsUpdated();
}

////////////////////////////////////////////////////////////////////////////////

bool AutofillDialogControllerImpl::HandleKeyPressEventInInput(
    const content::NativeWebKeyboardEvent& event) {
  if (popup_controller_.get())
    return popup_controller_->HandleKeyPressEvent(event);

  return false;
}

void AutofillDialogControllerImpl::ShowNewCreditCardBubble(
    scoped_ptr<CreditCard> new_card,
    scoped_ptr<AutofillProfile> billing_profile) {
  NewCreditCardBubbleController::Show(web_contents(), std::move(new_card),
                                      std::move(billing_profile));
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
      suggested_cc_(this),
      suggested_billing_(this),
      suggested_shipping_(this),
      cares_about_shipping_(true),
      popup_input_type_(UNKNOWN_TYPE),
      popup_section_(SECTION_MIN),
      data_was_passed_back_(false),
      was_ui_latency_logged_(false),
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

void AutofillDialogControllerImpl::OpenTabWithUrl(const GURL& url) {
  chrome::NavigateParams params(
      chrome::FindBrowserWithWebContents(web_contents()),
      url,
      ui::PAGE_TRANSITION_LINK);
  params.disposition = NEW_FOREGROUND_TAB;
  chrome::Navigate(&params);
}

// TODO(estade): remove.
DialogSection AutofillDialogControllerImpl::ActiveBillingSection() const {
  return SECTION_BILLING;
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

void AutofillDialogControllerImpl::SuggestionsUpdated() {
  ScopedViewUpdates updates(view_.get());

  const FieldValueMap snapshot = TakeUserInputSnapshot();

  suggested_cc_.Reset();
  suggested_billing_.Reset();
  suggested_shipping_.Reset();
  HidePopup();

  suggested_shipping_.AddKeyedItem(
      kSameAsBillingKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING));

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
          cards[i]->guid(), cards[i]->Label(),
          rb.GetImageNamed(CreditCard::IconResourceId(cards[i]->type())));
      suggested_cc_.SetEnabled(
          cards[i]->guid(), !ShouldDisallowCcType(cards[i]->TypeForDisplay()));
    }

    const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
    std::vector<base::string16> labels;
    AutofillProfile::CreateDifferentiatingLabels(
        profiles, g_browser_process->GetApplicationLocale(), &labels);
    DCHECK_EQ(labels.size(), profiles.size());
    for (size_t i = 0; i < profiles.size(); ++i) {
      const AutofillProfile& profile = *profiles[i];
      if (!i18ninput::AddressHasCompleteAndVerifiedData(
              profile, g_browser_process->GetApplicationLocale())) {
        continue;
      }

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
      l10n_util::GetStringUTF16(IsAutofillEnabled()
                                    ? IDS_AUTOFILL_DIALOG_ADD_CREDIT_CARD
                                    : IDS_AUTOFILL_DIALOG_ENTER_CREDIT_CARD));
  suggested_cc_.AddKeyedItem(
      kManageItemsKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_CREDIT_CARD));
  suggested_billing_.AddKeyedItem(
      kAddNewItemKey,
      l10n_util::GetStringUTF16(
          IsAutofillEnabled() ? IDS_AUTOFILL_DIALOG_ADD_BILLING_ADDRESS
                              : IDS_AUTOFILL_DIALOG_ENTER_BILLING_DETAILS));
  suggested_billing_.AddKeyedItem(
      kManageItemsKey,
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_BILLING_ADDRESS));

  suggested_shipping_.AddKeyedItem(
      kAddNewItemKey,
      l10n_util::GetStringUTF16(
          IsAutofillEnabled()
              ? IDS_AUTOFILL_DIALOG_ADD_SHIPPING_ADDRESS
              : IDS_AUTOFILL_DIALOG_USE_DIFFERENT_SHIPPING_ADDRESS));

  if (IsAutofillEnabled()) {
    suggested_shipping_.AddKeyedItem(
        kManageItemsKey,
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_MANAGE_SHIPPING_ADDRESS));

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
  std::vector<ServerFieldType> types = TypesFromInputs(inputs);

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  if (wrapper) {
    // Only fill in data that is associated with this section.
    wrapper->FillFormStructure(types, compare, &form_structure_);

    // CVC needs special-casing because the CreditCard class doesn't store or
    // handle them. This isn't necessary when filling the combined CC and
    // billing section as CVC comes from |full_wallet_| in this case.
    if (section == SECTION_CC)
      SetOutputForFieldsOfType(CREDIT_CARD_VERIFICATION_CODE, view_->GetCvc());

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
      section, base::Bind(ServerTypeMatchesField, section));
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
    std::vector<autofill::Suggestion>* popup_suggestions) {
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
    popup_suggestions->push_back(autofill::Suggestion(
        base::UTF8ToUTF16(
            i18n_validator_suggestions_[i].GetFieldValue(focused_field))));

    // Disambiguate the suggestion by showing the smallest administrative
    // region of the suggested address:
    //    ADMIN_AREA > LOCALITY > DEPENDENT_LOCALITY
    for (int field = DEPENDENT_LOCALITY; field >= ADMIN_AREA; --field) {
      const std::string& field_value =
          i18n_validator_suggestions_[i].GetFieldValue(
              static_cast<AddressField>(field));
      if (focused_field != field && !field_value.empty()) {
        popup_suggestions->back().label = base::UTF8ToUTF16(field_value);
        break;
      }
    }
  }
}

DetailInputs* AutofillDialogControllerImpl::MutableRequestedFieldsForSection(
    DialogSection section) {
  return const_cast<DetailInputs*>(&RequestedFieldsForSection(section));
}

std::string* AutofillDialogControllerImpl::MutableAddressLanguageCodeForSection(
    DialogSection section) {
  switch (section) {
    case SECTION_BILLING:
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
  return TypesFromInputs(RequestedFieldsForSection(section));
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

  return CountryNames::GetInstance()->GetCountryCode(country);
}

bool AutofillDialogControllerImpl::RebuildInputsForCountry(
    DialogSection section,
    const base::string16& country_name,
    bool should_clobber) {
  CountryComboboxModel* model = CountryComboboxModelForSection(section);
  if (!model)
    return false;

  std::string country_code =
      CountryNames::GetInstance()->GetCountryCode(country_name);
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
    GetValidator()->LoadRules(
        CountryNames::GetInstance()->GetCountryCode(country_name));
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

  if (ShouldDisallowCcType(
          CreditCard::TypeForDisplay(CreditCard::GetCreditCardType(number)))) {
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

void AutofillDialogControllerImpl::FinishSubmit() {
  FillOutputForSection(SECTION_CC);
  FillOutputForSection(SECTION_BILLING);

  if (ShouldUseBillingForShipping()) {
    FillOutputForSectionWithComparator(
        SECTION_BILLING,
        base::Bind(ServerTypeMatchesShippingField));
    FillOutputForSectionWithComparator(
        SECTION_CC,
        base::Bind(ServerTypeMatchesShippingField));
  } else {
    FillOutputForSection(SECTION_SHIPPING);
  }

  if (ShouldOfferToSaveInChrome()) {
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

  LogOnFinishSubmitMetrics();

  // Callback should be called as late as possible.
  callback_.Run(AutofillClient::AutocompleteResultSuccess,
                base::string16(),
                &form_structure_);
  data_was_passed_back_ = true;

  // This might delete us.
  Hide();
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

    if (IsManuallyEditingSection(section) && needs_validation_.count(section)) {
      view_->ValidateSection(section);
      needs_validation_.erase(section);
    } else if (success) {
      ShowEditUiIfBadSuggestion(section);
      UpdateSection(section);
    }
  }
}

void AutofillDialogControllerImpl::PersistAutofillChoice(
    DialogSection section,
    const std::string& guid) {
  DCHECK(ShouldOfferToSaveInChrome());
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
  DCHECK(IsAutofillEnabled());
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
  DCHECK(IsAutofillEnabled());
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
  AutofillMetrics::LogDialogUiDuration(
      base::Time::Now() - dialog_shown_timestamp_,
      AutofillMetrics::DIALOG_ACCEPTED);

  AutofillMetrics::LogDialogUiEvent(AutofillMetrics::DIALOG_UI_ACCEPTED);

  AutofillMetrics::DialogDismissalState dismissal_state;
  if (!IsManuallyEditingAnySection()) {
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_EXISTING_AUTOFILL_DATA;
  } else if (ShouldSaveDetailsLocally()) {
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_SAVE_TO_AUTOFILL;
  } else {
    dismissal_state = AutofillMetrics::DIALOG_ACCEPTED_NO_SAVE;
  }

  AutofillMetrics::LogDialogDismissalState(dismissal_state);
}

void AutofillDialogControllerImpl::LogOnCancelMetrics() {
  AutofillMetrics::LogDialogUiEvent(AutofillMetrics::DIALOG_UI_CANCELED);

  AutofillMetrics::DialogDismissalState dismissal_state;
  if (!IsManuallyEditingAnySection())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_NO_EDITS;
  else if (AllSectionsAreValid())
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_NO_INVALID_FIELDS;
  else
    dismissal_state = AutofillMetrics::DIALOG_CANCELED_WITH_INVALID_FIELDS;

  AutofillMetrics::LogDialogDismissalState(dismissal_state);

  AutofillMetrics::LogDialogUiDuration(
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

  AutofillMetrics::LogDialogUiEvent(dialog_ui_event);
}

void AutofillDialogControllerImpl::LogDialogLatencyToShow() {
  if (was_ui_latency_logged_)
    return;

  AutofillMetrics::LogDialogLatencyToShow(base::Time::Now() -
                                          dialog_shown_timestamp_);
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

  return has_autofill_profiles
             ? AutofillMetrics::DIALOG_USER_NOT_SIGNED_IN_HAS_AUTOFILL
             : AutofillMetrics::DIALOG_USER_NOT_SIGNED_IN_NO_AUTOFILL;
}

void AutofillDialogControllerImpl::MaybeShowCreditCardBubble() {
  if (!data_was_passed_back_ || !newly_saved_card_)
    return;

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
    std::string item_key = SuggestionsMenuModelForSection(SECTION_BILLING)
                               ->GetItemKeyForCheckedItem();
    AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
    billing_profile.reset(new AutofillProfile(*profile));
  }

  ShowNewCreditCardBubble(std::move(newly_saved_card_),
                          std::move(billing_profile));
}

void AutofillDialogControllerImpl::OnSubmitButtonDelayEnd() {
  if (!view_)
    return;
  ScopedViewUpdates updates(view_.get());
  view_->UpdateButtonStrip();
}

}  // namespace autofill
