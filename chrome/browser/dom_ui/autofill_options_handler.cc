// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/autofill_options_handler.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/values.h"
#include "grit/generated_resources.h"

AutoFillOptionsHandler::AutoFillOptionsHandler() {
}

AutoFillOptionsHandler::~AutoFillOptionsHandler() {
}

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

void AutoFillOptionsHandler::RegisterMessages() {
}
