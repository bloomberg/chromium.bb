// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/accounts_options_handler.h"

#include "app/l10n_util.h"
#include "base/json/json_reader.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "grit/generated_resources.h"

namespace chromeos {

AccountsOptionsHandler::AccountsOptionsHandler() {
}

AccountsOptionsHandler::~AccountsOptionsHandler() {
}

void AccountsOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"accountsPage", l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_TAB_LABEL));

  localized_strings->SetString(L"allow_BWSI", l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_ALLOW_BWSI_DESCRIPTION));
  localized_strings->SetString(L"allow_guest",l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_ALLOW_GUEST_DESCRIPTION));
  localized_strings->SetString(L"user_list_title",l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_USER_LIST_TITLE));
  localized_strings->SetString(L"add_user",l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_ADD_USER));
  localized_strings->SetString(L"remove_user",l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_REMOVE_USER));
  localized_strings->SetString(L"add_user_email",l10n_util::GetString(
      IDS_OPTIONS_ACCOUNTS_EMAIL_LABEL));

  localized_strings->SetString(L"ok_label",l10n_util::GetString(
      IDS_OK));
  localized_strings->SetString(L"cancel_label",l10n_util::GetString(
      IDS_CANCEL));
}

}  // namespace chromeos
