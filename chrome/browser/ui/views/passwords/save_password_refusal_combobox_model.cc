// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/save_password_refusal_combobox_model.h"

#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SavePasswordRefusalComboboxModel::SavePasswordRefusalComboboxModel() {
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON));
}

SavePasswordRefusalComboboxModel::~SavePasswordRefusalComboboxModel() {}

int SavePasswordRefusalComboboxModel::GetItemCount() const {
  return items_.size();
}

base::string16 SavePasswordRefusalComboboxModel::GetItemAt(int index) {
  return items_[index];
}

bool SavePasswordRefusalComboboxModel::IsItemSeparatorAt(int index) {
  return items_[index].empty();
}

int SavePasswordRefusalComboboxModel::GetDefaultIndex() const {
  return 0;
}
