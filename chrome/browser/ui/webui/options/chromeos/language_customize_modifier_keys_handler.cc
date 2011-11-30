// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/language_customize_modifier_keys_handler.h"

#include "base/values.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const struct ModifierKeysSelectItem {
  int message_id;
  chromeos::input_method::ModifierKey value;
} kModifierKeysSelectItems[] = {
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_SEARCH,
    chromeos::input_method::kSearchKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_CTRL,
    chromeos::input_method::kLeftControlKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_ALT,
    chromeos::input_method::kLeftAltKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_VOID,
    chromeos::input_method::kVoidKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_CAPS_LOCK,
    chromeos::input_method::kCapsLockKey },
};

const char* kDataValuesNames[] = {
  "xkbRemapSearchKeyToValue",
  "xkbRemapControlKeyToValue",
  "xkbRemapAltKeyToValue",
};
}  // namespace

namespace chromeos {

LanguageCustomizeModifierKeysHandler::LanguageCustomizeModifierKeysHandler() {
}

LanguageCustomizeModifierKeysHandler::~LanguageCustomizeModifierKeysHandler() {
}

void LanguageCustomizeModifierKeysHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("xkbRemapSearchKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_SEARCH_LABEL));
  localized_strings->SetString("xkbRemapControlKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_CTRL_LABEL));
  localized_strings->SetString("xkbRemapAltKeyToContent",
      l10n_util::GetStringUTF16(
          IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_ALT_LABEL));

  for (size_t i = 0; i < arraysize(kDataValuesNames); ++i) {
    ListValue* list_value = new ListValue();
    for (size_t j = 0; j < arraysize(kModifierKeysSelectItems); ++j) {
      const input_method::ModifierKey value =
          kModifierKeysSelectItems[j].value;
      const int message_id = kModifierKeysSelectItems[j].message_id;
      // Only the seach key can be remapped to the caps lock key.
      if (kDataValuesNames[i] != std::string("xkbRemapSearchKeyToValue") &&
          value == input_method::kCapsLockKey) {
        continue;
      }
      ListValue* option = new ListValue();
      option->Append(Value::CreateIntegerValue(value));
      option->Append(Value::CreateStringValue(l10n_util::GetStringUTF16(
          message_id)));
      list_value->Append(option);
    }
    localized_strings->Set(kDataValuesNames[i], list_value);
  }
}

}  // namespace chromeos
