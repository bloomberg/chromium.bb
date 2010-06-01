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
const char kPreloadEnginesConfigName[] = "preload_engines";
const char kNextEngineInMenuConfigName[] = "next_engine_in_menu";
const char kPreviousEngineConfigName[] = "previous_engine";

// TODO(suzhe): In order to avoid blocking any accelerator keys with alt+shift
// modifiers, we should use release events of alt+shift key binding instead of
// press events. The corresponding release events are:
// "Alt+Shift+Shift_L+Release" and "Alt+Shift+Meta_L+Release"
// But unfortunately http://crbug.com/40754 prevents these release events
// from taking effect.
// TODO(yusukes): Check if the "Kana/Eisu" key in the Japanese keyboard for
// Chrome OS actually generates Zenkaku_Hankaku when the keyboard gets ready.

// ibus-daemon accepts up to 5 next-engine hot-keys.
const wchar_t kHotkeyNextEngineInMenu[] =
    L"Alt+Shift_L,Shift+Meta_L,Control+Shift+space,Zenkaku_Hankaku";
// TODO(suzhe): Add more key bindings?
const wchar_t kHotkeyPreviousEngine[] = L"Control+space";

// For Simplified Chinese input method (ibus-chewing)
const char kChewingSectionName[] = "engine/Chewing";

