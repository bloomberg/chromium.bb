// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONSTANTS_H_
#define CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONSTANTS_H_

// This file defines configuration parameter names for ibus.
namespace chromeos {
namespace input_method {

// ---------------------------------------------------------------------------
// For ibus-daemon
// ---------------------------------------------------------------------------
extern const char kGeneralSectionName[];
extern const char kPreloadEnginesConfigName[];

// ---------------------------------------------------------------------------
// For Traditional Chinese input method (ibus-mozc-chewing)
// ---------------------------------------------------------------------------
extern const char kChewingSectionName[];

// We have to sync the |ibus_config_name|s with those in
// ibus-chewing/files/src/Config.cc.
extern const char kChewingAutoShiftCur[];
extern const char kChewingAddPhraseDirection[];
extern const char kChewingEasySymbolInput[];
extern const char kChewingEscCleanAllBuf[];
extern const char kChewingForceLowercaseEnglish[];
extern const char kChewingPlainZhuyin[];
extern const char kChewingPhraseChoiceRearward[];
extern const char kChewingSpaceAsSelection[];
extern const char kChewingMaxChiSymbolLen[];
extern const char kChewingCandPerPage[];
extern const char kChewingKeyboardType[];
extern const char kChewingKeyboardTypeDefault[];
extern const char kChewingKeyboardTypeHsu[];
extern const char kChewingKeyboardTypeIbm[];
extern const char kChewingKeyboardTypeGinYieh[];
extern const char kChewingKeyboardTypeEten[];
extern const char kChewingKeyboardTypeEten26[];
extern const char kChewingKeyboardTypeDvorak[];
extern const char kChewingKeyboardTypeDvorakHsu[];
extern const char kChewingKeyboardTypeDachen26[];
extern const char kChewingKeyboardTypeHanyu[];

extern const char kChewingSelKeys[];
extern const char kChewingSelKeys1234567890[];
extern const char kChewingSelKeysAsdfghjkl_[];
extern const char kChewingSelKeysAsdfzxcv89[];
extern const char kChewingSelKeysAsdfjkl789[];
extern const char kChewingSelKeysAoeu_qjkix[];
extern const char kChewingSelKeysAoeuhtnsid[];
extern const char kChewingSelKeysAoeuidhtns[];
extern const char kChewingSelKeys1234qweras[];

extern const char kChewingHsuSelKeyType[];
extern const int kChewingHsuSelKeyType1;
extern const int kChewingHsuSelKeyType2;

// ---------------------------------------------------------------------------
// For Korean input method (ibus-mozc-hangul)
// ---------------------------------------------------------------------------
extern const char kHangulSectionName[];
extern const char kHangulKeyboardConfigName[];

extern const char kHangulHanjaBindingKeysConfigName[];
// Mozc-hangul treats Hangul_Hanja key as hanja key event even if it is not set.
// We add Control+9 since F9 key is reserved by the window manager.
// TODO(nona): Hanja keys are not configurable yet (and we're not sure if it
// should.)
extern const char kHangulHanjaBindingKeys[];

// We have to sync the |keyboard_id|s with those in libhangul.
extern const char kHangulKeyboard2Set[];
extern const char kHangulKeyboard3SetFinal[];
extern const char kHangulKeyboard3Set390[];
extern const char kHangulKeyboard3SetNoShift[];
extern const char kHangulKeyboardRomaja[];
// We don't support "Sebeolsik 2 set" keyboard.

// ---------------------------------------------------------------------------
// For Simplified Chinese input method (ibus-mozc-pinyin)
// ---------------------------------------------------------------------------
extern const char kPinyinSectionName[];

// We have to sync the |ibus_config_name|s with those in
// ibus-mozc-pinyin/files/languages/pinyin/unix/ibus/config_updater.cc.
extern const char kPinyinCorrectPinyin[];
extern const char kPinyinFuzzyPinyin[];
extern const char kPinyinShiftSelectCandidate[];
extern const char kPinyinMinusEqualPage[];
extern const char kPinyinCommaPeriodPage[];
extern const char kPinyinAutoCommit[];
extern const char kPinyinDoublePinyin[];
extern const char kPinyinInitChinese[];
extern const char kPinyinInitFull[];
extern const char kPinyinInitFullPunct[];
extern const char kPinyinInitSimplifiedChinese[];
// TODO(yusukes): Support PINYIN_{INCOMPLETE,CORRECT,FUZZY}_... prefs (32
// additional boolean prefs.)
// TODO(yusukes): Support HalfWidthPuncts and IncompletePinyin prefs if needed.

extern const char kPinyinDoublePinyinSchema[];
extern const int kPinyinDoublePinyinSchemaMspy;
extern const int kPinyinDoublePinyinSchemaZrm;
extern const int kPinyinDoublePinyinSchemaAbc;
extern const int kPinyinDoublePinyinSchemaZgpy;
extern const int kPinyinDoublePinyinSchemaPyjj;
extern const char kPinyinLookupTablePageSize[];

// ---------------------------------------------------------------------------
// For Japanese input method (ibus-mozc)
// ---------------------------------------------------------------------------
extern const char kMozcSectionName[];

extern const char kMozcIncognitoMode[];
extern const char kMozcUseAutoImeTurnOff[];
extern const char kMozcUseHistorySuggest[];
extern const char kMozcUseDictionarySuggest[];

extern const char kMozcPreeditMethod[];
extern const char kMozcPreeditMethodRoman[];
extern const char kMozcPreeditMethodKana[];

extern const char kMozcSessionKeymap[];
extern const char kMozcSessionKeymapAtok[];
extern const char kMozcSessionKeymapMsime[];
extern const char kMozcSessionKeymapKotoeri[];

extern const char kMozcPunctuationMethod[];
extern const char kMozcPunctuationMethodKutenTouten[];
extern const char kMozcPunctuationMethodCommaPeriod[];
extern const char kMozcPunctuationMethodKutenPeriod[];
extern const char kMozcPunctuationMethodCommaTouten[];

extern const char kMozcSymbolMethod[];
extern const char kMozcSymbolMethodCornerBracketMiddleDot[];
extern const char kMozcSymbolMethodSquareBracketSlash[];
extern const char kMozcSymbolMethodCornerBracketSlash[];
extern const char kMozcSymbolMethodSquareBracketMiddleDot[];

extern const char kMozcSpaceCharacterForm[];
extern const char kMozcSpaceCharacterFormFundamentalInputMode[];
extern const char kMozcSpaceCharacterFormFundamentalFullWidth[];
extern const char kMozcSpaceCharacterFormFundamentalHalfWidth[];

extern const char kMozcHistoryLearningLevel[];
extern const char kMozcHistoryLearningLevelDefaultHistory[];
extern const char kMozcHistoryLearningLevelReadOnly[];
extern const char kMozcHistoryLearningLevelNoHistory[];

extern const char kMozcSelectionShortcut[];
extern const char kMozcSelectionShortcutNoShortcut[];
extern const char kMozcSelectionShortcutShortcut123456789[];
extern const char kMozcSelectionShortcutShortcutAsdfghjkl[];


extern const char kMozcShiftKeyModeSwitch[];
extern const char kMozcShiftKeyModeSwitchOff[];
extern const char kMozcShiftKeyModeSwitchAsciiInputMode[];
extern const char kMozcShiftKeyModeSwitchKatakanaInputMode[];

extern const char kMozcNumpadCharacterForm[];
extern const char kMozcNumpadCharacterFormNumpadInputMode[];
extern const char kMozcNumpadCharacterFormNumpadFullWidth[];
extern const char kMozcNumpadCharacterFormNumpadHalfWidth[];
extern const char kMozcNumpadCharacterFormNumpadDirectInput[];

extern const char kMozcSuggestionsSize[];

}  // input_method
}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_INPUT_METHOD_INPUT_METHOD_CONSTANTS_H_
