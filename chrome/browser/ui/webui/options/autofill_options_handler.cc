// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/autofill_options_handler.h"

#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/country_combobox_model.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "content/public/browser/web_ui.h"
#include "third_party/libaddressinput/messages.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

using autofill::AutofillCountry;
using autofill::AutofillType;
using autofill::ServerFieldType;
using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::PersonalDataManager;
using i18n::addressinput::AddressUiComponent;

namespace {

const char kSettingsOrigin[] = "Chrome settings";

static const char kFullNameField[] = "fullName";
static const char kCompanyNameField[] = "companyName";
static const char kAddressLineField[] = "addrLines";
static const char kDependentLocalityField[] = "dependentLocality";
static const char kCityField[] = "city";
static const char kStateField[] = "state";
static const char kPostalCodeField[] = "postalCode";
static const char kSortingCodeField[] = "sortingCode";
static const char kCountryField[] = "country";

static const char kComponents[] = "components";
static const char kLanguageCode[] = "languageCode";

// Fills |components| with the address UI components that should be used to
// input an address for |country_code| when UI BCP 47 language code is
// |ui_language_code|. If |components_language_code| is not NULL, then sets it
// to the BCP 47 language code that should be used to format the address for
// display.
void GetAddressComponents(const std::string& country_code,
                          const std::string& ui_language_code,
                          base::ListValue* address_components,
                          std::string* components_language_code) {
  DCHECK(address_components);

  i18n::addressinput::Localization localization;
  localization.SetGetter(l10n_util::GetStringUTF8);
  std::string not_used;
  std::vector<AddressUiComponent> components =
      i18n::addressinput::BuildComponents(
          country_code,
          localization,
          ui_language_code,
          components_language_code == NULL ?
              &not_used : components_language_code);
  if (components.empty()) {
    static const char kDefaultCountryCode[] = "US";
    components = i18n::addressinput::BuildComponents(
        kDefaultCountryCode,
        localization,
        ui_language_code,
        components_language_code == NULL ?
            &not_used : components_language_code);
  }
  DCHECK(!components.empty());

  base::ListValue* line = NULL;
  static const char kField[] = "field";
  static const char kLength[] = "length";
  for (size_t i = 0; i < components.size(); ++i) {
    if (i == 0 ||
        components[i - 1].length_hint == AddressUiComponent::HINT_LONG ||
        components[i].length_hint == AddressUiComponent::HINT_LONG) {
      line = new base::ListValue;
      address_components->Append(line);
    }

    scoped_ptr<base::DictionaryValue> component(new base::DictionaryValue);
    component->SetString("name", components[i].name);

    switch (components[i].field) {
      case i18n::addressinput::COUNTRY:
        component->SetString(kField, kCountryField);
        break;
      case i18n::addressinput::ADMIN_AREA:
        component->SetString(kField, kStateField);
        break;
      case i18n::addressinput::LOCALITY:
        component->SetString(kField, kCityField);
        break;
      case i18n::addressinput::DEPENDENT_LOCALITY:
        component->SetString(kField, kDependentLocalityField);
        break;
      case i18n::addressinput::SORTING_CODE:
        component->SetString(kField, kSortingCodeField);
        break;
      case i18n::addressinput::POSTAL_CODE:
        component->SetString(kField, kPostalCodeField);
        break;
      case i18n::addressinput::STREET_ADDRESS:
        component->SetString(kField, kAddressLineField);
        break;
      case i18n::addressinput::ORGANIZATION:
        component->SetString(kField, kCompanyNameField);
        break;
      case i18n::addressinput::RECIPIENT:
        component->SetString(kField, kFullNameField);
        component->SetString(
            "placeholder",
            l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_ADD_NAME));
        break;
    }

    switch (components[i].length_hint) {
      case AddressUiComponent::HINT_LONG:
        component->SetString(kLength, "long");
        break;
      case AddressUiComponent::HINT_SHORT:
        component->SetString(kLength, "short");
        break;
    }

    line->Append(component.release());
  }
}

