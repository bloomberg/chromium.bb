// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/options/language_config_model.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

TEST(LanguageConfigModelTest, MaybeRewriteLanguageName) {
  EXPECT_EQ(L"English",
            LanguageConfigModel::MaybeRewriteLanguageName(L"English"));
  EXPECT_EQ(l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS),
            LanguageConfigModel::MaybeRewriteLanguageName(L"t"));
}

TEST(LanguageConfigModelTest, GetLanguageDisplayNameFromCode) {
  EXPECT_EQ(L"French",
            LanguageConfigModel::GetLanguageDisplayNameFromCode("fr"));
  // MaybeRewriteLanguageName() should be applied.
  EXPECT_EQ(l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_OTHERS),
            LanguageConfigModel::GetLanguageDisplayNameFromCode("t"));
}

TEST(LanguageConfigModelTest, SortLanguageCodesByNames) {
  std::vector<std::string> language_codes;
  // Check if this function can handle an empty list.
  LanguageConfigModel::SortLanguageCodesByNames(&language_codes);

  language_codes.push_back("ja");
  language_codes.push_back("fr");
  language_codes.push_back("t");
  LanguageConfigModel::SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(3, static_cast<int>(language_codes.size()));
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("t",  language_codes[2]);  // Others

  // Add a duplicate entry and see if it works.
  language_codes.push_back("ja");
  LanguageConfigModel::SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(4, static_cast<int>(language_codes.size()));
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("ja", language_codes[2]);  // Japanese
  ASSERT_EQ("t",  language_codes[3]);  // Others
}

TEST(LanguageConfigModelTest, SortInputMethodIdsByNames) {
  std::map<std::string, std::string> id_to_language_code_map;
  id_to_language_code_map.insert(std::make_pair("mozc", "ja"));
  id_to_language_code_map.insert(std::make_pair("mozc-jp", "ja"));
  id_to_language_code_map.insert(std::make_pair("xkb:jp::jpn", "ja"));
  id_to_language_code_map.insert(std::make_pair("xkb:fr::fra", "fr"));
  id_to_language_code_map.insert(std::make_pair("m17n:latn-pre", "t"));

  std::vector<std::string> input_method_ids;
  // Check if this function can handle an empty list.
  LanguageConfigModel::SortInputMethodIdsByNames(id_to_language_code_map,
                                                 &input_method_ids);

  input_method_ids.push_back("mozc");           // Japanese
  input_method_ids.push_back("xkb:fr::fra");    // French
  input_method_ids.push_back("m17n:latn-pre");  // Others
  LanguageConfigModel::SortInputMethodIdsByNames(id_to_language_code_map,
                                                 &input_method_ids);
  ASSERT_EQ(3, static_cast<int>(input_method_ids.size()));
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("m17n:latn-pre",  input_method_ids[2]);  // Others

  // Add a duplicate entry and see if it works.
  // Note that SortInputMethodIdsByNames uses std::stable_sort.
  input_method_ids.push_back("xkb:jp::jpn");  // also Japanese
  LanguageConfigModel::SortInputMethodIdsByNames(id_to_language_code_map,
                                                 &input_method_ids);
  ASSERT_EQ(4, static_cast<int>(input_method_ids.size()));
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("xkb:jp::jpn", input_method_ids[2]);     // Japanese
  ASSERT_EQ("m17n:latn-pre",  input_method_ids[3]);  // Others

  input_method_ids.push_back("mozc-jp");  // also Japanese
  LanguageConfigModel::SortInputMethodIdsByNames(id_to_language_code_map,
                                                 &input_method_ids);
  ASSERT_EQ(5, static_cast<int>(input_method_ids.size()));
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("xkb:jp::jpn", input_method_ids[2]);     // Japanese
  ASSERT_EQ("mozc-jp", input_method_ids[3]);         // Japanese
  ASSERT_EQ("m17n:latn-pre",  input_method_ids[4]);  // Others
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_DE) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:ch::ger");  // Switzerland - German
  input_method_ids.push_back("xkb:de::ger");  // Germany - German
  LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
      "de", &input_method_ids);
  // The list should be reordered.
  ASSERT_EQ(2, static_cast<int>(input_method_ids.size()));
  EXPECT_EQ("xkb:de::ger", input_method_ids[0]);
  EXPECT_EQ("xkb:ch::ger", input_method_ids[1]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_FR) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:be::fra");  // Belgium - French
  input_method_ids.push_back("xkb:fr::fra");  // France - French
  LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
      "fr", &input_method_ids);
  // The list should be reordered.
  ASSERT_EQ(2, static_cast<int>(input_method_ids.size()));
  EXPECT_EQ("xkb:fr::fra", input_method_ids[0]);
  EXPECT_EQ("xkb:be::fra", input_method_ids[1]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_EN_US) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:us:dvorak:eng");  // US - Dvorak - English
  input_method_ids.push_back("xkb:us::eng");  // US - English
  LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
      "en-US", &input_method_ids);
  // The list should be reordered.
  ASSERT_EQ(2, static_cast<int>(input_method_ids.size()));
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("xkb:us:dvorak:eng", input_method_ids[1]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_FI) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:fi::fin");  // Finland - Finnish
  LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
      "fi", &input_method_ids);
  // There is no rule for reordering for Finnish.
  ASSERT_EQ(1, static_cast<int>(input_method_ids.size()));
  EXPECT_EQ("xkb:fi::fin", input_method_ids[0]);
}

