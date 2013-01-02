// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_constants.h"

namespace chromeos {
namespace input_method {

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
const char kChewingAutoShiftCur[] = "autoShiftCur";
const char kChewingAddPhraseDirection[] = "addPhraseDirection";
const char kChewingEasySymbolInput[] = "easySymbolInput";
const char kChewingEscCleanAllBuf[] = "escCleanAllBuf";
const char kChewingForceLowercaseEnglish[] = "forceLowercaseEnglish";
const char kChewingPlainZhuyin[] = "plainZhuyin";
const char kChewingPhraseChoiceRearward[] = "phraseChoiceRearward";
const char kChewingSpaceAsSelection[] = "spaceAsSelection";
const char kChewingMaxChiSymbolLen[] = "maxChiSymbolLen";
const char kChewingCandPerPage[] = "candPerPage";
const char kChewingKeyboardType[] = "KBType";
const char kChewingKeyboardTypeDefault[] = "default";
const char kChewingKeyboardTypeHsu[] = "hsu";
const char kChewingKeyboardTypeIbm[] = "ibm";
const char kChewingKeyboardTypeGinYieh[] = "gin_yieh";
const char kChewingKeyboardTypeEten[] = "eten";
const char kChewingKeyboardTypeEten26[] = "eten26";
const char kChewingKeyboardTypeDvorak[] = "dvorak";
const char kChewingKeyboardTypeDvorakHsu[] = "dvorak_hsu";
const char kChewingKeyboardTypeDachen26[] = "dachen_26";
const char kChewingKeyboardTypeHanyu[] = "hanyu";

const char kChewingSelKeys[] = "selKeys";
const char kChewingSelKeys1234567890[] = "1234567890";
const char kChewingSelKeysAsdfghjkl_[] = "asdfghjkl;";
const char kChewingSelKeysAsdfzxcv89[] = "asdfzxcv89";
const char kChewingSelKeysAsdfjkl789[] = "asdfjkl789";
const char kChewingSelKeysAoeu_qjkix[] = "aoeu;qjkix";
const char kChewingSelKeysAoeuhtnsid[] = "aoeuhtnsid";
const char kChewingSelKeysAoeuidhtns[] = "aoeuidhtns";
const char kChewingSelKeys1234qweras[] = "1234qweras";

const char kChewingHsuSelKeyType[] = "hsuSelKeyType";
const int kChewingHsuSelKeyType1 = 1;
const int kChewingHsuSelKeyType2 = 2;

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

// We have to sync the |keyboard_id|s with those in libhangul.
const char kHangulKeyboard2Set[] = "2";
const char kHangulKeyboard3SetFinal[] = "3f";
const char kHangulKeyboard3Set390[] = "39";
const char kHangulKeyboard3SetNoShift[] = "3s";
const char kHangulKeyboardRomaja[] = "ro";
// We don't support "Sebeolsik 2 set" keyboard.

// ---------------------------------------------------------------------------
// For Simplified Chinese input method (ibus-mozc-pinyin)
// ---------------------------------------------------------------------------
const char kPinyinSectionName[] = "engine/Pinyin";

// We have to sync the |ibus_config_name|s with those in
// ibus-mozc-pinyin/files/languages/pinyin/unix/ibus/config_updater.cc.
const char kPinyinCorrectPinyin[] = "CorrectPinyin";
const char kPinyinFuzzyPinyin[] = "FuzzyPinyin";
const char kPinyinShiftSelectCandidate[] = "ShiftSelectCandidate";
const char kPinyinMinusEqualPage[] = "MinusEqualPage";
const char kPinyinCommaPeriodPage[] = "CommaPeriodPage";
const char kPinyinAutoCommit[] = "AutoCommit";
const char kPinyinDoublePinyin[] = "DoublePinyin";
const char kPinyinInitChinese[] = "InitChinese";
const char kPinyinInitFull[] = "InitFull";
const char kPinyinInitFullPunct[] = "InitFullPunct";
const char kPinyinInitSimplifiedChinese[] = "InitSimplifiedChinese";
// TODO(yusukes): Support PINYIN_{INCOMPLETE,CORRECT,FUZZY}_... prefs (32
// additional boolean prefs.)
// TODO(yusukes): Support HalfWidthPuncts and IncompletePinyin prefs if needed.

const char kPinyinDoublePinyinSchema[] = "DoublePinyinSchema";
const int kPinyinDoublePinyinSchemaMspy = 0;
const int kPinyinDoublePinyinSchemaZrm = 1;
const int kPinyinDoublePinyinSchemaAbc = 2;
const int kPinyinDoublePinyinSchemaZgpy = 3;
const int kPinyinDoublePinyinSchemaPyjj = 4;
const char kPinyinLookupTablePageSize[] = "LookupTablePageSize";

// ---------------------------------------------------------------------------
// For Japanese input method (ibus-mozc)
// ---------------------------------------------------------------------------
const char kMozcSectionName[] = "engine/Mozc";

const char kMozcIncognitoMode[] = "incognito_mode";
const char kMozcUseAutoImeTurnOff[] = "use_auto_ime_turn_off";
const char kMozcUseHistorySuggest[] = "use_history_suggest";
const char kMozcUseDictionarySuggest[] = "use_dictionary_suggest";

const char kMozcPreeditMethod[] = "preedit_method";
const char kMozcPreeditMethodRoman[] = "ROMAN";
const char kMozcPreeditMethodKana[] = "KANA";

const char kMozcSessionKeymap[] = "session_keymap";
const char kMozcSessionKeymapAtok[] = "ATOK";
const char kMozcSessionKeymapMsime[] = "MSIME";
const char kMozcSessionKeymapKotoeri[] = "KOTOERI";

const char kMozcPunctuationMethod[] = "punctuation_method";
const char kMozcPunctuationMethodKutenTouten[] = "KUTEN_TOUTEN";
const char kMozcPunctuationMethodCommaPeriod[] = "COMMA_PERIOD";
const char kMozcPunctuationMethodKutenPeriod[] = "KUTEN_PERIOD";
const char kMozcPunctuationMethodCommaTouten[] = "COMMA_TOUTEN";

const char kMozcSymbolMethod[] = "symbol_method";
const char kMozcSymbolMethodCornerBracketMiddleDot[] =
    "CORNER_BRACKET_MIDDLE_DOT";
const char kMozcSymbolMethodSquareBracketSlash[] = "SQUARE_BRACKET_SLASH";
const char kMozcSymbolMethodCornerBracketSlash[] = "CORNER_BRACKET_SLASH";
const char kMozcSymbolMethodSquareBracketMiddleDot[] =
    "SQUARE_BRACKET_MIDDLE_DOT";

const char kMozcSpaceCharacterForm[] = "space_character_form";
const char kMozcSpaceCharacterFormFundamentalInputMode[] =
    "FUNDAMENTAL_INPUT_MODE";
const char kMozcSpaceCharacterFormFundamentalFullWidth[] =
    "FUNDAMENTAL_FULL_WIDTH";
const char kMozcSpaceCharacterFormFundamentalHalfWidth[] =
    "FUNDAMENTAL_HALF_WIDTH";

const char kMozcHistoryLearningLevel[] = "history_learning_level";
const char kMozcHistoryLearningLevelDefaultHistory[] = "DEFAULT_HISTORY";
const char kMozcHistoryLearningLevelReadOnly[] = "READ_ONLY";
const char kMozcHistoryLearningLevelNoHistory[] = "NO_HISTORY";

const char kMozcSelectionShortcut[] = "selection_shortcut";
const char kMozcSelectionShortcutNoShortcut[] = "NO_SHORTCUT";
const char kMozcSelectionShortcutShortcut123456789[] = "SHORTCUT_123456789";
const char kMozcSelectionShortcutShortcutAsdfghjkl[] = "SHORTCUT_ASDFGHJKL";


const char kMozcShiftKeyModeSwitch[] = "shift_key_mode_switch";
const char kMozcShiftKeyModeSwitchOff[] = "OFF";
const char kMozcShiftKeyModeSwitchAsciiInputMode[] = "ASCII_INPUT_MODE";
const char kMozcShiftKeyModeSwitchKatakanaInputMode[] = "KATAKANA_INPUT_MODE";

const char kMozcNumpadCharacterForm[] = "numpad_character_form";
const char kMozcNumpadCharacterFormNumpadInputMode[] = "NUMPAD_INPUT_MODE";
const char kMozcNumpadCharacterFormNumpadFullWidth[] = "NUMPAD_FULL_WIDTH";
const char kMozcNumpadCharacterFormNumpadHalfWidth[] = "NUMPAD_HALF_WIDTH";
const char kMozcNumpadCharacterFormNumpadDirectInput[] = "NUMPAD_DIRECT_INPUT";

const char kMozcSuggestionsSize[] = "suggestions_size";

}  // namespace input_method
}  // namespace chromeos
