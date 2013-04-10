// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler.h"

#include <string>

#include "base/values.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chromeos/ime/input_method_descriptor.h"

using chromeos::input_method::InputMethodDescriptor;
using chromeos::input_method::InputMethodDescriptors;
using chromeos::input_method::MockInputMethodManager;

namespace {

class LanguageOptionsHandlerTest : public testing::Test {
 public:
  LanguageOptionsHandlerTest() {
    chromeos::input_method::InitializeForTesting(new MockInputMethodManager);
  }
  virtual ~LanguageOptionsHandlerTest() {
    chromeos::input_method::Shutdown();
  }

 protected:
  InputMethodDescriptors CreateInputMethodDescriptors() {
    InputMethodDescriptors descriptors;
    descriptors.push_back(GetDesc("xkb:us::eng", "us", "en-US"));
    descriptors.push_back(GetDesc("xkb:fr::fra", "fr", "fr"));
    descriptors.push_back(GetDesc("xkb:be::fra", "be", "fr"));
    descriptors.push_back(GetDesc("mozc", "us", "ja"));
    return descriptors;
  }

 private:
  InputMethodDescriptor GetDesc(const std::string& id,
                                const std::string& raw_layout,
                                const std::string& language_code) {
    return InputMethodDescriptor(id,
                                 "",  // name
                                 raw_layout,
                                 language_code,
                                 false);
  }
};

}  // namespace
#else
typedef testing::Test LanguageOptionsHandlerTest;
#endif

#if defined(OS_CHROMEOS)
TEST_F(LanguageOptionsHandlerTest, GetInputMethodList) {
  InputMethodDescriptors descriptors = CreateInputMethodDescriptors();
  scoped_ptr<ListValue> list(
      chromeos::options::CrosLanguageOptionsHandler::GetInputMethodList(
          descriptors));
  ASSERT_EQ(4U, list->GetSize());

  DictionaryValue* entry = NULL;
  DictionaryValue *language_code_set = NULL;
  std::string input_method_id;
  std::string display_name;
  std::string language_code;

  // As shown below, the list should be input method ids should appear in
  // the same order of the descriptors.
  ASSERT_TRUE(list->GetDictionary(0, &entry));
  ASSERT_TRUE(entry->GetString("id", &input_method_id));
  ASSERT_TRUE(entry->GetString("displayName", &display_name));
  ASSERT_TRUE(entry->GetDictionary("languageCodeSet", &language_code_set));
  EXPECT_EQ("xkb:us::eng", input_method_id);
  // Commented out as it depends on translation in generated_resources.grd
  // (i.e. makes the test fragile).
  // EXPECT_EQ("English (USA) keyboard layout", display_name);
  ASSERT_TRUE(language_code_set->HasKey("en-US"));
  ASSERT_TRUE(language_code_set->HasKey("id"));  // From kExtraLanguages.
  ASSERT_TRUE(language_code_set->HasKey("fil"));  // From kExtraLanguages.

  ASSERT_TRUE(list->GetDictionary(1, &entry));
  ASSERT_TRUE(entry->GetString("id", &input_method_id));
  ASSERT_TRUE(entry->GetString("displayName", &display_name));
  ASSERT_TRUE(entry->GetDictionary("languageCodeSet", &language_code_set));
  EXPECT_EQ("xkb:fr::fra", input_method_id);
  // Commented out. See above.
  // EXPECT_EQ("French keyboard layout", display_name);
  ASSERT_TRUE(language_code_set->HasKey("fr"));

  ASSERT_TRUE(list->GetDictionary(2, &entry));
  ASSERT_TRUE(entry->GetString("id", &input_method_id));
  ASSERT_TRUE(entry->GetString("displayName", &display_name));
  ASSERT_TRUE(entry->GetDictionary("languageCodeSet", &language_code_set));
  EXPECT_EQ("xkb:be::fra", input_method_id);
  // Commented out. See above.
  // EXPECT_EQ("Belgian keyboard layout", display_name);
  ASSERT_TRUE(language_code_set->HasKey("fr"));

  ASSERT_TRUE(list->GetDictionary(3, &entry));
  ASSERT_TRUE(entry->GetString("id", &input_method_id));
  ASSERT_TRUE(entry->GetString("displayName", &display_name));
  ASSERT_TRUE(entry->GetDictionary("languageCodeSet", &language_code_set));
  EXPECT_EQ("mozc", input_method_id);
  // Commented out. See above.
  // EXPECT_EQ("Japanese input method (for US keyboard)", display_name);
  ASSERT_TRUE(language_code_set->HasKey("ja"));
}
#endif  // defined(OS_CHROMEOS)

#if !defined(OS_MACOSX)
TEST_F(LanguageOptionsHandlerTest, GetUILanguageCodeSet) {
  scoped_ptr<DictionaryValue> dictionary(
      options::LanguageOptionsHandler::GetUILanguageCodeSet());
  EXPECT_TRUE(dictionary->HasKey("en-US"));
  // Note that we don't test a false case, as such an expectation will
  // fail when we add support for the language.
  // EXPECT_FALSE(dictionary->HasKey("no"));
}
#endif  // !defined(OS_MACOSX)

TEST_F(LanguageOptionsHandlerTest, GetSpellCheckLanguageCodeSet) {
  scoped_ptr<DictionaryValue> dictionary(
      options::LanguageOptionsHandler::GetSpellCheckLanguageCodeSet());
  EXPECT_TRUE(dictionary->HasKey("en-US"));
}
