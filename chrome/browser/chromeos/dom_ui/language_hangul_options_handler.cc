// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/dom_ui/language_hangul_options_handler.h"

#include "app/l10n_util.h"
#include "base/values.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "grit/generated_resources.h"

LanguageHangulOptionsHandler::LanguageHangulOptionsHandler() {
}

LanguageHangulOptionsHandler::~LanguageHangulOptionsHandler() {
}

void LanguageHangulOptionsHandler::GetLocalizedValues(
    DictionaryValue* localized_strings) {
  DCHECK(localized_strings);
  // Language Hangul page - ChromeOS
  localized_strings->SetString(L"keyboard_layout",
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_KEYBOARD_LAYOUT_TEXT));

  localized_strings->Set(L"keyboardLayoutList", GetKeyboardLayoutList());
}

ListValue* LanguageHangulOptionsHandler::GetKeyboardLayoutList() {
  ListValue* keyboard_layout_list = new ListValue();
  for (size_t i = 0; i < arraysize(chromeos::kHangulKeyboardNameIDPairs); ++i) {
    ListValue* option = new ListValue();
    option->Append(Value::CreateStringValue(ASCIIToWide(
        chromeos::kHangulKeyboardNameIDPairs[i].keyboard_id)));
    option->Append(Value::CreateStringValue(l10n_util::GetString(
        chromeos::kHangulKeyboardNameIDPairs[i].message_id)));
    keyboard_layout_list->Append(option);
  }
  return keyboard_layout_list;
}
