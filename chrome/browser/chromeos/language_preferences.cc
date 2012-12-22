// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace chromeos {
namespace language_prefs {

// ---------------------------------------------------------------------------
// For ibus-daemon
// ---------------------------------------------------------------------------
const char kGeneralSectionName[] = "general";
const char kPreloadEnginesConfigName[] = "preload_engines";

// ---------------------------------------------------------------------------
// For Traditional Chinese input method (ibus-mozc-chewing)
// ---------------------------------------------------------------------------
const char kChewingSectionName[] = "engine/Chewing";

// We have to sync the |ibus_config_name|s with those in
// ibus-chewing/files/src/Config.cc.
const LanguageBooleanPrefs kChewingBooleanPrefs[] = {
  { prefs::kLanguageChewingAutoShiftCur, false, "autoShiftCur",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_AUTO_SHIFT_CUR,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageChewingAddPhraseDirection, false, "addPhraseDirection",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_ADD_PHRASE_DIRECTION,
    PrefServiceSyncable::SYNCABLE_PREF },
  /* Temporarily disabled. (crosbug.com/14185)
  { prefs::kLanguageChewingEasySymbolInput, true, "easySymbolInput",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_EASY_SYMBOL_INPUT,
    PrefServiceSyncable::SYNCABLE_PREF },
  */
  { prefs::kLanguageChewingEscCleanAllBuf, false, "escCleanAllBuf",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_ESC_CLEAN_ALL_BUF,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageChewingForceLowercaseEnglish, false,
    "forceLowercaseEnglish",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_FORCE_LOWER_CASE_ENGLISH,
    PrefServiceSyncable::SYNCABLE_PREF },
  /* Temporarily disabled. (crosbug.com/14185)
  { prefs::kLanguageChewingPlainZhuyin, false, "plainZhuyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_PLAIN_ZHUYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  */
  { prefs::kLanguageChewingPhraseChoiceRearward, true, "phraseChoiceRearward",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_PHRASE_CHOICE_REARWARD,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageChewingSpaceAsSelection, true, "spaceAsSelection",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_SPACE_AS_SELECTION,
    PrefServiceSyncable::SYNCABLE_PREF },
};
COMPILE_ASSERT(kNumChewingBooleanPrefs == arraysize(kChewingBooleanPrefs),
               TheSizeShouldMatch);

const LanguageIntegerRangePreference kChewingIntegerPrefs[] = {
  { prefs::kLanguageChewingMaxChiSymbolLen, 20, 8, 40, "maxChiSymbolLen",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_MAX_CHI_SYMBOL_LEN,
    PrefServiceSyncable::SYNCABLE_PREF
  },
  { prefs::kLanguageChewingCandPerPage, 10, 8, 10, "candPerPage",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_CAND_PER_PAGE,
    PrefServiceSyncable::SYNCABLE_PREF
  },
};
COMPILE_ASSERT(kNumChewingIntegerPrefs == arraysize(kChewingIntegerPrefs),
               TheSizeShouldMatch);

const LanguageMultipleChoicePreference<const char*>
    kChewingMultipleChoicePrefs[] = {
  { prefs::kLanguageChewingKeyboardType,
    "default",
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
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageChewingSelKeys,
    "1234567890",
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
    PrefServiceSyncable::SYNCABLE_PREF,
  },
};
COMPILE_ASSERT(kNumChewingMultipleChoicePrefs ==
               arraysize(kChewingMultipleChoicePrefs),
               TheSizeShouldMatch);

const LanguageMultipleChoicePreference<int> kChewingHsuSelKeyType = {
  prefs::kLanguageChewingHsuSelKeyType,
  1,
  "hsuSelKeyType",
  {{ 1, IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_HSU_SEL_KEY_TYPE_1 },
   { 2, IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_HSU_SEL_KEY_TYPE_2 }},
  IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_HSU_SEL_KEY_TYPE,
  PrefServiceSyncable::SYNCABLE_PREF,
};

// ---------------------------------------------------------------------------
// For Korean input method (ibus-mozc-hangul)
// ---------------------------------------------------------------------------
const char kHangulSectionName[] = "engine/Hangul";
const char kHangulKeyboardConfigName[] = "HangulKeyboard";

const char kHangulHanjaBindingKeysConfigName[] = "HanjaKeyBindings";
// Mozc-hangul treats Hangul_Hanja key as hanja key event even if it is not set.
// We add Control+9 since F9 key is reserved by the window manager.
// TODO(nona): Hanja keys are not configurable yet (and we're not sure if it
// should.)
const char kHangulHanjaBindingKeys[] = "F9,Ctrl 9";

const HangulKeyboardNameIDPair kHangulKeyboardNameIDPairs[] = {
  // We have to sync the |keyboard_id|s with those in libhangul.
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_2_SET, "2" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_FINAL,
    "3f" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_390, "39" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_NO_SHIFT,
    "3s" },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_ROMAJA, "ro" },
  // We don't support "Sebeolsik 2 set" keyboard.
};
COMPILE_ASSERT(kNumHangulKeyboardNameIDPairs ==
               arraysize(kHangulKeyboardNameIDPairs),
               TheSizeShouldMatch);