// We have to sync the |ibus_config_name|s with those in
// ibus-chewing/files/src/Config.cc.
const struct {
  const wchar_t* pref_name;  // Chrome preference name.
  bool default_pref_value;
  const char* ibus_config_name;
  int message_id;
} kChewingBooleanPrefs[] = {
  { prefs::kLanguageChewingAutoShiftCur, false, "autoShiftCur",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_AUTO_SHIFT_CUR},
  { prefs::kLanguageChewingAddPhraseDirection, false, "addPhraseDirection",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_ADD_PHRASE_DIRECTION},
  { prefs::kLanguageChewingEasySymbolInput, true, "easySymbolInput",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_EASY_SYMBOL_INPUT},
  { prefs::kLanguageChewingEscCleanAllBuf, false, "escCleanAllBuf",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_ESC_CLEAN_ALL_BUF},
  { prefs::kLanguageChewingForceLowercaseEnglish, false,
    "forceLowercaseEnglish",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_FORCE_LOWER_CASE_ENGLISH},
  { prefs::kLanguageChewingPlainZhuyin, false, "plainZhuyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_PLAIN_ZHUYIN},
  { prefs::kLanguageChewingPhraseChoiceRearward, true, "phraseChoiceRearward",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_PHRASE_CHOICE_REARWARD},
  { prefs::kLanguageChewingSpaceAsSelection, true, "spaceAsSelection",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_SPACE_AS_SELECTION},
};
const size_t kNumChewingBooleanPrefs = ARRAYSIZE_UNSAFE(kChewingBooleanPrefs);

const struct ChewingMultipleChoicePreference {
  const wchar_t* pref_name;  // Chrome preference name.
  const wchar_t* default_pref_value;
  const char* ibus_config_name;
  // Currently we have 10 combobox items at most.
  static const size_t kMaxItems = 10;
  struct {
    const char* ibus_config_value;
    int item_message_id;  // Resource grd ID for the combobox item.
  } values_and_ids[kMaxItems];
  int label_message_id;  // Resource grd ID for the label.

} kChewingMultipleChoicePrefs[] = {
  { prefs::kLanguageChewingKeyboardType,
    L"default",
    "KBType",
    {{ "default",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DEFAULT },
     { "hsu", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_HSU },
     { "ibm", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_IBM },
     { "gin_yieh",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_GIN_YIEH },
     { "eten", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_ETEN },
     { "eten26", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_ETEN26 },
     { "dvorak", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DVORAK },
     { "dvorak_hsu",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DVORAK_HSU },
     { "dachen_26",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DACHEN_26 },
     { "hanyu", IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_HANYU }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE,
  },
  { prefs::kLanguageChewingSelKeys,
    L"1234567890",
    "selKeys",
    {{ "1234567890",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_1234567890 },
     { "asdfghjkl;",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_ASDFGHJKLS },
     { "asdfzxcv89",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_ASDFZXCV89 },
     { "asdfjkl789",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_ASDFJKL789 },
     { "aoeu;qjkix",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_AOEUSQJKIX },
     { "aoeuhtnsid",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_AOEUHTNSID },
     { "aoeuidhtns",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_AOEUIDHTNS },
     { "1234qweras",
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_1234QWERAS }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS,
  },
};
const size_t kNumChewingMultipleChoicePrefs =
    arraysize(kChewingMultipleChoicePrefs);
// TODO(zork): Support candPerPage, hsuSelKeyType, and maxChiSymbolLen

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
  const wchar_t* pref_name;  // Chrome preference name.
  bool default_pref_value;
  const char* ibus_config_name;
  int message_id;
} kPinyinBooleanPrefs[] = {
  { prefs::kLanguagePinyinCorrectPinyin, true, "CorrectPinyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_CORRECT_PINYIN },
  { prefs::kLanguagePinyinFuzzyPinyin, false, "FuzzyPinyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_FUZZY_PINYIN },
  { prefs::kLanguagePinyinShiftSelectCandidate, false, "ShiftSelectCandidate",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_SHIFT_SELECT_PINYIN },
  { prefs::kLanguagePinyinMinusEqualPage, true, "MinusEqualPage",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_MINUS_EQUAL_PAGE },
  { prefs::kLanguagePinyinCommaPeriodPage, true, "CommaPeriodPage",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_COMMA_PERIOD_PAGE },
  { prefs::kLanguagePinyinAutoCommit, false, "AutoCommit",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_AUTO_COMMIT },
  { prefs::kLanguagePinyinDoublePinyin, false, "DoublePinyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_DOUBLE_PINYIN },
  { prefs::kLanguagePinyinInitChinese, true, "InitChinese",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_CHINESE },
  { prefs::kLanguagePinyinInitFull, false, "InitFull",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL },
  { prefs::kLanguagePinyinInitFullPunct, true, "InitFullPunct",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL_PUNCT },
  { prefs::kLanguagePinyinInitSimplifiedChinese, true, "InitSimplifiedChinese",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_SIMPLIFIED_CHINESE },
  { prefs::kLanguagePinyinTradCandidate, false, "TradCandidate",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_TRAD_CANDIDATE },
  // TODO(yusukes): Support PINYIN_{INCOMPLETE,CORRECT,FUZZY}_... prefs (32
  // additional boolean prefs.)
};
const size_t kNumPinyinBooleanPrefs = ARRAYSIZE_UNSAFE(kPinyinBooleanPrefs);
// TODO(yusukes): Support HalfWidthPuncts and IncompletePinyin prefs if needed.

const struct {
  const wchar_t* pref_name;  // Chrome preference name.
  int default_pref_value;
  const char* ibus_config_name;
  // TODO(yusukes): Add message_id if needed.
} kPinyinIntegerPrefs[] = {
  { prefs::kLanguagePinyinDoublePinyinSchema, 0, "DoublePinyinSchema" },
  // TODO(yusukes): the type of lookup_table_page_size on ibus should be uint.
  { prefs::kLanguagePinyinLookupTablePageSize, 5, "LookupTablePageSize" },
};
const size_t kNumPinyinIntegerPrefs = ARRAYSIZE_UNSAFE(kPinyinIntegerPrefs);

// For Japanese input method (ibus-mozc)
const char kMozcSectionName[] = "engine/Mozc";

const struct MozcMultipleChoicePreference {
  const wchar_t* pref_name;  // Chrome preference name.
  const wchar_t* default_pref_value;
  // The config names and values have to be matched with protobuf member names
  // in chromiumos/src/third_party/ibus-mozc/files/src/session/config.proto
  // since ibus-mozc uses protobuf reflection APIs to pass prefs to the Mozc
  // Japanese converter.
  const char* ibus_config_name;
  // Currently we have 4 combobox items at most.
  static const size_t kMaxItems = 4;
  struct {
    const char* ibus_config_value;
    int item_message_id;  // Resource grd ID for the combobox item.
  } values_and_ids[kMaxItems];
  int label_message_id;  // Resource grd ID for the label.

} kMozcMultipleChoicePrefs[] = {
  { prefs::kLanguageMozcPreeditMethod,
    L"ROMAN",
    "preedit_method",
    {{ "ROMAN", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD_ROMAN },
     { "KANA", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD_KANA }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD,
  },
  { prefs::kLanguageMozcSessionKeymap,
    L"MSIME",
    "session_keymap",
    {{ "ATOK", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_ATOK },
     { "MSIME", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_MSIME },
     { "KOTOERI", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_KOTOERI }},
    // TODO: Support "CUSTOM" keymap.
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP,
  },
  { prefs::kLanguageMozcPunctuationMethod,
    L"KUTEN_TOTEN",
    "punctuation_method",
    {{ "KUTEN_TOUTEN",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_KUTEN_TOUTEN },
     { "COMMA_PERIOD",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_COMMA_PERIOD },
     { "KUTEN_PERIOD",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_KUTEN_PERIOD },
     { "COMMA_TOUTEN",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_COMMA_TOUTEN }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD,
  },
  { prefs::kLanguageMozcSymbolMethod,
    L"CORNER_BRACKET_MIDDLE_DOT",
    "symbol_method",
    {{ "CORNER_BRACKET_MIDDLE_DOT",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_CORNER_BRACKET_MIDDLE_DOT },
     { "SQUARE_BRACKET_SLASH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_SQUARE_BRACKET_SLASH },
     { "CORNER_BRACKET_SLASH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_CORNER_BRACKET_SLASH },
     { "SQUARE_BRACKET_MIDDLE_DOT",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_SQUARE_BRACKET_MIDDLE_DOT }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD,
  },
};
const size_t kNumMozcMultipleChoicePrefs = arraysize(kMozcMultipleChoicePrefs);

// For Traditional Chinese input methods (ibus-pinyin-bopomofo and ibus-chewing)
// TODO(yusukes): Add constants for Traditional Chinese input methods.
}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
