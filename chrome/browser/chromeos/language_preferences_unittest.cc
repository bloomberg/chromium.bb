// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/language_preferences.h"

#include <cstring>
#include <set>

#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace language_prefs {

namespace {

// Compares |a| and |b| and returns true if a is equal to b. The second one is a
// specialized function for LanguageMultipleChoicePreference<const char*>.
template <typename T>
bool Compare(T a, T b) {
  return a == b;
}
template <>
bool Compare<const char*>(const char* a, const char* b) {
  return !std::strcmp(a, b);
}

// Returns false if one or more prefs have a bad |default_pref_value|.
// C++ template is used here since LanguageMultipleChoicePreference is a class
// template.
template <typename T>
bool CheckDefaultValueOfMultipleChoicePrefs(
    const LanguageMultipleChoicePreference<T>* prefs, size_t prefs_len) {
  const size_t kMaxItems = LanguageMultipleChoicePreference<T>::kMaxItems;
  for (size_t i = 0; i < prefs_len; ++i) {
    bool default_value_is_valid = false;
    for (size_t j = 0; j < kMaxItems; ++j) {
      const LanguageMultipleChoicePreference<T>& pref = prefs[i];
      if (pref.values_and_ids[j].item_message_id == 0) {
        break;
      }
      if (Compare(pref.default_pref_value,
                  pref.values_and_ids[j].ibus_config_value)) {
        default_value_is_valid = true;
      }
    }
    if (!default_value_is_valid) {
      return false;
    }
  }
  return true;
}

// Returns false if |prefs| has duplicated |ibus_config_value| or
// |item_message_id|.
template <typename T>
bool CheckDuplicationOfMultipleChoicePrefs(
    const LanguageMultipleChoicePreference<T>* prefs, size_t prefs_len) {
  const size_t kMaxItems = LanguageMultipleChoicePreference<T>::kMaxItems;
  for (size_t i = 0; i < prefs_len; ++i) {
    std::set<T> ibus_config_value_set;
    std::set<int> item_message_id_set;
    for (size_t j = 0; j < kMaxItems; ++j) {
      const LanguageMultipleChoicePreference<T>& pref = prefs[i];
      if (pref.values_and_ids[j].item_message_id == 0) {
        break;
      }
      const T ibus_config_value = pref.values_and_ids[j].ibus_config_value;
      if (!ibus_config_value_set.insert(ibus_config_value).second) {
        // |ibus_config_value| is already in the set.
        return false;
      }
      const int item_message_id = pref.values_and_ids[j].item_message_id;
      if (!item_message_id_set.insert(item_message_id).second) {
        // |item_message_id| is already in the set.
        return false;
      }
    }
  }
  return true;
}

// Returns false if one or more prefs have an out-of-range |default_pref_value|.
bool CheckDefaultValueOfIntegerRangePrefs(
    const LanguageIntegerRangePreference* prefs, size_t prefs_len) {
  for (size_t i = 0; i < prefs_len; ++i) {
    const LanguageIntegerRangePreference& pref = prefs[i];
    if (pref.default_pref_value < pref.min_pref_value) {
      return false;
    }
    if (pref.default_pref_value > pref.max_pref_value) {
      return false;
    }
  }
  return true;
}

}  // namespace

// Checks |default_pref_value| in LanguageMultipleChoicePreference prefs.
TEST(LanguagePreferencesTest, TestDefaultValuesOfMultipleChoicePrefs) {
  EXPECT_TRUE(CheckDefaultValueOfMultipleChoicePrefs(
      kChewingMultipleChoicePrefs, kNumChewingMultipleChoicePrefs));
  EXPECT_TRUE(CheckDefaultValueOfMultipleChoicePrefs(
      &kXkbModifierMultipleChoicePrefs, 1));
  EXPECT_TRUE(CheckDefaultValueOfMultipleChoicePrefs(
      &kChewingHsuSelKeyType, 1));
  EXPECT_TRUE(CheckDefaultValueOfMultipleChoicePrefs(
      &kPinyinDoublePinyinSchema, 1));
  EXPECT_TRUE(CheckDefaultValueOfMultipleChoicePrefs(
      kMozcMultipleChoicePrefs, kNumMozcMultipleChoicePrefs));
}

// Checks |ibus_config_value| and |item_message_id| duplications in
// LanguageMultipleChoicePreference prefs.
TEST(LanguagePreferencesTest, TestDuplicationOfMultipleChoicePrefs) {
  EXPECT_TRUE(CheckDuplicationOfMultipleChoicePrefs(
      kChewingMultipleChoicePrefs, kNumChewingMultipleChoicePrefs));
  EXPECT_TRUE(CheckDuplicationOfMultipleChoicePrefs(
      &kXkbModifierMultipleChoicePrefs, 1));
  EXPECT_TRUE(CheckDuplicationOfMultipleChoicePrefs(
      &kChewingHsuSelKeyType, 1));
  EXPECT_TRUE(CheckDuplicationOfMultipleChoicePrefs(
      &kPinyinDoublePinyinSchema, 1));
  EXPECT_TRUE(CheckDuplicationOfMultipleChoicePrefs(
      kMozcMultipleChoicePrefs, kNumMozcMultipleChoicePrefs));
}

// Checks |default_pref_value| in LanguageIntegerRangePreference prefs.
TEST(LanguagePreferencesTest, TestDefaultValuesOfIntegerRangePrefs) {
  EXPECT_TRUE(CheckDefaultValueOfIntegerRangePrefs(
      kChewingIntegerPrefs, kNumChewingIntegerPrefs));
  EXPECT_TRUE(CheckDefaultValueOfIntegerRangePrefs(
      kMozcIntegerPrefs, kNumMozcIntegerPrefs));
}

}  // namespace language_prefs
}  // namespace chromeos
