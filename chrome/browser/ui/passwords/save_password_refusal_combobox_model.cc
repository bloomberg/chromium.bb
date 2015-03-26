// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/save_password_refusal_combobox_model.h"

#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

SavePasswordRefusalComboboxModel::SavePasswordRefusalComboboxModel(
    bool never_is_default)
    : never_is_default_(never_is_default) {
#if !defined(OS_ANDROID)
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON));
  items_.push_back(
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_BLACKLIST_BUTTON));
  if (never_is_default)
    swap(items_[0], items_[1]);
#endif
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
