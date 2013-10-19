// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/translate/language_combobox_model.h"

#include "chrome/browser/ui/translate/translate_bubble_model.h"

LanguageComboboxModel::LanguageComboboxModel(
    int default_index,
    TranslateBubbleModel* model)
    : default_index_(default_index),
      model_(model) {
}

LanguageComboboxModel::~LanguageComboboxModel() {
}

int LanguageComboboxModel::GetItemCount() const {
  return model_->GetNumberOfLanguages();
}

string16 LanguageComboboxModel::GetItemAt(int index) {
  return model_->GetLanguageNameAt(index);
}

bool LanguageComboboxModel::IsItemSeparatorAt(int index) {
  return false;
}

int LanguageComboboxModel::GetDefaultIndex() const {
  return default_index_;
}
