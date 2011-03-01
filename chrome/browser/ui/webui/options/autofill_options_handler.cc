// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/autofill_options_handler.h"

#include <vector>

#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/guid.h"
#include "content/browser/webui/web_ui_util.h"
#include "grit/generated_resources.h"
#include "grit/webkit_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Converts a credit card type to the appropriate resource ID of the CC icon.
int CreditCardTypeToResourceID(const string16& type16) {
  std::string type = UTF16ToUTF8(type16);
  if (type == kAmericanExpressCard)
    return IDR_AUTOFILL_CC_AMEX;
  else if (type == kDinersCard)
    return IDR_AUTOFILL_CC_DINERS;
  else if (type == kDiscoverCard)
    return IDR_AUTOFILL_CC_DISCOVER;
  else if (type == kGenericCard)
    return IDR_AUTOFILL_CC_GENERIC;
  else if (type == kJCBCard)
    return IDR_AUTOFILL_CC_JCB;
  else if (type == kMasterCard)
    return IDR_AUTOFILL_CC_MASTERCARD;
  else if (type == kSoloCard)
    return IDR_AUTOFILL_CC_SOLO;
  else if (type == kVisaCard)
    return IDR_AUTOFILL_CC_VISA;

  NOTREACHED();
  return 0;
}

// Returns a dictionary that maps country codes to data for the country.
DictionaryValue* GetCountryData() {
  std::string app_locale = AutofillCountry::ApplicationLocale();
  std::vector<std::string> country_codes;
  AutofillCountry::GetAvailableCountries(&country_codes);

  DictionaryValue* country_data = new DictionaryValue();
  for (size_t i = 0; i < country_codes.size(); ++i) {
    const AutofillCountry country(country_codes[i], app_locale);

    DictionaryValue* details = new DictionaryValue();
    details->SetString("name", country.name());
    details->SetString("postalCodeLabel", country.postal_code_label());
    details->SetString("stateLabel", country.state_label());

    country_data->Set(country.country_code(), details);
  }

  return country_data;
}

}  // namespace

AutoFillOptionsHandler::AutoFillOptionsHandler()
    : personal_data_(NULL) {
}

AutoFillOptionsHandler::~AutoFillOptionsHandler() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// OptionsPageUIHandler implementation:
void AutoFillOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  static OptionsStringResource resources[] = {
    { "autoFillAddresses", IDS_AUTOFILL_ADDRESSES_GROUP_NAME },
    { "autoFillCreditCards", IDS_AUTOFILL_CREDITCARDS_GROUP_NAME },
    { "autoFillAddAddress", IDS_AUTOFILL_ADD_ADDRESS_BUTTON },
    { "autoFillAddCreditCard", IDS_AUTOFILL_ADD_CREDITCARD_BUTTON },
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
  RegisterTitle(localized_strings, "autoFillOptionsPage",
                IDS_AUTOFILL_OPTIONS_TITLE);

  SetAddressOverlayStrings(localized_strings);
  SetCreditCardOverlayStrings(localized_strings);
}

void AutoFillOptionsHandler::Initialize() {
  personal_data_ = web_ui_->GetProfile()->GetPersonalDataManager();
  personal_data_->SetObserver(this);

  LoadAutoFillData();
}

void AutoFillOptionsHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback(
      "removeAddress",
      NewCallback(this, &AutoFillOptionsHandler::RemoveAddress));
  web_ui_->RegisterMessageCallback(
      "removeCreditCard",
      NewCallback(this, &AutoFillOptionsHandler::RemoveCreditCard));
  web_ui_->RegisterMessageCallback(
      "loadAddressEditor",
      NewCallback(this, &AutoFillOptionsHandler::LoadAddressEditor));
  web_ui_->RegisterMessageCallback(
      "loadCreditCardEditor",
      NewCallback(this, &AutoFillOptionsHandler::LoadCreditCardEditor));
  web_ui_->RegisterMessageCallback(
      "setAddress",
      NewCallback(this, &AutoFillOptionsHandler::SetAddress));
  web_ui_->RegisterMessageCallback(
      "setCreditCard",
      NewCallback(this, &AutoFillOptionsHandler::SetCreditCard));
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager::Observer implementation:
void  AutoFillOptionsHandler::OnPersonalDataLoaded() {
  LoadAutoFillData();
}

void AutoFillOptionsHandler::OnPersonalDataChanged() {
  LoadAutoFillData();
}

