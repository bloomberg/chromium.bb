// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dom_ui/personal_options_handler.h"

#include "app/l10n_util.h"
#include "base/basictypes.h"
#include "base/values.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"

PersonalOptionsHandler::PersonalOptionsHandler() {
}

PersonalOptionsHandler::~PersonalOptionsHandler() {
}

void PersonalOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  localized_strings->SetString(L"passwordsGroupName",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_GROUP_NAME));
  localized_strings->SetString(L"passwordsAskToSave",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_ASKTOSAVE));
  localized_strings->SetString(L"passwordsNeverSave",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_NEVERSAVE));
  localized_strings->SetString(L"passwordShowPasswords",
      l10n_util::GetString(IDS_OPTIONS_PASSWORDS_SHOWPASSWORDS));
  localized_strings->SetString(L"autoFillSettingWindowsGroupName",
      l10n_util::GetString(IDS_AUTOFILL_SETTING_WINDOWS_GROUP_NAME));
  localized_strings->SetString(L"autoFillEnable",
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_ENABLE));
  localized_strings->SetString(L"autoFillDisable",
      l10n_util::GetString(IDS_OPTIONS_AUTOFILL_DISABLE));
  localized_strings->SetString(L"themesGroupName",
      l10n_util::GetString(IDS_THEMES_GROUP_NAME));
#if defined(OS_LINUX) || defined(OS_FREEBSD) || defined(OS_OPENBSD)
  localized_strings->SetString(L"appearanceGroupName",
      l10n_util::GetString(IDS_APPEARANCE_GROUP_NAME));
  localized_strings->SetString(L"themesGTKButton",
      l10n_util::GetString(IDS_THEMES_GTK_BUTTON));
  localized_strings->SetString(L"themesSetClassic",
      l10n_util::GetString(IDS_THEMES_SET_CLASSIC));
  localized_strings->SetString(L"showWindowDecorationsRadio",
      l10n_util::GetString(IDS_SHOW_WINDOW_DECORATIONS_RADIO));
  localized_strings->SetString(L"hideWindowDecorationsRadio",
      l10n_util::GetString(IDS_HIDE_WINDOW_DECORATIONS_RADIO));
#endif
  localized_strings->SetString(L"themesResetButton",
      l10n_util::GetString(IDS_THEMES_RESET_BUTTON));
  localized_strings->SetString(L"themesDefaultThemeLabel",
      l10n_util::GetString(IDS_THEMES_DEFAULT_THEME_LABEL));
  localized_strings->SetString(L"themesGalleryButton",
      l10n_util::GetString(IDS_THEMES_GALLERY_BUTTON));
  localized_strings->SetString(L"browsingDataGroupName",
      l10n_util::GetString(IDS_OPTIONS_BROWSING_DATA_GROUP_NAME));
  localized_strings->SetString(L"importDataButton",
      l10n_util::GetString(IDS_OPTIONS_IMPORT_DATA_BUTTON));
}