// Sets data related to the country <select>.
void SetCountryData(const PersonalDataManager& manager,
                    base::DictionaryValue* localized_strings) {
  autofill::CountryComboboxModel model;
  model.SetCountries(manager, base::Callback<bool(const std::string&)>());
  const std::vector<AutofillCountry*>& countries = model.countries();
  localized_strings->SetString("defaultCountryCode",
                               countries.front()->country_code());

  // An ordered list of options to show in the <select>.
  scoped_ptr<base::ListValue> country_list(new base::ListValue());
  for (size_t i = 0; i < countries.size(); ++i) {
    scoped_ptr<base::DictionaryValue> option_details(
        new base::DictionaryValue());
    option_details->SetString("name", model.GetItemAt(i));
    option_details->SetString(
        "value",
        countries[i] ? countries[i]->country_code() : "separator");
    country_list->Append(option_details.release());
  }
  localized_strings->Set("autofillCountrySelectList", country_list.release());

  scoped_ptr<base::ListValue> default_country_components(new base::ListValue);
  std::string default_country_language_code;
  GetAddressComponents(countries.front()->country_code(),
                       g_browser_process->GetApplicationLocale(),
                       default_country_components.get(),
                       &default_country_language_code);
  localized_strings->Set("autofillDefaultCountryComponents",
                         default_country_components.release());
  localized_strings->SetString("autofillDefaultCountryLanguageCode",
                               default_country_language_code);
}

// Get the multi-valued element for |type| and return it in |ListValue| form.
// Buyer beware: the type of data affects whether GetRawInfo or GetInfo is used.
void GetValueList(const AutofillProfile& profile,
                  ServerFieldType type,
                  scoped_ptr<base::ListValue>* list) {
  list->reset(new base::ListValue);

  std::vector<base::string16> values;
  if (AutofillType(type).group() == autofill::NAME) {
    profile.GetMultiInfo(
        AutofillType(type), g_browser_process->GetApplicationLocale(), &values);
  } else {
    profile.GetRawMultiInfo(type, &values);
  }

  // |Get[Raw]MultiInfo()| always returns at least one, potentially empty, item.
  if (values.size() == 1 && values.front().empty())
    return;

  for (size_t i = 0; i < values.size(); ++i) {
    (*list)->Set(i, new base::StringValue(values[i]));
  }
}

// Converts a ListValue of StringValues to a vector of string16s.
void ListValueToStringVector(const base::ListValue& list,
                             std::vector<base::string16>* output) {
  output->resize(list.GetSize());
  for (size_t i = 0; i < list.GetSize(); ++i) {
    base::string16 value;
    if (list.GetString(i, &value))
      (*output)[i].swap(value);
  }
}

// Pulls the phone number |index|, |phone_number_list|, and |country_code| from
// the |args| input.
void ExtractPhoneNumberInformation(const base::ListValue* args,
                                   size_t* index,
                                   const base::ListValue** phone_number_list,
                                   std::string* country_code) {
  // Retrieve index as a |double|, as that is how it comes across from
  // JavaScript.
  double number = 0.0;
  if (!args->GetDouble(0, &number)) {
    NOTREACHED();
    return;
  }
  *index = number;

  if (!args->GetList(1, phone_number_list)) {
    NOTREACHED();
    return;
  }

  if (!args->GetString(2, country_code)) {
    NOTREACHED();
    return;
  }
}

// Searches the |list| for the value at |index|.  If this value is present
// in any of the rest of the list, then the item (at |index|) is removed.
// The comparison of phone number values is done on normalized versions of the
// phone number values.
void RemoveDuplicatePhoneNumberAtIndex(size_t index,
                                       const std::string& country_code,
                                       base::ListValue* list) {
  base::string16 new_value;
  if (!list->GetString(index, &new_value)) {
    NOTREACHED() << "List should have a value at index " << index;
    return;
  }

  bool is_duplicate = false;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < list->GetSize() && !is_duplicate; ++i) {
    if (i == index)
      continue;

    base::string16 existing_value;
    if (!list->GetString(i, &existing_value)) {
      NOTREACHED() << "List should have a value at index " << i;
      continue;
    }
    is_duplicate = autofill::i18n::PhoneNumbersMatch(
        new_value, existing_value, country_code, app_locale);
  }

  if (is_duplicate)
    list->Remove(index, NULL);
}

scoped_ptr<base::ListValue> ValidatePhoneArguments(
    const base::ListValue* args) {
  size_t index = 0;
  std::string country_code;
  const base::ListValue* extracted_list = NULL;
  ExtractPhoneNumberInformation(args, &index, &extracted_list, &country_code);

  scoped_ptr<base::ListValue> list(extracted_list->DeepCopy());
  RemoveDuplicatePhoneNumberAtIndex(index, country_code, list.get());
  return list.Pass();
}

}  // namespace