// ---------------------------------------------------------------------------
// For Simplified Chinese input method (ibus-mozc-pinyin)
// ---------------------------------------------------------------------------
const char kPinyinSectionName[] = "engine/Pinyin";

// We have to sync the |ibus_config_name|s with those in
// ibus-mozc-pinyin/files/languages/pinyin/unix/ibus/config_updater.cc.
const LanguageBooleanPrefs kPinyinBooleanPrefs[] = {
  { prefs::kLanguagePinyinCorrectPinyin, true, "CorrectPinyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_CORRECT_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinFuzzyPinyin, false, "FuzzyPinyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_FUZZY_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinShiftSelectCandidate, false, "ShiftSelectCandidate",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_SHIFT_SELECT_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinMinusEqualPage, true, "MinusEqualPage",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_MINUS_EQUAL_PAGE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinCommaPeriodPage, true, "CommaPeriodPage",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_COMMA_PERIOD_PAGE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinAutoCommit, false, "AutoCommit",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_AUTO_COMMIT,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinDoublePinyin, false, "DoublePinyin",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_DOUBLE_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitChinese, true, "InitChinese",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_CHINESE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitFull, false, "InitFull",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitFullPunct, true, "InitFullPunct",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL_PUNCT,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitSimplifiedChinese, true, "InitSimplifiedChinese",
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_SIMPLIFIED_CHINESE,
    PrefServiceSyncable::SYNCABLE_PREF },
  // TODO(yusukes): Support PINYIN_{INCOMPLETE,CORRECT,FUZZY}_... prefs (32
  // additional boolean prefs.)
};
COMPILE_ASSERT(kNumPinyinBooleanPrefs == arraysize(kPinyinBooleanPrefs),
               TheSizeShouldMatch);
// TODO(yusukes): Support HalfWidthPuncts and IncompletePinyin prefs if needed.

const LanguageMultipleChoicePreference<int> kPinyinDoublePinyinSchema = {
  prefs::kLanguagePinyinDoublePinyinSchema,
  0,
  "DoublePinyinSchema",
  {{ 0, IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_MSPY},
   { 1, IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_ZRM},
   { 2, IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_ABC},
   { 3, IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_ZGPY},
   { 4, IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_PYJJ}},
  IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA,
  PrefServiceSyncable::SYNCABLE_PREF,
};

const PinyinIntegerPref kPinyinIntegerPrefs[] = {
  // TODO(yusukes): the type of lookup_table_page_size on ibus should be uint.
  { prefs::kLanguagePinyinLookupTablePageSize,
    5,
    "LookupTablePageSize",

    // don't sync as it's not user configurable.
    PrefServiceSyncable::UNSYNCABLE_PREF }
};
COMPILE_ASSERT(kNumPinyinIntegerPrefs == arraysize(kPinyinIntegerPrefs),
               TheSizeShouldMatch);

// ---------------------------------------------------------------------------
// For Japanese input method (ibus-mozc)
// ---------------------------------------------------------------------------
const char kMozcSectionName[] = "engine/Mozc";

const LanguageBooleanPrefs kMozcBooleanPrefs[] = {
  { prefs::kLanguageMozcIncognitoMode,
    false,
    "incognito_mode",
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_INCOGNITO_MODE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageMozcUseAutoImeTurnOff,
    true,
    "use_auto_ime_turn_off",
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_USE_AUTO_IME_TURN_OFF,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageMozcUseHistorySuggest,
    true,
    "use_history_suggest",
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_USE_HISTORY_SUGGEST,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageMozcUseDictionarySuggest,
    true,
    "use_dictionary_suggest",
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_USE_DICTIONARY_SUGGEST,
    PrefServiceSyncable::SYNCABLE_PREF },
};
COMPILE_ASSERT(kNumMozcBooleanPrefs == arraysize(kMozcBooleanPrefs),
               TheSizeShouldMatch);

