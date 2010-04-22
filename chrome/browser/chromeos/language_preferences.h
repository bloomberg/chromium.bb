// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_

#include "chrome/common/pref_names.h"

// Section and config names for the IBus configuration daemon.
namespace chromeos {

// For ibus-daemon
const char kGeneralSectionName[] = "general";
const char kHotKeySectionName[] = "general/hotkey";
const char kUseGlobalEngineConfigName[] = "use_global_engine";
const char kPreloadEnginesConfigName[] = "preload_engines";
const char kNextEngineConfigName[] = "next_engine";
const char kTriggerConfigName[] = "trigger";

// TODO(yusukes): We'll add more "next engine" hot-keys like "Zenkaku_Hankaku"
// (Japanese keyboard specific).
const wchar_t kHotkeyNextEngine[] = L"Shift+Alt_L,Alt+Shift_L,Alt+grave";
const wchar_t kHotkeyTrigger[] = L"";  // We don't allow users to disable IBus.

// For Korean input method (ibus-hangul)
const char kHangulSectionName[] = "engine/Hangul";
const char kHangulKeyboardConfigName[] = "HangulKeyboard";

const struct HangulKeyboardNameIDPair {
  const wchar_t* keyboard_name;
  const wchar_t* keyboard_id;
} kHangulKeyboardNameIDPairs[] = {
  // We have to sync the IDs with those in ibus-hangul/files/setup/main.py.
  { L"Dubeolsik", L"2" },
  { L"Sebeolsik Final", L"3f" },
  { L"Sebeolsik 390", L"39" },
  { L"Sebeolsik No-shift", L"3s" },
  { L"Sebeolsik 2 set", L"32" },
  // TODO(yusukes): Use generated_resources.grd IDs for |keyboard_name|. Ask
  // jshin first.
};

// For Simplified Chinese input method (ibus-pinyin)
const char kPinyinSectionName[] = "engine/Pinyin";

// We have to sync the |ibus_config_name|s with those in
// ibus-pinyin/files/src/Config.cc.
const struct {
  const wchar_t* pref_name;
  const char* ibus_config_name;
  bool default_value;
} kPinyinBooleanPrefs[] = {
  { prefs::kLanguagePinyinCorrectPinyin, "correct_pinyin", true },
  { prefs::kLanguagePinyinFuzzyPinyin, "fuzzy_pinyin", false },
  { prefs::kLanguagePinyinShiftSelectCandidate, "shift_select_candidate",
    false },
  { prefs::kLanguagePinyinMinusEqualPage, "minus_equal_page", true },
  { prefs::kLanguagePinyinCommaPeriodPage, "comma_period_page", true },
  { prefs::kLanguagePinyinAutoCommit, "auto_commit", false },
  { prefs::kLanguagePinyinDoublePinyin, "double_pinyin", false },
  { prefs::kLanguagePinyinInitChinese, "init_chinese", true },
  { prefs::kLanguagePinyinInitFull, "init_full", false },
  { prefs::kLanguagePinyinInitFullPunct, "init_full_punct", true },
  { prefs::kLanguagePinyinInitSimplifiedChinese, "init_simplified_chinese",
    true },
  { prefs::kLanguagePinyinTradCandidate, "trad_candidate", false },
  // TODO(yusukes): Support PINYIN_{INCOMPLETE,CORRECT,FUZZY}_... prefs (32
  // additional boolean prefs.)
};

const struct {
  const wchar_t* pref_name;
  const char* ibus_config_name;
  int default_value;
} kPinyinIntegerPrefs[] = {
  { prefs::kLanguagePinyinDoublePinyinSchema, "double_pinyin_schema", 0 },
  // TODO(yusukes): the type of lookup_table_page_size on ibus should be uint.
  { prefs::kLanguagePinyinLookupTablePageSize, "lookup_table_page_size", 5 },
};

// For Traditional Chinese input method (ibus-chewing)

// For Japanese input method (ibus-mozc)

// TODO(yusukes): Add constants for these components.
}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_

