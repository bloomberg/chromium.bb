// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_options_handler.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

LanguageOptionsHandler::LanguageOptionsHandler() {
}

LanguageOptionsHandler::~LanguageOptionsHandler() {
}

void LanguageOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"languagePage",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE));
  localized_strings->SetString(L"add_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_BUTTON));
  localized_strings->SetString(L"configure",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE));
  localized_strings->SetString(L"input_method",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_INPUT_METHOD));
  localized_strings->SetString(L"languages",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES));
  localized_strings->SetString(L"remove_button",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_REMOVE_BUTTON));
  localized_strings->SetString(L"others",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS));
  localized_strings->SetString(L"is_displayed_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_IS_DISPLAYED_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
  localized_strings->SetString(L"display_in_this_language",
      l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_LANGUAGES_DISPLAY_IN_THIS_LANGUAGE,
          l10n_util::GetString(IDS_PRODUCT_OS_NAME)));
}
