// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"

#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace options {

class CrosLanguageOptionsHandlerTest : public testing::Test {
 public:
  CrosLanguageOptionsHandlerTest()
      : input_manager_(new MockInputMethodManagerWithInputMethods) {
    chromeos::input_method::InitializeForTesting(input_manager_);
  }

  virtual ~CrosLanguageOptionsHandlerTest() {
    chromeos::input_method::Shutdown();
  }

  // testing::Test:
  virtual void SetUp() OVERRIDE {
    input_manager_->AddInputMethod("xkb:us::eng", "us", "en-US");
    input_manager_->AddInputMethod("xkb:fr::fra", "fr", "fr");
    input_manager_->AddInputMethod("xkb:be::fra", "be", "fr");
    input_manager_->AddInputMethod("xkb:is::ice", "is", "is");
  }

 private:
  MockInputMethodManagerWithInputMethods* input_manager_;
};

TEST_F(CrosLanguageOptionsHandlerTest, GetInputMethodList) {
  scoped_ptr<base::ListValue> list(
    CrosLanguageOptionsHandler::GetInputMethodList());
  ASSERT_EQ(4U, list->GetSize());

  base::DictionaryValue* entry = NULL;
  base::DictionaryValue *language_code_set = NULL;
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
  EXPECT_EQ("xkb:is::ice", input_method_id);
  // Commented out. See above.
  // EXPECT_EQ("Japanese input method (for US keyboard)", display_name);
  ASSERT_TRUE(language_code_set->HasKey("is"));
}

}  // namespace options
}  // namespace chromeos