namespace options {

AutofillOptionsHandler::AutofillOptionsHandler()
    : personal_data_(NULL) {}

AutofillOptionsHandler::~AutofillOptionsHandler() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// OptionsPageUIHandler implementation:
void AutofillOptionsHandler::GetLocalizedValues(
    base::DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "autofillAddresses", IDS_AUTOFILL_ADDRESSES_GROUP_NAME },
    { "autofillCreditCards", IDS_AUTOFILL_CREDITCARDS_GROUP_NAME },
    { "autofillAddAddress", IDS_AUTOFILL_ADD_ADDRESS_BUTTON },
    { "autofillAddCreditCard", IDS_AUTOFILL_ADD_CREDITCARD_BUTTON },
    { "autofillEditProfileButton", IDS_AUTOFILL_EDIT_PROFILE_BUTTON },
    { "helpButton", IDS_AUTOFILL_HELP_LABEL },
    { "addAddressTitle", IDS_AUTOFILL_ADD_ADDRESS_CAPTION },
    { "editAddressTitle", IDS_AUTOFILL_EDIT_ADDRESS_CAPTION },
    { "addCreditCardTitle", IDS_AUTOFILL_ADD_CREDITCARD_CAPTION },
    { "editCreditCardTitle", IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION },
#if defined(OS_MACOSX)
    { "auxiliaryProfilesEnabled", IDS_AUTOFILL_USE_MAC_ADDRESS_BOOK },
#endif  // defined(OS_MACOSX)
  };

  RegisterStrings(localized_strings, resources, arraysize(resources));
  RegisterTitle(localized_strings, "autofillOptionsPage",
                IDS_AUTOFILL_OPTIONS_TITLE);

  localized_strings->SetString("helpUrl", autofill::kHelpURL);
  SetAddressOverlayStrings(localized_strings);
  SetCreditCardOverlayStrings(localized_strings);
}

void AutofillOptionsHandler::InitializeHandler() {
  // personal_data_ is NULL in guest mode on Chrome OS.
  if (personal_data_)
    personal_data_->AddObserver(this);
}

void AutofillOptionsHandler::InitializePage() {
  if (personal_data_)
    LoadAutofillData();
}

