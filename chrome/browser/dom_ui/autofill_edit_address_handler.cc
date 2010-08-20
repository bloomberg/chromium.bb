// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/autofill_edit_address_handler.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "grit/generated_resources.h"

AutoFillEditAddressHandler::AutoFillEditAddressHandler() {
}

AutoFillEditAddressHandler::~AutoFillEditAddressHandler() {
}

void AutoFillEditAddressHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
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
  localized_strings->SetString("autoFillEditAddressApplyButton",
      l10n_util::GetStringUTF16(IDS_OK));
  localized_strings->SetString("autoFillEditAddressCancelButton",
      l10n_util::GetStringUTF16(IDS_CANCEL));
}

void AutoFillEditAddressHandler::RegisterMessages() {
}
