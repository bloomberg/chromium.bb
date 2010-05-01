// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_

#include "base/basictypes.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

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
  int message_id;
  const wchar_t* keyboard_id;
} kHangulKeyboardNameIDPairs[] = {
  // We have to sync the |keyboard_id|s with those in
  // ibus-hangul/files/setup/main.py.
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_2_SET, L"2" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_FINAL,
    L"3f" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_390, L"39" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_NO_SHIFT,
    L"3s" },
  // We don't support "Sebeolsik 2 set" keyboard.
};

// For Simplified Chinese input method (ibus-pinyin)
const char kPinyinSectionName[] = "engine/Pinyin";

// We have to sync the |ibus_config_name|s with those in
// ibus-pinyin/files/src/Config.cc.
const struct {
  const wchar_t* pref_name;
  const char* ibus_config_name;
  bool default_value;
  int message_id;
} kPinyinBooleanPrefs[] = {
  { prefs::kLanguagePinyinCorrectPinyin, "correct_pinyin", true,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_CORRECT_PINYIN },
  { prefs::kLanguagePinyinFuzzyPinyin, "fuzzy_pinyin", false,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_FUZZY_PINYIN },
  { prefs::kLanguagePinyinShiftSelectCandidate, "shift_select_candidate",
    false, IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_SHIFT_SELECT_PINYIN },
  { prefs::kLanguagePinyinMinusEqualPage, "minus_equal_page", true,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_MINUS_EQUAL_PAGE },
  { prefs::kLanguagePinyinCommaPeriodPage, "comma_period_page", true,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_COMMA_PERIOD_PAGE },
  { prefs::kLanguagePinyinAutoCommit, "auto_commit", false,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_AUTO_COMMIT },
  { prefs::kLanguagePinyinDoublePinyin, "double_pinyin", false,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_DOUBLE_PINYIN },
  { prefs::kLanguagePinyinInitChinese, "init_chinese", true,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_CHINESE },
  { prefs::kLanguagePinyinInitFull, "init_full", false,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL },
  { prefs::kLanguagePinyinInitFullPunct, "init_full_punct", true,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL_PUNCT },
  { prefs::kLanguagePinyinInitSimplifiedChinese, "init_simplified_chinese",
    true,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_SIMPLIFIED_CHINESE },
  { prefs::kLanguagePinyinTradCandidate, "trad_candidate", false,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_TRAD_CANDIDATE },
  // TODO(yusukes): Support PINYIN_{INCOMPLETE,CORRECT,FUZZY}_... prefs (32
  // additional boolean prefs.)
};
const size_t kNumPinyinBooleanPrefs = ARRAYSIZE_UNSAFE(kPinyinBooleanPrefs);

const struct {
  const wchar_t* pref_name;
  const char* ibus_config_name;
  int default_value;
  // TODO(yusukes): Add message_id if needed.
} kPinyinIntegerPrefs[] = {
  { prefs::kLanguagePinyinDoublePinyinSchema, "double_pinyin_schema", 0 },
  // TODO(yusukes): the type of lookup_table_page_size on ibus should be uint.
  { prefs::kLanguagePinyinLookupTablePageSize, "lookup_table_page_size", 5 },
};
const size_t kNumPinyinIntegerPrefs = ARRAYSIZE_UNSAFE(kPinyinIntegerPrefs);

// For Traditional Chinese input method (ibus-chewing)

// For Japanese input method (ibus-mozc)

// TODO(yusukes): Add constants for these components.
}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
