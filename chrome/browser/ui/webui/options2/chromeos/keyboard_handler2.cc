// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options2/chromeos/keyboard_handler2.h"

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
    chromeos::input_method::kControlKey },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_XKB_KEY_LEFT_ALT,
    chromeos::input_method::kAltKey },
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
namespace options2 {

KeyboardHandler::KeyboardHandler() {
}

KeyboardHandler::~KeyboardHandler() {
}

void KeyboardHandler::GetLocalizedValues(DictionaryValue* localized_strings) {
  DCHECK(localized_strings);

  localized_strings->SetString("keyboardOverlayTitle",
      l10n_util::GetStringUTF16(IDS_OPTIONS_KEYBOARD_OVERLAY_TITLE));
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

}  // namespace options2
}  // namespace chromeos
