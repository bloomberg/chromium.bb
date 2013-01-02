// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
#define CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_

#include <stddef.h>  // For size_t

#include "chrome/browser/prefs/pref_service.h"

// TODO(yusukes): Rename this file to input_method_preference.cc. Since
// "language" usually means UI language, the current file name is confusing.
// The namespace should also be changed to "namespace input_method {".

// This file defines types and declare variables used in "Languages and
// Input" settings in Chromium OS.
namespace chromeos {
namespace language_prefs {

// TODO(yusukes): Remove the "Language" prefix from all structs and variables.
// They're redundant (we already have the language_prefs namespace) and even
// confusing.

// The struct is used for preferences consisting of multiple choices, like
// punctuation types used in Japanese input method.
template <typename DataType>
struct LanguageMultipleChoicePreference {
  const char* pref_name;  // Chrome preference name.
  DataType default_pref_value;
  const char* ibus_config_name;
  // Currently we have 10 combobox items at most.
  static const size_t kMaxItems = 11;
  struct {
    DataType ibus_config_value;
    int item_message_id;  // Resource grd ID for the combobox item.
  } values_and_ids[kMaxItems];
  int label_message_id;  // Resource grd ID for the label.
  PrefServiceSyncable::PrefSyncStatus sync_status;
};

// The struct is used for preferences of boolean values, like switches to
// enable or disable particular features.
struct LanguageBooleanPrefs {
  const char* pref_name;  // Chrome preference name.
  bool default_pref_value;
  const char* ibus_config_name;
  int message_id;
  PrefServiceSyncable::PrefSyncStatus sync_status;
};

// The struct is used for preferences of integer range values, like the
// key repeat rate.
struct LanguageIntegerRangePreference {
  const char* pref_name;  // Chrome preference name.
  int default_pref_value;
  int min_pref_value;
  int max_pref_value;
  const char* ibus_config_name;
  int message_id;
  PrefServiceSyncable::PrefSyncStatus sync_status;
};

extern const LanguageBooleanPrefs kChewingBooleanPrefs[];
// This is not ideal, but we should hard-code the number here as the value
// is referenced in other header files as array sizes. We have a
// COMPILE_ASSERT in .cc to ensure that the number is correct.
const size_t kNumChewingBooleanPrefs = 8 - 2; // -2 is for crosbug.com/14185

extern const LanguageIntegerRangePreference kChewingIntegerPrefs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumChewingIntegerPrefs = 2;
const int kChewingMaxChiSymbolLenIndex = 0;
const int kChewingCandPerPageIndex = 1;

extern const LanguageMultipleChoicePreference<const char*>
    kChewingMultipleChoicePrefs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumChewingMultipleChoicePrefs = 2;

extern const LanguageMultipleChoicePreference<int> kChewingHsuSelKeyType;

// ---------------------------------------------------------------------------
// For Korean input method (ibus-mozc-hangul)
// ---------------------------------------------------------------------------

struct HangulKeyboardNameIDPair {
  int message_id;
  const char* keyboard_id;
};

extern const HangulKeyboardNameIDPair kHangulKeyboardNameIDPairs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumHangulKeyboardNameIDPairs = 5;

// ---------------------------------------------------------------------------
// For Simplified Chinese input method (ibus-mozc-pinyin)
// ---------------------------------------------------------------------------

extern const LanguageBooleanPrefs kPinyinBooleanPrefs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumPinyinBooleanPrefs = 11;

extern const LanguageMultipleChoicePreference<int> kPinyinDoublePinyinSchema;

struct PinyinIntegerPref {
  const char* pref_name;  // Chrome preference name.
  int default_pref_value;
  const char* ibus_config_name;
  PrefServiceSyncable::PrefSyncStatus sync_status;
  // TODO(yusukes): Add message_id if needed.
};

extern const PinyinIntegerPref kPinyinIntegerPrefs[];
const size_t kNumPinyinIntegerPrefs = 1;

// ---------------------------------------------------------------------------
// For Japanese input method (ibus-mozc)
// ---------------------------------------------------------------------------

extern const LanguageBooleanPrefs kMozcBooleanPrefs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumMozcBooleanPrefs = 4;

extern const LanguageMultipleChoicePreference<const char*>
    kMozcMultipleChoicePrefs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumMozcMultipleChoicePrefs = 8;

extern const LanguageIntegerRangePreference kMozcIntegerPrefs[];
// See comments at kNumChewingBooleanPrefs for why we hard-code this here.
const size_t kNumMozcIntegerPrefs = 1;

// ---------------------------------------------------------------------------
// For keyboard stuff
// ---------------------------------------------------------------------------
// A delay between the first and the start of the rest.
extern const int kXkbAutoRepeatDelayInMs;
// An interval between the repeated keys.
extern const int kXkbAutoRepeatIntervalInMs;

// A string Chrome preference (Local State) of the preferred keyboard layout in
// the login screen.
extern const char kPreferredKeyboardLayout[];

// Registers non-user prefs for the default keyboard layout on the login screen.
void RegisterPrefs(PrefServiceSimple* local_state);

}  // language_prefs
}  // chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LANGUAGE_PREFERENCES_H_