void AutofillOptionsHandler::RegisterMessages() {
  personal_data_ = autofill::PersonalDataManagerFactory::GetForProfile(
      Profile::FromWebUI(web_ui()));

#if defined(OS_MACOSX) && !defined(OS_IOS)
  web_ui()->RegisterMessageCallback(
      "accessAddressBook",
      base::Bind(&AutofillOptionsHandler::AccessAddressBook,
                 base::Unretained(this)));
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)
  web_ui()->RegisterMessageCallback(
      "removeData",
      base::Bind(&AutofillOptionsHandler::RemoveData,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadAddressEditor",
      base::Bind(&AutofillOptionsHandler::LoadAddressEditor,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadAddressEditorComponents",
      base::Bind(&AutofillOptionsHandler::LoadAddressEditorComponents,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "loadCreditCardEditor",
      base::Bind(&AutofillOptionsHandler::LoadCreditCardEditor,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setAddress",
      base::Bind(&AutofillOptionsHandler::SetAddress, base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setCreditCard",
      base::Bind(&AutofillOptionsHandler::SetCreditCard,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "validatePhoneNumbers",
      base::Bind(&AutofillOptionsHandler::ValidatePhoneNumbers,
                 base::Unretained(this)));
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManagerObserver implementation:
void AutofillOptionsHandler::OnPersonalDataChanged() {
  LoadAutofillData();
}

void AutofillOptionsHandler::SetAddressOverlayStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("autofillEditAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION));
  localized_strings->SetString("autofillCountryLabel",
      l10n_util::GetStringUTF16(IDS_LIBADDRESSINPUT_COUNTRY_OR_REGION_LABEL));
  localized_strings->SetString("autofillPhoneLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_PHONE));
  localized_strings->SetString("autofillEmailLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EMAIL));
  localized_strings->SetString("autofillAddPhonePlaceholder",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_ADD_PHONE));
  localized_strings->SetString("autofillAddEmailPlaceholder",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_ADD_EMAIL));
  SetCountryData(*personal_data_, localized_strings);
}

void AutofillOptionsHandler::SetCreditCardOverlayStrings(
    base::DictionaryValue* localized_strings) {
  localized_strings->SetString("autofillEditCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));
  localized_strings->SetString("nameOnCardLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_NAME_ON_CARD));
  localized_strings->SetString("creditCardNumberLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_CREDIT_CARD_NUMBER));
  localized_strings->SetString("creditCardExpirationDateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_DATE));
}

void AutofillOptionsHandler::LoadAutofillData() {
  if (!IsPersonalDataLoaded())
    return;

  const std::vector<AutofillProfile*>& profiles =
      personal_data_->web_profiles();
  std::vector<base::string16> labels;
  AutofillProfile::CreateDifferentiatingLabels(
      profiles,
      g_browser_process->GetApplicationLocale(),
      &labels);
  DCHECK_EQ(labels.size(), profiles.size());

  base::ListValue addresses;
  for (size_t i = 0; i < profiles.size(); ++i) {
    base::ListValue* entry = new base::ListValue();
    entry->Append(new base::StringValue(profiles[i]->guid()));
    entry->Append(new base::StringValue(labels[i]));
    addresses.Append(entry);
  }

  web_ui()->CallJavascriptFunction("AutofillOptions.setAddressList", addresses);

  base::ListValue credit_cards;
  const std::vector<CreditCard*>& cards = personal_data_->GetCreditCards();
  for (std::vector<CreditCard*>::const_iterator iter = cards.begin();
       iter != cards.end(); ++iter) {
    const CreditCard* card = *iter;
    // TODO(estade): this should be a dictionary.
    base::ListValue* entry = new base::ListValue();
    entry->Append(new base::StringValue(card->guid()));
    entry->Append(new base::StringValue(card->Label()));
    entry->Append(new base::StringValue(
        webui::GetBitmapDataUrlFromResource(
            CreditCard::IconResourceId(card->type()))));
    entry->Append(new base::StringValue(card->TypeForDisplay()));
    credit_cards.Append(entry);
  }

  web_ui()->CallJavascriptFunction("AutofillOptions.setCreditCardList",
                                   credit_cards);
}

#if defined(OS_MACOSX) && !defined(OS_IOS)
void AutofillOptionsHandler::AccessAddressBook(const base::ListValue* args) {
  personal_data_->AccessAddressBook();
}
#endif  // defined(OS_MACOSX) && !defined(OS_IOS)

void AutofillOptionsHandler::RemoveData(const base::ListValue* args) {
  DCHECK(IsPersonalDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveByGUID(guid);
}

void AutofillOptionsHandler::LoadAddressEditor(const base::ListValue* args) {
  DCHECK(IsPersonalDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutofillProfile* profile = personal_data_->GetProfileByGUID(guid);
  if (!profile) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  base::DictionaryValue address;
  AutofillProfileToDictionary(*profile, &address);

  web_ui()->CallJavascriptFunction("AutofillOptions.editAddress", address);
}

void AutofillOptionsHandler::LoadAddressEditorComponents(
    const base::ListValue* args) {
  std::string country_code;
  if (!args->GetString(0, &country_code)) {
    NOTREACHED();
    return;
  }

  base::DictionaryValue input;
  scoped_ptr<base::ListValue> components(new base::ListValue);
  std::string language_code;
  GetAddressComponents(country_code, g_browser_process->GetApplicationLocale(),
                       components.get(), &language_code);
  input.Set(kComponents, components.release());
  input.SetString(kLanguageCode, language_code);

  web_ui()->CallJavascriptFunction(
      "AutofillEditAddressOverlay.loadAddressComponents", input);
}

void AutofillOptionsHandler::LoadCreditCardEditor(const base::ListValue* args) {
  DCHECK(IsPersonalDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard* credit_card = personal_data_->GetCreditCardByGUID(guid);
  if (!credit_card) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  base::DictionaryValue credit_card_data;
  credit_card_data.SetString("guid", credit_card->guid());
  credit_card_data.SetString(
      "nameOnCard",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_NAME));
  credit_card_data.SetString(
      "creditCardNumber",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_NUMBER));
  credit_card_data.SetString(
      "expirationMonth",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH));
  credit_card_data.SetString(
      "expirationYear",
      credit_card->GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));

  web_ui()->CallJavascriptFunction("AutofillOptions.editCreditCard",
                                   credit_card_data);
}

void AutofillOptionsHandler::SetAddress(const base::ListValue* args) {
  if (!IsPersonalDataLoaded())
    return;

  int arg_counter = 0;
  std::string guid;
  if (!args->GetString(arg_counter++, &guid)) {
    NOTREACHED();
    return;
  }

  AutofillProfile profile(guid, kSettingsOrigin);

  base::string16 value;
  const base::ListValue* list_value;
  if (args->GetList(arg_counter++, &list_value)) {
    std::vector<base::string16> values;
    ListValueToStringVector(*list_value, &values);
    profile.SetMultiInfo(AutofillType(autofill::NAME_FULL),
                         values,
                         g_browser_process->GetApplicationLocale());
  }

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::COMPANY_NAME, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_CITY, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_STATE, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_ZIP, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE, value);

  if (args->GetString(arg_counter++, &value))
    profile.SetRawInfo(autofill::ADDRESS_HOME_COUNTRY, value);

  if (args->GetList(arg_counter++, &list_value)) {
    std::vector<base::string16> values;
    ListValueToStringVector(*list_value, &values);
    profile.SetRawMultiInfo(autofill::PHONE_HOME_WHOLE_NUMBER, values);
  }

  if (args->GetList(arg_counter++, &list_value)) {
    std::vector<base::string16> values;
    ListValueToStringVector(*list_value, &values);
    profile.SetRawMultiInfo(autofill::EMAIL_ADDRESS, values);
  }

  if (args->GetString(arg_counter++, &value))
    profile.set_language_code(base::UTF16ToUTF8(value));

  if (!base::IsValidGUID(profile.guid())) {
    profile.set_guid(base::GenerateGUID());
    personal_data_->AddProfile(profile);
  } else {
    personal_data_->UpdateProfile(profile);
  }
}

void AutofillOptionsHandler::SetCreditCard(const base::ListValue* args) {
  if (!IsPersonalDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard credit_card(guid, kSettingsOrigin);

  base::string16 value;
  if (args->GetString(1, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME, value);

  if (args->GetString(2, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER, value);

  if (args->GetString(3, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH, value);

  if (args->GetString(4, &value))
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR, value);

  if (!base::IsValidGUID(credit_card.guid())) {
    credit_card.set_guid(base::GenerateGUID());
    personal_data_->AddCreditCard(credit_card);
  } else {
    personal_data_->UpdateCreditCard(credit_card);
  }
}

void AutofillOptionsHandler::ValidatePhoneNumbers(const base::ListValue* args) {
  if (!IsPersonalDataLoaded())
    return;

  scoped_ptr<base::ListValue> list_value = ValidatePhoneArguments(args);

  web_ui()->CallJavascriptFunction(
    "AutofillEditAddressOverlay.setValidatedPhoneNumbers", *list_value);
}

bool AutofillOptionsHandler::IsPersonalDataLoaded() const {
  return personal_data_ && personal_data_->IsDataLoaded();
}

// static
void AutofillOptionsHandler::AutofillProfileToDictionary(
    const autofill::AutofillProfile& profile,
    base::DictionaryValue* address) {
  address->SetString("guid", profile.guid());
  scoped_ptr<base::ListValue> list;
  GetValueList(profile, autofill::NAME_FULL, &list);
  address->Set(kFullNameField, list.release());
  address->SetString(kCompanyNameField,
                     profile.GetRawInfo(autofill::COMPANY_NAME));
  address->SetString(kAddressLineField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS));
  address->SetString(kCityField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_CITY));
  address->SetString(kStateField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_STATE));
  address->SetString(
      kDependentLocalityField,
      profile.GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY));
  address->SetString(kSortingCodeField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE));
  address->SetString(kPostalCodeField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_ZIP));
  address->SetString(kCountryField,
                     profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  GetValueList(profile, autofill::PHONE_HOME_WHOLE_NUMBER, &list);
  address->Set("phone", list.release());
  GetValueList(profile, autofill::EMAIL_ADDRESS, &list);
  address->Set("email", list.release());
  address->SetString(kLanguageCode, profile.language_code());

  scoped_ptr<base::ListValue> components(new base::ListValue);
  GetAddressComponents(
      base::UTF16ToUTF8(profile.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY)),
      profile.language_code(),
      components.get(),
      NULL);
  address->Set(kComponents, components.release());
}

}  // namespace options
