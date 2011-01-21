// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_customize_modifier_keys_handler.h"

#include "base/values.h"
#include "grit/generated_resources.h"
#include "third_party/cros/chromeos_keyboard.h"
#include "ui/base/l10n/l10n_util.h"

namespace {
const struct ModifierKeysSelectItem {
  int message_id;
  chromeos::ModifierKey value;
} kModifierKeysSelectItems[] = {
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_SEARCH_LABEL,
    chromeos::kSearchKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_CTRL,
    chromeos::kLeftControlKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_ALT,
    chromeos::kLeftAltKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_VOID,
    chromeos::kVoidKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_CAPSLOCK,
    chromeos::kCapsLockKey },
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
      const ModifierKey value = kModifierKeysSelectItems[j].value;
      const int message_id = kModifierKeysSelectItems[j].message_id;
      // Only the seach key can be remapped to the caps lock key.
      if (kDataValuesNames[i] != std::string("xkbRemapSearchKeyToValue") &&
          value == kCapsLockKey) {
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
