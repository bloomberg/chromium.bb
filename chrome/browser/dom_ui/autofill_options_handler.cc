// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/autofill_options_handler.h"

#include <vector>

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
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
}

void AutoFillOptionsHandler::Initialize() {
  personal_data_ = dom_ui_->GetProfile()->GetPersonalDataManager();
  personal_data_->SetObserver(this);

  LoadAutoFillData();
}

/////////////////////////////////////////////////////////////////////////////
// PersonalDataManager::Observer implementation:
void  AutoFillOptionsHandler::OnPersonalDataLoaded() {
  LoadAutoFillData();
}

void AutoFillOptionsHandler::OnPersonalDataChanged() {
  LoadAutoFillData();
}

void AutoFillOptionsHandler::RegisterMessages() {
}

void AutoFillOptionsHandler::LoadAutoFillData() {
  if (!personal_data_->IsDataLoaded())
    return;

  ListValue addresses;
  for (std::vector<AutoFillProfile*>::const_iterator i =
           personal_data_->profiles().begin();
       i != personal_data_->profiles().end(); ++i) {
    DictionaryValue* address = new DictionaryValue();
    address->SetString("label", (*i)->PreviewSummary());
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
    credit_cards.Append(credit_card);
  }

  dom_ui_->CallJavascriptFunction(L"AutoFillOptions.updateCreditCards",
                                  credit_cards);
}