extern const LanguageMultipleChoicePreference<const char*>
    kMozcMultipleChoicePrefs[] = {
  { prefs::kLanguageMozcPreeditMethod,
    "ROMAN",
    "preedit_method",
    {{ "ROMAN", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD_ROMAN },
     { "KANA", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD_KANA }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcSessionKeymap,
    "MSIME",
    "session_keymap",
    {{ "ATOK", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_ATOK },
     { "MSIME", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_MSIME },
     { "KOTOERI", IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_KOTOERI }},
    // TODO: Support "CUSTOM" keymap.
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcPunctuationMethod,
    "KUTEN_TOUTEN",
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
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcSymbolMethod,
    "CORNER_BRACKET_MIDDLE_DOT",
    "symbol_method",
    {{ "CORNER_BRACKET_MIDDLE_DOT",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_CORNER_BRACKET_MIDDLE_DOT },
     { "SQUARE_BRACKET_SLASH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_SQUARE_BRACKET_SLASH },
     { "CORNER_BRACKET_SLASH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_CORNER_BRACKET_SLASH },
     { "SQUARE_BRACKET_MIDDLE_DOT",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_SQUARE_BRACKET_MIDDLE_DOT }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcSpaceCharacterForm,
    "FUNDAMENTAL_INPUT_MODE",
    "space_character_form",
    {{ "FUNDAMENTAL_INPUT_MODE",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM_FUNDAMENTAL_INPUT_MODE },
     { "FUNDAMENTAL_FULL_WIDTH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM_FUNDAMENTAL_FULL_WIDTH },
     { "FUNDAMENTAL_HALF_WIDTH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM_FUNDAMENTAL_HALF_WIDTH }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcHistoryLearningLevel,
    "DEFAULT_HISTORY",
    "history_learning_level",
    {{ "DEFAULT_HISTORY",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL_DEFAULT_HISTORY },
     { "READ_ONLY",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL_READ_ONLY },
     { "NO_HISTORY",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL_NO_HISTORY }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  // TODO(mazda): Uncomment this block once the candidate window in Chrome OS
  // supports changing shortcut labels.
  // { prefs::kLanguageMozcSelectionShortcut,
  //   "SHORTCUT_123456789",
  //   "selection_shortcut",
  //   {{ "NO_SHORTCUT",
  //      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT_NO_SHORTCUT },
  //    { "SHORTCUT_123456789",
  //      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT_SHORTCUT_123456789 },
  //    { "SHORTCUT_ASDFGHJKL",
  //      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT_SHORTCUT_ASDFGHJKL }},
  //   IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT,
  //   PrefServiceSyncable::SYNCABLE_PREF,
  // },
  { prefs::kLanguageMozcShiftKeyModeSwitch,
    "ASCII_INPUT_MODE",
    "shift_key_mode_switch",
    {{ "OFF",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH_OFF },
     { "ASCII_INPUT_MODE",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH_ASCII_INPUT_MODE },
     { "KATAKANA_INPUT_MODE",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH_KATAKANA_INPUT_MODE }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcNumpadCharacterForm,
    "NUMPAD_HALF_WIDTH",
    "numpad_character_form",
    {{ "NUMPAD_INPUT_MODE",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_INPUT_MODE },
     { "NUMPAD_FULL_WIDTH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_FULL_WIDTH },
     { "NUMPAD_HALF_WIDTH",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_HALF_WIDTH },
     { "NUMPAD_DIRECT_INPUT",
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_DIRECT_INPUT }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
};
COMPILE_ASSERT(kNumMozcMultipleChoicePrefs ==
               arraysize(kMozcMultipleChoicePrefs),
               TheSizeShouldMatch);

const LanguageIntegerRangePreference kMozcIntegerPrefs[] = {
  { prefs::kLanguageMozcSuggestionsSize, 3, 1, 9, "suggestions_size",
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SUGGESTIONS_SIZE,
    PrefServiceSyncable::SYNCABLE_PREF }
};
COMPILE_ASSERT(kNumMozcIntegerPrefs == arraysize(kMozcIntegerPrefs),
               TheSizeShouldMatch);

// ---------------------------------------------------------------------------
// For keyboard stuff
// ---------------------------------------------------------------------------
const int kXkbAutoRepeatDelayInMs = 500;
const int kXkbAutoRepeatIntervalInMs = 50;
const char kPreferredKeyboardLayout[] = "PreferredKeyboardLayout";

void RegisterPrefs(PrefServiceSimple* local_state) {
  // We use an empty string here rather than a hardware keyboard layout name
  // since input_method::GetHardwareInputMethodId() might return a fallback
  // layout name if local_state->RegisterStringPref(kHardwareKeyboardLayout)
  // is not called yet.
  local_state->RegisterStringPref(kPreferredKeyboardLayout,
                                  "");
}

}  // namespace language_prefs
}  // namespace chromeos