void AutoFillOptionsHandler::SetAddressOverlayStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("autoFillEditAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION));
  localized_strings->SetString("fullNameLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FULL_NAME));
  localized_strings->SetString("companyNameLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COMPANY_NAME));
  localized_strings->SetString("addrLine1Label",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_1));
  localized_strings->SetString("addrLine2Label",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADDRESS_LINE_2));
  localized_strings->SetString("cityLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CITY));
  localized_strings->SetString("countryLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COUNTRY));
  localized_strings->SetString("phoneLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PHONE));
  localized_strings->SetString("faxLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FAX));
  localized_strings->SetString("emailLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EMAIL));

  std::string app_locale = AutofillCountry::ApplicationLocale();
  std::string default_country_code =
      AutofillCountry::CountryCodeForLocale(app_locale);
  localized_strings->SetString("defaultCountryCode", default_country_code);
  localized_strings->Set("autofillCountryData", GetCountryData());
}

void AutoFillOptionsHandler::SetCreditCardOverlayStrings(
    DictionaryValue* localized_strings) {
  localized_strings->SetString("autoFillEditCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));
  localized_strings->SetString("nameOnCardLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  localized_strings->SetString("creditCardNumberLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  localized_strings->SetString("creditCardExpirationDateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE));
}

void AutoFillOptionsHandler::LoadAutoFillData() {
  if (!personal_data_->IsDataLoaded())
    return;

  ListValue addresses;
  for (std::vector<AutoFillProfile*>::const_iterator i =
           personal_data_->web_profiles().begin();
       i != personal_data_->web_profiles().end(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue((*i)->guid()));
    entry->Append(new StringValue((*i)->Label()));
    addresses.Append(entry);
  }

  web_ui_->CallJavascriptFunction(L"AutoFillOptions.setAddressList",
                                  addresses);

  ListValue credit_cards;
  for (std::vector<CreditCard*>::const_iterator i =
           personal_data_->credit_cards().begin();
       i != personal_data_->credit_cards().end(); ++i) {
    ListValue* entry = new ListValue();
    entry->Append(new StringValue((*i)->guid()));
    entry->Append(new StringValue((*i)->PreviewSummary()));
    int res = CreditCardTypeToResourceID((*i)->type());
    entry->Append(
        new StringValue(web_ui_util::GetImageDataUrlFromResource(res)));
    credit_cards.Append(entry);
  }

  web_ui_->CallJavascriptFunction(L"AutoFillOptions.setCreditCardList",
                                  credit_cards);
}

void AutoFillOptionsHandler::RemoveAddress(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveProfile(guid);
}

void AutoFillOptionsHandler::RemoveCreditCard(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveCreditCard(guid);
}

void AutoFillOptionsHandler::LoadAddressEditor(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutoFillProfile* profile = personal_data_->GetProfileByGUID(guid);
  if (!profile) {
    // There is a race where a user can click once on the close button and
    // quickly click again on the list item before the item is removed (since
    // the list is not updated until the model tells the list an item has been
    // removed). This will activate the editor for a profile that has been
    // removed. Do nothing in that case.
    return;
  }

  DictionaryValue address;
  address.SetString("guid", profile->guid());
  address.SetString("fullName",
                    profile->GetFieldText(AutoFillType(NAME_FULL)));
  address.SetString("companyName",
                    profile->GetFieldText(AutoFillType(COMPANY_NAME)));
  address.SetString("addrLine1",
                    profile->GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)));
  address.SetString("addrLine2",
                    profile->GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)));
  address.SetString("city",
                    profile->GetFieldText(AutoFillType(ADDRESS_HOME_CITY)));
  address.SetString("state",
                    profile->GetFieldText(AutoFillType(ADDRESS_HOME_STATE)));
  address.SetString("postalCode",
                    profile->GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)));
  address.SetString("country",
                    profile->CountryCode());
  address.SetString(
      "phone",
      profile->GetFieldText(AutoFillType(PHONE_HOME_WHOLE_NUMBER)));
  address.SetString(
      "fax",
      profile->GetFieldText(AutoFillType(PHONE_FAX_WHOLE_NUMBER)));
  address.SetString("email",
                     profile->GetFieldText(AutoFillType(EMAIL_ADDRESS)));

  web_ui_->CallJavascriptFunction(L"AutoFillOptions.editAddress", address);
}

void AutoFillOptionsHandler::LoadCreditCardEditor(const ListValue* args) {
  DCHECK(personal_data_->IsDataLoaded());

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

  DictionaryValue credit_card_data;
  credit_card_data.SetString("guid", credit_card->guid());
  credit_card_data.SetString(
      "nameOnCard",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_NAME)));
  credit_card_data.SetString(
      "creditCardNumber",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)));
  credit_card_data.SetString("obfuscatedCardNumber",
                             credit_card->ObfuscatedNumber());
  credit_card_data.SetString(
      "expirationMonth",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)));
  credit_card_data.SetString(
      "expirationYear",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)));

  web_ui_->CallJavascriptFunction(L"AutoFillOptions.editCreditCard",
                                  credit_card_data);
}

void AutoFillOptionsHandler::SetAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutoFillProfile profile(guid);

  std::string country_code;
  string16 value;
  if (args->GetString(1, &value))
    profile.SetInfo(AutoFillType(NAME_FULL), value);
  if (args->GetString(2, &value))
    profile.SetInfo(AutoFillType(COMPANY_NAME), value);
  if (args->GetString(3, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE1), value);
  if (args->GetString(4, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_LINE2), value);
  if (args->GetString(5, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_CITY), value);
  if (args->GetString(6, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_STATE), value);
  if (args->GetString(7, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_ZIP), value);
  if (args->GetString(8, &country_code))
    profile.SetCountryCode(country_code);
  if (args->GetString(9, &value))
    profile.SetInfo(AutoFillType(PHONE_HOME_WHOLE_NUMBER), value);
  if (args->GetString(10, &value))
    profile.SetInfo(AutoFillType(PHONE_FAX_WHOLE_NUMBER), value);
  if (args->GetString(11, &value))
    profile.SetInfo(AutoFillType(EMAIL_ADDRESS), value);

  if (!guid::IsValidGUID(profile.guid())) {
    profile.set_guid(guid::GenerateGUID());
    personal_data_->AddProfile(profile);
  } else {
    personal_data_->UpdateProfile(profile);
  }
}

void AutoFillOptionsHandler::SetCreditCard(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard credit_card(guid);

  string16 value;
  if (args->GetString(1, &value))
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_NAME), value);
  if (args->GetString(2, &value))
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_NUMBER), value);
  if (args->GetString(3, &value))
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_MONTH), value);
  if (args->GetString(4, &value))
    credit_card.SetInfo(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR), value);

  if (!guid::IsValidGUID(credit_card.guid())) {
    credit_card.set_guid(guid::GenerateGUID());
    personal_data_->AddCreditCard(credit_card);
  } else {
    personal_data_->UpdateCreditCard(credit_card);
  }
}
