// Copyright (c) 2010 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#include "chrome/browser/translate/languages_menu_model.h"

#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"

LanguagesMenuModel::LanguagesMenuModel(
    menus::SimpleMenuModel::Delegate* menu_delegate,
    TranslateInfoBarDelegate* translate_delegate,
    bool original_language)
    : menus::SimpleMenuModel(menu_delegate) {
  std::vector<std::string> languages;
  int base_command_id = 0;
  if (original_language) {
    base_command_id = IDC_TRANSLATE_ORIGINAL_LANGUAGE_BASE;
    translate_delegate->GetAvailableOriginalLanguages(&languages);
  } else {
    base_command_id = IDC_TRANSLATE_TARGET_LANGUAGE_BASE;
    translate_delegate->GetAvailableTargetLanguages(&languages);
  }
  std::vector<std::string>::const_iterator iter = languages.begin();
  for (int i = base_command_id; iter != languages.end(); ++i, ++iter) {
    AddItem(i, ASCIIToUTF16(*iter));
  }
}

LanguagesMenuModel::~LanguagesMenuModel() {
}

