// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/languages_menu_model.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"

LanguagesMenuModel::LanguagesMenuModel(
    TranslateInfoBarDelegate* translate_delegate,
    LanguageType language_type)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      translate_infobar_delegate_(translate_delegate),
      language_type_(language_type) {
  for (size_t i = 0; i < translate_delegate->GetLanguageCount(); ++i) {
    AddCheckItem(static_cast<int>(i),
                 translate_delegate->GetLanguageDisplayableNameAt(i));
  }
}

LanguagesMenuModel::~LanguagesMenuModel() {
}

bool LanguagesMenuModel::IsCommandIdChecked(int command_id) const {
  return static_cast<size_t>(command_id) == ((language_type_ == ORIGINAL) ?
      translate_infobar_delegate_->original_language_index() :
      translate_infobar_delegate_->target_language_index());
}

bool LanguagesMenuModel::IsCommandIdEnabled(int command_id) const {
  // Prevent the same language from being selectable in original and target.
  return static_cast<size_t>(command_id) != ((language_type_ == ORIGINAL) ?
      translate_infobar_delegate_->target_language_index() :
      translate_infobar_delegate_->original_language_index());
}

bool LanguagesMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void LanguagesMenuModel::ExecuteCommand(int command_id) {
  size_t command_id_size_t = static_cast<size_t>(command_id);
  if (language_type_ == ORIGINAL) {
    UMA_HISTOGRAM_COUNTS("Translate.ModifyOriginalLang", 1);
    translate_infobar_delegate_->SetOriginalLanguage(command_id_size_t);
    return;
  }
  UMA_HISTOGRAM_COUNTS("Translate.ModifyTargetLang", 1);
  translate_infobar_delegate_->SetTargetLanguage(command_id_size_t);
}