TEST(LanguageConfigModelTest, ReorderInputMethodIdsForLanguageCode_Noop) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:fr::fra");  // France - French
  input_method_ids.push_back("xkb:be::fra");  // Belgium - French
  // If the list is already sorted, nothing should happen.
  LanguageConfigModel::ReorderInputMethodIdsForLanguageCode(
      "fr", &input_method_ids);
  ASSERT_EQ(2, static_cast<int>(input_method_ids.size()));
  EXPECT_EQ("xkb:fr::fra", input_method_ids[0]);
  EXPECT_EQ("xkb:be::fra", input_method_ids[1]);
}

TEST(AddLanguageComboboxModelTest, AddLanguageComboboxModel) {
  std::vector<std::string> language_codes;
  language_codes.push_back("de");
  language_codes.push_back("fr");
  language_codes.push_back("ko");
  AddLanguageComboboxModel model(NULL, language_codes);

  // GetItemCount() should return 4 ("Add language" + 3 language codes).
  ASSERT_EQ(4, model.GetItemCount());

  // The first item should be "Add language" labe.
  EXPECT_EQ(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_ADD_LANGUAGE_COMBOBOX),
            model.GetItemAt(0));
  // Other items should be sorted language display names for UI (hence
  // French comes before German).  Note that the returned display names
  // are followed by their native representations. To simplify matching,
  // use StartsWith() here.
  EXPECT_TRUE(StartsWith(model.GetItemAt(1), L"French", true))
      << model.GetItemAt(1);
  EXPECT_TRUE(StartsWith(model.GetItemAt(2), L"German", true))
      << model.GetItemAt(2);
  EXPECT_TRUE(StartsWith(model.GetItemAt(3), L"Korean", true))
      << model.GetItemAt(3);

  // GetLanguageIndex() returns the given index -1 to offset "Add language".
  EXPECT_EQ(0, model.GetLanguageIndex(1));
  EXPECT_EQ(1, model.GetLanguageIndex(2));
  EXPECT_EQ(2, model.GetLanguageIndex(3));

  // The returned index can be used for GetLocaleFromIndex().
  EXPECT_EQ("fr", model.GetLocaleFromIndex(model.GetLanguageIndex(1)));
  EXPECT_EQ("de", model.GetLocaleFromIndex(model.GetLanguageIndex(2)));
  EXPECT_EQ("ko", model.GetLocaleFromIndex(model.GetLanguageIndex(3)));

  // GetIndexFromLocale() returns the language index.
  EXPECT_EQ(0, model.GetIndexFromLocale("fr"));
  EXPECT_EQ(1, model.GetIndexFromLocale("de"));
  EXPECT_EQ(2, model.GetIndexFromLocale("ko"));
  EXPECT_EQ(-1, model.GetIndexFromLocale("ja"));  // Not in the model.

  // Mark "de" to be ignored, and check if it's gone.
  model.SetIgnored("de", true);
  ASSERT_EQ(3, model.GetItemCount());
  EXPECT_EQ("fr", model.GetLocaleFromIndex(model.GetLanguageIndex(1)));
  EXPECT_EQ("ko", model.GetLocaleFromIndex(model.GetLanguageIndex(2)));

  // Mark "ko" to be ignored, and check if it's gone.
  model.SetIgnored("ko", true);
  ASSERT_EQ(2, model.GetItemCount());
  EXPECT_EQ("fr", model.GetLocaleFromIndex(model.GetLanguageIndex(1)));

  // Mark "fr" to be ignored, and check if it's gone.
  model.SetIgnored("fr", true);
  ASSERT_EQ(1, model.GetItemCount());

  // Mark "de" not to be ignored, and see if it's back.
  model.SetIgnored("de", false);
  ASSERT_EQ(2, model.GetItemCount());
  EXPECT_EQ("de", model.GetLocaleFromIndex(model.GetLanguageIndex(1)));

  // Mark "fr" not to be ignored, and see if it's back.
  model.SetIgnored("fr", false);
  ASSERT_EQ(3, model.GetItemCount());
  EXPECT_EQ("fr", model.GetLocaleFromIndex(model.GetLanguageIndex(1)));
  EXPECT_EQ("de", model.GetLocaleFromIndex(model.GetLanguageIndex(2)));

  // Mark "ko" not to be ignored, and see if it's back.
  model.SetIgnored("ko", false);
  ASSERT_EQ(4, model.GetItemCount());
  EXPECT_EQ("fr", model.GetLocaleFromIndex(model.GetLanguageIndex(1)));
  EXPECT_EQ("de", model.GetLocaleFromIndex(model.GetLanguageIndex(2)));
  EXPECT_EQ("ko", model.GetLocaleFromIndex(model.GetLanguageIndex(3)));

  // Mark "ja" (not in the model) to be ignored.
  model.SetIgnored("ja", true);
  // The GetItemCount() should not be changed.
  ASSERT_EQ(4, model.GetItemCount());
}

}  // namespace chromeos
