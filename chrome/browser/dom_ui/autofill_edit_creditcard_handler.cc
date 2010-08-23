// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/autofill_edit_creditcard_handler.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "grit/generated_resources.h"

AutoFillEditCreditCardHandler::AutoFillEditCreditCardHandler() {
}

AutoFillEditCreditCardHandler::~AutoFillEditCreditCardHandler() {
}

void AutoFillEditCreditCardHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString("autoFillEditCreditCardTitle",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_EDIT_CREDITCARD_CAPTION));
  localized_strings->SetString("nameOnCardLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_NAME_ON_CARD));
  localized_strings->SetString("billingAddressLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_BILLING_ADDRESS));
  localized_strings->SetString("chooseExistingAddress",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CHOOSE_EXISTING_ADDRESS));
  localized_strings->SetString("creditCardNumberLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_CREDIT_CARD_NUMBER));
  localized_strings->SetString("creditCardExpirationDateLabel",
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EXPIRATION_DATE));
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
  localized_strings->SetString("autoFillEditCreditCardApplyButton",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("autoFillEditCreditCardCancelButton",
      l10n_util::GetStringUTF16(IDS_CANCEL));
}
