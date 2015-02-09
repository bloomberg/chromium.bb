// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/account_chooser_more_combobox_model.h"

#include "base/logging.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

const int AccountChooserMoreComboboxModel::INDEX_MORE = 0;
const int AccountChooserMoreComboboxModel::INDEX_LEARN_MORE = 1;
const int AccountChooserMoreComboboxModel::INDEX_SETTINGS = 2;

AccountChooserMoreComboboxModel::AccountChooserMoreComboboxModel() = default;

AccountChooserMoreComboboxModel::~AccountChooserMoreComboboxModel() = default;

int AccountChooserMoreComboboxModel::GetItemCount() const {
  return 3;
}

base::string16 AccountChooserMoreComboboxModel::GetItemAt(int index) {
  switch (index) {
    default:
      NOTREACHED();
    case INDEX_MORE:
      return l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_MANAGEMENT_ACCOUNT_CHOOSER_MORE);
    case INDEX_LEARN_MORE:
      return l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_MANAGEMENT_ACCOUNT_CHOOSER_LEARN_MORE);
    case INDEX_SETTINGS:
      return l10n_util::GetStringUTF16(
          IDS_CREDENTIAL_MANAGEMENT_ACCOUNT_CHOOSER_PASSWORDS_LINK);
  }
}

bool AccountChooserMoreComboboxModel::IsItemSeparatorAt(int index) {
  return false;
}

int AccountChooserMoreComboboxModel::GetDefaultIndex() const {
  return 0;
}

bool AccountChooserMoreComboboxModel::IsItemEnabledAt(int index) const {
  return true;
}
