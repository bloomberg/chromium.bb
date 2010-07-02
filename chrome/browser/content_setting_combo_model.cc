// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/content_setting_combo_model.h"

#include "app/l10n_util.h"
#include "grit/generated_resources.h"

namespace {

// The settings shown in the combobox if show_ask_ is false;
const ContentSetting kNoAskSettings[] = { CONTENT_SETTING_ALLOW,
                                          CONTENT_SETTING_BLOCK };

// The settings shown in the combobox if show_ask_ is true;
const ContentSetting kAskSettings[] = { CONTENT_SETTING_ALLOW,
                                        CONTENT_SETTING_ASK,
                                        CONTENT_SETTING_SESSION_ONLY,
                                        CONTENT_SETTING_BLOCK };

}  // namespace

ContentSettingComboModel::ContentSettingComboModel(bool show_ask)
    : show_ask_(show_ask) {
}

ContentSettingComboModel::~ContentSettingComboModel() {
}

int ContentSettingComboModel::GetItemCount() {
  return show_ask_ ? arraysize(kAskSettings) : arraysize(kNoAskSettings);
}

std::wstring ContentSettingComboModel::GetItemAt(int index) {
  switch (SettingForIndex(index)) {
    case CONTENT_SETTING_ALLOW:
      return l10n_util::GetString(IDS_EXCEPTIONS_ALLOW_BUTTON);
    case CONTENT_SETTING_BLOCK:
      return l10n_util::GetString(IDS_EXCEPTIONS_BLOCK_BUTTON);
    case CONTENT_SETTING_ASK:
      return l10n_util::GetString(IDS_EXCEPTIONS_ASK_BUTTON);
    case CONTENT_SETTING_SESSION_ONLY:
      return l10n_util::GetString(IDS_EXCEPTIONS_SESSION_ONLY_BUTTON);
    default:
      NOTREACHED();
  }
  return std::wstring();
}

ContentSetting ContentSettingComboModel::SettingForIndex(int index) {
  return show_ask_ ? kAskSettings[index] : kNoAskSettings[index];
}

int ContentSettingComboModel::IndexForSetting(ContentSetting setting) {
  for (int i = 0; i < GetItemCount(); ++i)
    if (SettingForIndex(i) == setting)
      return i;
  NOTREACHED();
  return 0;
}

