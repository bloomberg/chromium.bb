// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/options_menu_model.h"

#include "app/l10n_util.h"
#include "chrome/app/chrome_dll_resource.h"
#include "chrome/browser/translate/translate_infobars_delegates.h"
#include "grit/generated_resources.h"

OptionsMenuModel::OptionsMenuModel(menus::SimpleMenuModel::Delegate* delegate,
    TranslateInfoBarDelegate* translate_delegate, bool before_translate)
    : menus::SimpleMenuModel(delegate) {
  string16 original_language =
      TranslateInfoBarDelegate::GetDisplayNameForLocale(
          translate_delegate->original_lang_code());
  if (before_translate) {
    AddCheckItem(IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_LANG,
        l10n_util::GetStringFUTF16(
            IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_LANG,
            original_language));
    AddCheckItem(IDC_TRANSLATE_OPTIONS_NEVER_TRANSLATE_SITE,
        l10n_util::GetStringUTF16(
            IDS_TRANSLATE_INFOBAR_OPTIONS_NEVER_TRANSLATE_SITE));
  } else {
    string16 target_language =
        TranslateInfoBarDelegate::GetDisplayNameForLocale(
            translate_delegate->target_lang_code());
    AddCheckItem(IDC_TRANSLATE_OPTIONS_ALWAYS,
        l10n_util::GetStringFUTF16(IDS_TRANSLATE_INFOBAR_OPTIONS_ALWAYS,
            original_language, target_language));
  }
  AddItemWithStringId(IDC_TRANSLATE_OPTIONS_ABOUT,
      IDS_TRANSLATE_INFOBAR_OPTIONS_ABOUT);
}

OptionsMenuModel::~OptionsMenuModel() {
}

