// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/chromeos/input_method/input_method_constants.h"
#include "chrome/browser/chromeos/language_preferences.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"

namespace chromeos {
namespace language_prefs {

// ---------------------------------------------------------------------------
// For Traditional Chinese input method (ibus-mozc-chewing)
// ---------------------------------------------------------------------------

// We have to sync the |ibus_config_name|s with those in
// ibus-chewing/files/src/Config.cc.
const LanguageBooleanPrefs kChewingBooleanPrefs[] = {
  { prefs::kLanguageChewingAutoShiftCur, false,
    input_method::kChewingAutoShiftCur,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_AUTO_SHIFT_CUR,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageChewingAddPhraseDirection, false,
    input_method::kChewingAddPhraseDirection,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_ADD_PHRASE_DIRECTION,
    PrefServiceSyncable::SYNCABLE_PREF },
  /* Temporarily disabled. (crosbug.com/14185)
  { prefs::kLanguageChewingEasySymbolInput, true, "easySymbolInput",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_EASY_SYMBOL_INPUT,
    PrefServiceSyncable::SYNCABLE_PREF },
  */
  { prefs::kLanguageChewingEscCleanAllBuf, false,
    input_method::kChewingEscCleanAllBuf,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_ESC_CLEAN_ALL_BUF,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageChewingForceLowercaseEnglish, false,
    "forceLowercaseEnglish",
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_FORCE_LOWER_CASE_ENGLISH,
    PrefServiceSyncable::SYNCABLE_PREF },
  /* Temporarily disabled. (crosbug.com/14185)
  { prefs::kLanguageChewingPlainZhuyin, false,
    input_method::kChewingPlainZhuyin,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_PLAIN_ZHUYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  */
  { prefs::kLanguageChewingPhraseChoiceRearward, true,
    input_method::kChewingPhraseChoiceRearward,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_PHRASE_CHOICE_REARWARD,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageChewingSpaceAsSelection, true,
    input_method::kChewingSpaceAsSelection,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_SPACE_AS_SELECTION,
    PrefServiceSyncable::SYNCABLE_PREF },
};
COMPILE_ASSERT(kNumChewingBooleanPrefs == arraysize(kChewingBooleanPrefs),
               TheSizeShouldMatch);

const LanguageIntegerRangePreference kChewingIntegerPrefs[] = {
  { prefs::kLanguageChewingMaxChiSymbolLen, 20, 8, 40,
    input_method::kChewingMaxChiSymbolLen,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_MAX_CHI_SYMBOL_LEN,
    PrefServiceSyncable::SYNCABLE_PREF
  },
  { prefs::kLanguageChewingCandPerPage, 10, 8, 10,
    input_method::kChewingCandPerPage,
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SETTING_CAND_PER_PAGE,
    PrefServiceSyncable::SYNCABLE_PREF
  },
};
COMPILE_ASSERT(kNumChewingIntegerPrefs == arraysize(kChewingIntegerPrefs),
               TheSizeShouldMatch);

const LanguageMultipleChoicePreference<const char*>
    kChewingMultipleChoicePrefs[] = {
  { prefs::kLanguageChewingKeyboardType,
    input_method::kChewingKeyboardTypeDefault,
    input_method::kChewingKeyboardType,
    {{ input_method::kChewingKeyboardTypeDefault,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DEFAULT },
     { input_method::kChewingKeyboardTypeHsu,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_HSU },
     { input_method::kChewingKeyboardTypeIbm,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_IBM },
     { input_method::kChewingKeyboardTypeGinYieh,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_GIN_YIEH },
     { input_method::kChewingKeyboardTypeEten,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_ETEN },
     { input_method::kChewingKeyboardTypeEten26,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_ETEN26 },
     { input_method::kChewingKeyboardTypeDvorak,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DVORAK },
     { input_method::kChewingKeyboardTypeDvorakHsu,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DVORAK_HSU },
     { input_method::kChewingKeyboardTypeDachen26,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_DACHEN_26 },
     { input_method::kChewingKeyboardTypeHanyu,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE_HANYU }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_KEYBOARD_TYPE,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageChewingSelKeys,
    input_method::kChewingSelKeys1234567890,
    input_method::kChewingSelKeys,
    {{ input_method::kChewingSelKeys1234567890,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_1234567890 },
     { input_method::kChewingSelKeysAsdfghjkl_,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_ASDFGHJKLS },
     { input_method::kChewingSelKeysAsdfzxcv89,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_ASDFZXCV89 },
     { input_method::kChewingSelKeysAsdfjkl789,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_ASDFJKL789 },
     { input_method::kChewingSelKeysAoeu_qjkix,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_AOEUSQJKIX },
     { input_method::kChewingSelKeysAoeuhtnsid,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_AOEUHTNSID },
     { input_method::kChewingSelKeysAoeuidhtns,
       IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_SEL_KEYS_AOEUIDHTNS },
     { input_method::kChewingSelKeys1234qweras,
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
  input_method::kChewingHsuSelKeyType1,
  input_method::kChewingHsuSelKeyType,
  {{ input_method::kChewingHsuSelKeyType1,
     IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_HSU_SEL_KEY_TYPE_1 },
   { input_method::kChewingHsuSelKeyType2,
     IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_HSU_SEL_KEY_TYPE_2 }},
  IDS_OPTIONS_SETTINGS_LANGUAGES_CHEWING_HSU_SEL_KEY_TYPE,
  PrefServiceSyncable::SYNCABLE_PREF,
};

// ---------------------------------------------------------------------------
// For Korean input method (ibus-mozc-hangul)
// ---------------------------------------------------------------------------

// Mozc-hangul treats Hangul_Hanja key as hanja key event even if it is not set.
// We add Control+9 since F9 key is reserved by the window manager.
// TODO(nona): Hanja keys are not configurable yet (and we're not sure if it
// should.)
const char kHangulHanjaBindingKeys[] = "F9,Ctrl 9";

const HangulKeyboardNameIDPair kHangulKeyboardNameIDPairs[] = {
  // We have to sync the |keyboard_id|s with those in libhangul.
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_2_SET,
    input_method::kHangulKeyboard2Set },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_FINAL,
    input_method::kHangulKeyboard3SetFinal },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_390,
    input_method::kHangulKeyboard3Set390 },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_3_SET_NO_SHIFT,
    input_method::kHangulKeyboard3SetNoShift },
  { IDS_OPTIONS_SETTINGS_LANGUAGES_HANGUL_SETTINGS_KEYBOARD_ROMAJA,
    input_method::kHangulKeyboardRomaja },
  // We don't support "Sebeolsik 2 set" keyboard.
};
COMPILE_ASSERT(kNumHangulKeyboardNameIDPairs ==
               arraysize(kHangulKeyboardNameIDPairs),
               TheSizeShouldMatch);

// ---------------------------------------------------------------------------
// For Simplified Chinese input method (ibus-mozc-pinyin)
// ---------------------------------------------------------------------------

// We have to sync the |ibus_config_name|s with those in
// ibus-mozc-pinyin/files/languages/pinyin/unix/ibus/config_updater.cc.
const LanguageBooleanPrefs kPinyinBooleanPrefs[] = {
  { prefs::kLanguagePinyinCorrectPinyin, true,
    input_method::kPinyinCorrectPinyin,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_CORRECT_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinFuzzyPinyin, false, input_method::kPinyinFuzzyPinyin,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_FUZZY_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinShiftSelectCandidate, false,
    input_method::kPinyinShiftSelectCandidate,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_SHIFT_SELECT_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinMinusEqualPage, true,
    input_method::kPinyinMinusEqualPage,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_MINUS_EQUAL_PAGE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinCommaPeriodPage, true,
    input_method::kPinyinCommaPeriodPage,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_COMMA_PERIOD_PAGE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinAutoCommit, false, input_method::kPinyinAutoCommit,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_AUTO_COMMIT,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinDoublePinyin, false,
    input_method::kPinyinDoublePinyin,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_DOUBLE_PINYIN,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitChinese, true, input_method::kPinyinInitChinese,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_CHINESE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitFull, false, input_method::kPinyinInitFull,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitFullPunct, true,
    input_method::kPinyinInitFullPunct,
    IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_SETTING_INIT_FULL_PUNCT,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguagePinyinInitSimplifiedChinese, true,
    input_method::kPinyinInitSimplifiedChinese,
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
  input_method::kPinyinDoublePinyinSchemaMspy,
  input_method::kPinyinDoublePinyinSchema,
  {{ input_method::kPinyinDoublePinyinSchemaMspy,
     IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_MSPY},
   { input_method::kPinyinDoublePinyinSchemaZrm,
     IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_ZRM},
   { input_method::kPinyinDoublePinyinSchemaAbc,
     IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_ABC},
   { input_method::kPinyinDoublePinyinSchemaZgpy,
     IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_ZGPY},
   { input_method::kPinyinDoublePinyinSchemaPyjj,
     IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA_PYJJ}},
  IDS_OPTIONS_SETTINGS_LANGUAGES_PINYIN_DOUBLE_SCHEMA,
  PrefServiceSyncable::SYNCABLE_PREF,
};

const PinyinIntegerPref kPinyinIntegerPrefs[] = {
  // TODO(yusukes): the type of lookup_table_page_size on ibus should be uint.
  { prefs::kLanguagePinyinLookupTablePageSize, 5,
    input_method::kPinyinLookupTablePageSize,
    // don't sync as it's not user configurable.
    PrefServiceSyncable::UNSYNCABLE_PREF }
};
COMPILE_ASSERT(kNumPinyinIntegerPrefs == arraysize(kPinyinIntegerPrefs),
               TheSizeShouldMatch);

// ---------------------------------------------------------------------------
// For Japanese input method (ibus-mozc)
// ---------------------------------------------------------------------------

const LanguageBooleanPrefs kMozcBooleanPrefs[] = {
  { prefs::kLanguageMozcIncognitoMode,
    false,
    input_method::kMozcIncognitoMode,
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_INCOGNITO_MODE,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageMozcUseAutoImeTurnOff,
    true,
    input_method::kMozcUseAutoImeTurnOff,
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_USE_AUTO_IME_TURN_OFF,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageMozcUseHistorySuggest,
    true,
    input_method::kMozcUseHistorySuggest,
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_USE_HISTORY_SUGGEST,
    PrefServiceSyncable::SYNCABLE_PREF },
  { prefs::kLanguageMozcUseDictionarySuggest,
    true,
    input_method::kMozcUseDictionarySuggest,
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_USE_DICTIONARY_SUGGEST,
    PrefServiceSyncable::SYNCABLE_PREF },
};
COMPILE_ASSERT(kNumMozcBooleanPrefs == arraysize(kMozcBooleanPrefs),
               TheSizeShouldMatch);

extern const LanguageMultipleChoicePreference<const char*>
    kMozcMultipleChoicePrefs[] = {
  { prefs::kLanguageMozcPreeditMethod,
    input_method::kMozcPreeditMethodRoman,
    input_method::kMozcPreeditMethod,
    {{ input_method::kMozcPreeditMethodRoman,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD_ROMAN },
     { input_method::kMozcPreeditMethodKana,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD_KANA }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PREEDIT_METHOD,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcSessionKeymap,
    input_method::kMozcSessionKeymapMsime,
    input_method::kMozcSessionKeymap,
    {{ input_method::kMozcSessionKeymapAtok,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_ATOK },
     { input_method::kMozcSessionKeymapMsime,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_MSIME },
     { input_method::kMozcSessionKeymapKotoeri,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP_KOTOERI }},
    // TODO: Support "CUSTOM" keymap.
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SESSION_KEYMAP,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcPunctuationMethod,
    input_method::kMozcPunctuationMethodKutenTouten,
    input_method::kMozcPunctuationMethod,
    {{ input_method::kMozcPunctuationMethodKutenTouten,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_KUTEN_TOUTEN },
     { input_method::kMozcPunctuationMethodCommaPeriod,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_COMMA_PERIOD },
     { input_method::kMozcPunctuationMethodKutenPeriod,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_KUTEN_PERIOD },
     { input_method::kMozcPunctuationMethodCommaTouten,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD_COMMA_TOUTEN }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_PUNCTUATION_METHOD,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcSymbolMethod,
    input_method::kMozcSymbolMethodCornerBracketMiddleDot,
    input_method::kMozcSymbolMethod,
    {{ input_method::kMozcSymbolMethodCornerBracketMiddleDot,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_CORNER_BRACKET_MIDDLE_DOT },
     { input_method::kMozcSymbolMethodSquareBracketSlash,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_SQUARE_BRACKET_SLASH },
     { input_method::kMozcSymbolMethodCornerBracketSlash,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_CORNER_BRACKET_SLASH },
     { input_method::kMozcSymbolMethodSquareBracketMiddleDot,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD_SQUARE_BRACKET_MIDDLE_DOT }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SYMBOL_METHOD,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcSpaceCharacterForm,
    input_method::kMozcSpaceCharacterFormFundamentalInputMode,
    input_method::kMozcSpaceCharacterForm,
    {{ input_method::kMozcSpaceCharacterFormFundamentalInputMode,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM_FUNDAMENTAL_INPUT_MODE },
     { input_method::kMozcSpaceCharacterFormFundamentalFullWidth,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM_FUNDAMENTAL_FULL_WIDTH },
     { input_method::kMozcSpaceCharacterFormFundamentalHalfWidth,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM_FUNDAMENTAL_HALF_WIDTH }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SPACE_CHARACTER_FORM,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcHistoryLearningLevel,
    input_method::kMozcHistoryLearningLevelDefaultHistory,
    input_method::kMozcHistoryLearningLevel,
    {{ input_method::kMozcHistoryLearningLevelDefaultHistory,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL_DEFAULT_HISTORY },
     { input_method::kMozcHistoryLearningLevelReadOnly,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL_READ_ONLY },
     { input_method::kMozcHistoryLearningLevelNoHistory,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL_NO_HISTORY }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_HISTORY_LEARNING_LEVEL,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  // TODO(mazda): Uncomment this block once the candidate window in Chrome OS
  // supports changing shortcut labels.
  // { prefs::kLanguageMozcSelectionShortcut,
  //   input_method::kMozcSelectionShortcutShortcut123456789,
  //   input_method::kMozcSelectionShortcut,
  //   {{ input_method::kMozcSelectionShortcutNoShortcut,
  //      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT_NO_SHORTCUT },
  //    { input_method::kMozcSelectionShortcutShortcut123456789,
  //      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT_SHORTCUT_123456789 },
  //    { input_method::kMozcSelectionShortcutShortcutAsdfghjkl,
  //      IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT_SHORTCUT_ASDFGHJKL }},
  //   IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SELECTION_SHORTCUT,
  //   PrefServiceSyncable::SYNCABLE_PREF,
  // },
  { prefs::kLanguageMozcShiftKeyModeSwitch,
    input_method::kMozcShiftKeyModeSwitchAsciiInputMode,
    input_method::kMozcShiftKeyModeSwitch,
    {{ input_method::kMozcShiftKeyModeSwitchOff,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH_OFF },
     { input_method::kMozcShiftKeyModeSwitchAsciiInputMode,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH_ASCII_INPUT_MODE },
     { input_method::kMozcShiftKeyModeSwitchKatakanaInputMode,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH_KATAKANA_INPUT_MODE }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_SHIFT_KEY_MODE_SWITCH,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
  { prefs::kLanguageMozcNumpadCharacterForm,
    input_method::kMozcNumpadCharacterFormNumpadHalfWidth,
    input_method::kMozcNumpadCharacterForm,
    {{ input_method::kMozcNumpadCharacterFormNumpadInputMode,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_INPUT_MODE },
     { input_method::kMozcNumpadCharacterFormNumpadFullWidth,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_FULL_WIDTH },
     { input_method::kMozcNumpadCharacterFormNumpadHalfWidth,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_HALF_WIDTH },
     { input_method::kMozcNumpadCharacterFormNumpadDirectInput,
       IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM_NUMPAD_DIRECT_INPUT }},
    IDS_OPTIONS_SETTINGS_LANGUAGES_MOZC_NUMPAD_CHARACTER_FORM,
    PrefServiceSyncable::SYNCABLE_PREF,
  },
};
COMPILE_ASSERT(kNumMozcMultipleChoicePrefs ==
               arraysize(kMozcMultipleChoicePrefs),
               TheSizeShouldMatch);

const LanguageIntegerRangePreference kMozcIntegerPrefs[] = {
  { prefs::kLanguageMozcSuggestionsSize, 3, 1, 9,
    input_method::kMozcSuggestionsSize,
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
