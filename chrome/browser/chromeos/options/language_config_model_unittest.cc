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
