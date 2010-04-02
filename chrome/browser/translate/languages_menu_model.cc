// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/languages_menu_model.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/browser_process.h"
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

  // List of languages in chrome language.
  std::vector<string16> display_languages;
  // Reserve size since we know it.
  display_languages.reserve(languages.size());

  // Map of language's display name to its menu command id.
  std::map<string16, int> language_cmdid_map;

  // Get display name of each locale and hash it into language cmdid map.
  std::vector<std::string>::const_iterator iter = languages.begin();
  for (int i = base_command_id; iter != languages.end(); ++i, ++iter) {
    string16 display_name = translate_delegate->GetDisplayNameForLocale(*iter);
    display_languages.push_back(display_name);
    language_cmdid_map[display_name] = i;
  }

  // Sort using locale-specific sorter.
  l10n_util::SortStrings16(g_browser_process->GetApplicationLocale(),
      &display_languages);

  // Add sorted list of display languages to menu.
  for (std::vector<string16>::const_iterator iter = display_languages.begin();
       iter != display_languages.end(); ++iter) {
    AddItem(language_cmdid_map[*iter], *iter);
  }
}

LanguagesMenuModel::~LanguagesMenuModel() {
}

