// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/save_account_more_combobox_model.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SaveAccountMoreComboboxModel::SaveAccountMoreComboboxModel() {
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_MORE_BUTTON));
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON));
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SETTINGS_LINK));
}

SaveAccountMoreComboboxModel::~SaveAccountMoreComboboxModel() {}

int SaveAccountMoreComboboxModel::GetItemCount() const {
  return items_.size();
}

base::string16 SaveAccountMoreComboboxModel::GetItemAt(int index) {
  return items_[index];
}
