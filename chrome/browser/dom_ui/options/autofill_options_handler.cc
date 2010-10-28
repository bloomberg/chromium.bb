// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/options/autofill_options_handler.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/string16.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/guid.h"
#include "chrome/browser/profile.h"
#include "grit/generated_resources.h"

AutoFillOptionsHandler::AutoFillOptionsHandler()
    : personal_data_(NULL) {
}

AutoFillOptionsHandler::~AutoFillOptionsHandler() {
  if (personal_data_)
    personal_data_->RemoveObserver(this);
}

/////////////////////////////////////////////////////////////////////////////
// OptionsUIHandler implementation:
void AutoFillOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("autoFillOptionsTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_OPTIONS_TITLE));
  localized_strings->SetString("autoFillEnabled",
      l10n_util::GetStringUTF16(IDS_OPTIONS_AUTOFILL_ENABLE));
  localized_strings->SetString("addressesHeader",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESSES_GROUP_NAME));
  localized_strings->SetString("creditCardsHeader",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_CREDITCARDS_GROUP_NAME));
  localized_strings->SetString("addAddressButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_ADDRESS_BUTTON));
  localized_strings->SetString("addCreditCardButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_CREDITCARD_BUTTON));
  localized_strings->SetString("editButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_BUTTON));
  localized_strings->SetString("deleteButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DELETE_BUTTON));
  localized_strings->SetString("helpButton",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_HELP_LABEL));
  localized_strings->SetString("addAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_ADDRESS_CAPTION));
  localized_strings->SetString("editAddressTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_ADDRESS_CAPTION));
  localized_strings->SetString("addCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADD_CREDITCARD_CAPTION));
  localized_strings->SetString("editCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));

  SetAddressOverlayStrings(localized_strings);
  SetCreditCardOverlayStrings(localized_strings);
}

void AutoFillOptionsHandler::Initialize() {
  personal_data_ =
      dom_ui_->GetProfile()->GetOriginalProfile()->GetPersonalDataManager();
  personal_data_->SetObserver(this);

  LoadAutoFillData();
}

void AutoFillOptionsHandler::RegisterMessages() {
  dom_ui_->RegisterMessageCallback(
      "updateAddress",
      NewCallback(this, &AutoFillOptionsHandler::UpdateAddress));

  dom_ui_->RegisterMessageCallback(
      "editAddress",
      NewCallback(this, &AutoFillOptionsHandler::EditAddress));

  dom_ui_->RegisterMessageCallback(
      "removeAddress",
      NewCallback(this, &AutoFillOptionsHandler::RemoveAddress));

  dom_ui_->RegisterMessageCallback(
      "updateCreditCard",
      NewCallback(this, &AutoFillOptionsHandler::UpdateCreditCard));

  dom_ui_->RegisterMessageCallback(
      "editCreditCard",
      NewCallback(this, &AutoFillOptionsHandler::EditCreditCard));

  dom_ui_->RegisterMessageCallback(
      "removeCreditCard",
      NewCallback(this, &AutoFillOptionsHandler::RemoveCreditCard));
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
  localized_strings->SetString("stateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_STATE));
  localized_strings->SetString("zipCodeLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ZIP_CODE));
  localized_strings->SetString("countryLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COUNTRY));
  localized_strings->SetString("countryLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_COUNTRY));
  localized_strings->SetString("phoneLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PHONE));
  localized_strings->SetString("faxLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_FAX));
  localized_strings->SetString("emailLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EMAIL));
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
    DictionaryValue* address = new DictionaryValue();
    address->SetString("label", (*i)->Label());
    address->SetString("guid", (*i)->guid());
    addresses.Append(address);
  }

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.updateAddresses",
                                  addresses);

  ListValue credit_cards;
  for (std::vector<CreditCard*>::const_iterator i =
           personal_data_->credit_cards().begin();
       i != personal_data_->credit_cards().end(); ++i) {
    DictionaryValue* credit_card = new DictionaryValue();
    credit_card->SetString("label", (*i)->PreviewSummary());
    credit_card->SetString("guid", (*i)->guid());
    credit_cards.Append(credit_card);
  }

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.updateCreditCards",
                                  credit_cards);
}

void AutoFillOptionsHandler::UpdateAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutoFillProfile profile(guid);

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
  if (args->GetString(8, &value))
    profile.SetInfo(AutoFillType(ADDRESS_HOME_COUNTRY), value);
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

void AutoFillOptionsHandler::EditAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  AutoFillProfile* profile = personal_data_->GetProfileByGUID(guid);
  if (!profile) {
    NOTREACHED();
    return;
  }

  // TODO(jhawkins): This is hacky because we can't send DictionaryValue
  // directly to CallJavascriptFunction().
  ListValue addressList;
  DictionaryValue* address = new DictionaryValue();
  address->SetString("guid", profile->guid());
  address->SetString("fullName",
                     profile->GetFieldText(AutoFillType(NAME_FULL)));
  address->SetString("companyName",
                     profile->GetFieldText(AutoFillType(COMPANY_NAME)));
  address->SetString("addrLine1",
                     profile->GetFieldText(AutoFillType(ADDRESS_HOME_LINE1)));
  address->SetString("addrLine2",
                     profile->GetFieldText(AutoFillType(ADDRESS_HOME_LINE2)));
  address->SetString("city",
                     profile->GetFieldText(AutoFillType(ADDRESS_HOME_CITY)));
  address->SetString("state",
                     profile->GetFieldText(AutoFillType(ADDRESS_HOME_STATE)));
  address->SetString("zipCode",
                     profile->GetFieldText(AutoFillType(ADDRESS_HOME_ZIP)));
  address->SetString("country",
                     profile->GetFieldText(AutoFillType(ADDRESS_HOME_COUNTRY)));
  address->SetString(
      "phone",
      profile->GetFieldText(AutoFillType(PHONE_HOME_WHOLE_NUMBER)));
  address->SetString(
      "fax",
      profile->GetFieldText(AutoFillType(PHONE_FAX_WHOLE_NUMBER)));
  address->SetString("email",
                     profile->GetFieldText(AutoFillType(EMAIL_ADDRESS)));
  addressList.Append(address);

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.editAddress",
                                  addressList);
}

void AutoFillOptionsHandler::RemoveAddress(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveProfile(guid);
}

void AutoFillOptionsHandler::UpdateCreditCard(const ListValue* args) {
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

void AutoFillOptionsHandler::EditCreditCard(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
      return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  CreditCard* credit_card = personal_data_->GetCreditCardByGUID(guid);

  if (!credit_card) {
    NOTREACHED();
    return;
  }

  // TODO(jhawkins): This is hacky because we can't send DictionaryValue
  // directly to CallJavascriptFunction().
  ListValue credit_card_list;
  DictionaryValue* credit_card_data = new DictionaryValue();
  credit_card_data->SetString("guid", credit_card->guid());
  credit_card_data->SetString(
      "nameOnCard",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_NAME)));
  credit_card_data->SetString(
      "creditCardNumber",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)));
  credit_card_data->SetString(
      "expirationMonth",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_EXP_MONTH)));
  credit_card_data->SetString(
      "expirationYear",
      credit_card->GetFieldText(AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
  credit_card_list.Append(credit_card_data);

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.editCreditCard",
                                  credit_card_list);
}

void AutoFillOptionsHandler::RemoveCreditCard(const ListValue* args) {
  if (!personal_data_->IsDataLoaded())
    return;

  std::string guid;
  if (!args->GetString(0, &guid)) {
    NOTREACHED();
    return;
  }

  personal_data_->RemoveCreditCard(guid);
}
