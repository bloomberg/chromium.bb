// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/infobars/translate_language_menu_model.h"

#include "base/metrics/histogram.h"
#include "chrome/browser/translate/translate_infobar_delegate.h"
#include "chrome/browser/ui/views/infobars/translate_infobar_base.h"

TranslateLanguageMenuModel::TranslateLanguageMenuModel(
    LanguageType language_type,
    TranslateInfoBarDelegate* infobar_delegate,
    TranslateInfoBarBase* infobar,
    views::MenuButton* button,
    bool translate_on_change)
    : ui::SimpleMenuModel(this),
      language_type_(language_type),
      infobar_delegate_(infobar_delegate),
      infobar_(infobar),
      button_(button),
      translate_on_change_(translate_on_change) {
  for (size_t i = 0; i < infobar_delegate_->num_languages(); ++i)
    AddCheckItem(static_cast<int>(i), infobar_delegate_->language_name_at(i));
}

TranslateLanguageMenuModel::~TranslateLanguageMenuModel() {
}

bool TranslateLanguageMenuModel::IsCommandIdChecked(int command_id) const {
  return static_cast<size_t>(command_id) == GetLanguageIndex();
}

bool TranslateLanguageMenuModel::IsCommandIdEnabled(int command_id) const {
  // Prevent the same language from being selectable in original and target.
  return static_cast<size_t>(command_id) != ((language_type_ == ORIGINAL) ?
      infobar_delegate_->target_language_index() :
      infobar_delegate_->original_language_index());
}

bool TranslateLanguageMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void TranslateLanguageMenuModel::ExecuteCommand(int command_id,
                                                int event_flags) {
  size_t command_id_size_t = static_cast<size_t>(command_id);
  if (language_type_ == ORIGINAL) {
    UMA_HISTOGRAM_BOOLEAN("Translate.ModifyOriginalLang", true);
    infobar_delegate_->set_original_language_index(command_id_size_t);
  } else {
    UMA_HISTOGRAM_BOOLEAN("Translate.ModifyTargetLang", true);
    infobar_delegate_->set_target_language_index(command_id_size_t);
  }
  infobar_->UpdateLanguageButtonText(button_,
      infobar_delegate_->language_name_at(GetLanguageIndex()));
  if (translate_on_change_)
    infobar_delegate_->Translate();
}

size_t TranslateLanguageMenuModel::GetLanguageIndex() const {
  return (language_type_ == ORIGINAL) ?
      infobar_delegate_->original_language_index() :
      infobar_delegate_->target_language_index();
}
