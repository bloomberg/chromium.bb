// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_util.h"

#include <string>

#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {
namespace input_method {

class InputMethodUtilTest : public testing::Test {
 public:
  static void SetUpTestCase() {
    // Reload the internal maps before running tests, with the stub
    // libcros enabled, so that test data is loaded properly.
    ScopedStubCrosEnabler stub_cros_enabler;
    ReloadInternalMaps();
  }

 private:
  // Ensure we always use the stub libcros in each test.
  ScopedStubCrosEnabler stub_cros_enabler_;
};

TEST_F(InputMethodUtilTest, GetStringUTF8) {
  EXPECT_EQ("Pinyin input method",
            GetStringUTF8("Pinyin"));
  EXPECT_EQ("Japanese input method (for US Dvorak keyboard)",
            GetStringUTF8("Mozc (US Dvorak keyboard layout)"));
  EXPECT_EQ("Google Japanese Input (for US Dvorak keyboard)",
            GetStringUTF8("Google Japanese Input (US Dvorak keyboard layout)"));
}

TEST_F(InputMethodUtilTest, StringIsSupported) {
  EXPECT_TRUE(StringIsSupported("Hiragana"));
  EXPECT_TRUE(StringIsSupported("Latin"));
  EXPECT_TRUE(StringIsSupported("Direct input"));
  EXPECT_FALSE(StringIsSupported("####THIS_STRING_IS_NOT_SUPPORTED####"));
}

TEST_F(InputMethodUtilTest, NormalizeLanguageCode) {
  // TODO(yusukes): test all language codes that IBus provides.
  EXPECT_EQ("ja", NormalizeLanguageCode("ja"));
  EXPECT_EQ("ja", NormalizeLanguageCode("jpn"));
  // In the past "t" had a meaning of "other language" for some m17n latin
  // input methods for testing purpose, but it is no longer used. We test "t"
  // here as just an "unknown" language.
  EXPECT_EQ("t", NormalizeLanguageCode("t"));
  EXPECT_EQ("zh-CN", NormalizeLanguageCode("zh-CN"));
  EXPECT_EQ("zh-CN", NormalizeLanguageCode("zh_CN"));
  EXPECT_EQ("en-US", NormalizeLanguageCode("EN_us"));
  // See app/l10n_util.cc for es-419.
  EXPECT_EQ("es-419", NormalizeLanguageCode("es_419"));

  // Special three-letter language codes.
  EXPECT_EQ("cs", NormalizeLanguageCode("cze"));
  EXPECT_EQ("de", NormalizeLanguageCode("ger"));
  EXPECT_EQ("el", NormalizeLanguageCode("gre"));
  EXPECT_EQ("hr", NormalizeLanguageCode("scr"));
  EXPECT_EQ("ro", NormalizeLanguageCode("rum"));
  EXPECT_EQ("sk", NormalizeLanguageCode("slo"));
}

TEST_F(InputMethodUtilTest, IsKeyboardLayout) {
  EXPECT_TRUE(IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(IsKeyboardLayout("anthy"));
}

TEST_F(InputMethodUtilTest, GetLanguageCodeFromDescriptor) {
  EXPECT_EQ("ja", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("anthy", "Anthy", "us", "ja")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("chewing", "Chewing", "us", "zh")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("bopomofo", "Bopomofo(Zhuyin)", "us", "zh")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("m17n:zh:cangjie", "Cangjie", "us", "zh")));
  EXPECT_EQ("zh-TW", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("m17n:zh:quick", "Quick", "us", "zh")));
  EXPECT_EQ("zh-CN", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("pinyin", "Pinyin", "us", "zh")));
  EXPECT_EQ("en-US", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:us::eng", "USA", "us", "eng")));
  EXPECT_EQ("en-UK", GetLanguageCodeFromDescriptor(
      InputMethodDescriptor("xkb:uk::eng", "United Kingdom", "us", "eng")));
}

TEST_F(InputMethodUtilTest, GetKeyboardLayoutName) {
  // Unsupported case.
  EXPECT_EQ("", GetKeyboardLayoutName("UNSUPPORTED_ID"));

  // Supported cases (samples).
  EXPECT_EQ("jp", GetKeyboardLayoutName("mozc-jp"));
  EXPECT_EQ("us", GetKeyboardLayoutName("pinyin"));
  EXPECT_EQ("us", GetKeyboardLayoutName("m17n:ar:kbd"));
  EXPECT_EQ("es", GetKeyboardLayoutName("xkb:es::spa"));
  EXPECT_EQ("es(cat)", GetKeyboardLayoutName("xkb:es:cat:cat"));
  EXPECT_EQ("gb(extd)", GetKeyboardLayoutName("xkb:gb:extd:eng"));
  EXPECT_EQ("us", GetKeyboardLayoutName("xkb:us::eng"));
  EXPECT_EQ("us(dvorak)", GetKeyboardLayoutName("xkb:us:dvorak:eng"));
  EXPECT_EQ("us(colemak)", GetKeyboardLayoutName("xkb:us:colemak:eng"));
}

TEST_F(InputMethodUtilTest, GetLanguageCodeFromInputMethodId) {
  // Make sure that the -CN is added properly.
  EXPECT_EQ("zh-CN", GetLanguageCodeFromInputMethodId("pinyin"));
}

TEST_F(InputMethodUtilTest, GetInputMethodDisplayNameFromId) {
  EXPECT_EQ("Pinyin input method", GetInputMethodDisplayNameFromId("pinyin"));
  EXPECT_EQ("English (United States)",
            GetInputMethodDisplayNameFromId("xkb:us::eng"));
  EXPECT_EQ("", GetInputMethodDisplayNameFromId("nonexistent"));
}

TEST_F(InputMethodUtilTest, GetInputMethodDescriptorFromId) {
  EXPECT_EQ(NULL, GetInputMethodDescriptorFromId("non_existent"));

  const InputMethodDescriptor* descriptor =
      GetInputMethodDescriptorFromId("pinyin");
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  EXPECT_EQ("pinyin", descriptor->id);
  EXPECT_EQ("Pinyin", descriptor->display_name);
  EXPECT_EQ("us", descriptor->keyboard_layout);
  // This is not zh-CN as the language code in InputMethodDescriptor is
  // not normalized to our format. The normalization is done in
  // GetLanguageCodeFromDescriptor().
  EXPECT_EQ("zh", descriptor->language_code);
}

TEST_F(InputMethodUtilTest, GetLanguageNativeDisplayNameFromCode) {
  EXPECT_EQ(UTF8ToUTF16("suomi"), GetLanguageNativeDisplayNameFromCode("fi"));
}

TEST_F(InputMethodUtilTest, SortLanguageCodesByNames) {
  std::vector<std::string> language_codes;
  // Check if this function can handle an empty list.
  SortLanguageCodesByNames(&language_codes);

  language_codes.push_back("ja");
  language_codes.push_back("fr");
  // For "t", see the comment in NormalizeLanguageCode test.
  language_codes.push_back("t");
  SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(3U, language_codes.size());
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("t",  language_codes[2]);  // Others

  // Add a duplicate entry and see if it works.
  language_codes.push_back("ja");
  SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(4U, language_codes.size());
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("ja", language_codes[2]);  // Japanese
  ASSERT_EQ("t",  language_codes[3]);  // Others
}

TEST_F(InputMethodUtilTest, SortInputMethodIdsByNamesInternal) {
  std::map<std::string, std::string> id_to_language_code_map;
  id_to_language_code_map.insert(std::make_pair("mozc", "ja"));
  id_to_language_code_map.insert(std::make_pair("mozc-jp", "ja"));
  id_to_language_code_map.insert(std::make_pair("xkb:jp::jpn", "ja"));
  id_to_language_code_map.insert(std::make_pair("xkb:fr::fra", "fr"));

  std::vector<std::string> input_method_ids;
  // Check if this function can handle an empty list.
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);

  input_method_ids.push_back("mozc");           // Japanese
  input_method_ids.push_back("xkb:fr::fra");    // French
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese

  // Add a duplicate entry and see if it works.
  // Note that SortInputMethodIdsByNamesInternal uses std::stable_sort.
  input_method_ids.push_back("xkb:jp::jpn");  // also Japanese
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);
  ASSERT_EQ(3U, input_method_ids.size());
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("xkb:jp::jpn", input_method_ids[2]);     // Japanese

  input_method_ids.push_back("mozc-jp");  // also Japanese
  SortInputMethodIdsByNamesInternal(id_to_language_code_map,
                                    &input_method_ids);
  ASSERT_EQ(4U, input_method_ids.size());
  ASSERT_EQ("xkb:fr::fra", input_method_ids[0]);     // French
  ASSERT_EQ("mozc", input_method_ids[1]);            // Japanese
  ASSERT_EQ("xkb:jp::jpn", input_method_ids[2]);     // Japanese
  ASSERT_EQ("mozc-jp", input_method_ids[3]);         // Japanese
}

TEST_F(InputMethodUtilTest, GetInputMethodIdsForLanguageCode) {
  std::multimap<std::string, std::string> language_code_to_ids_map;
  language_code_to_ids_map.insert(std::make_pair("ja", "mozc"));
  language_code_to_ids_map.insert(std::make_pair("ja", "mozc-jp"));
  language_code_to_ids_map.insert(std::make_pair("ja", "xkb:jp:jpn"));
  language_code_to_ids_map.insert(std::make_pair("fr", "xkb:fr:fra"));

  std::vector<std::string> result;
  EXPECT_TRUE(GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "ja", kAllInputMethods, &result));
  EXPECT_EQ(3U, result.size());
  EXPECT_TRUE(GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "ja", kKeyboardLayoutsOnly, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:jp:jpn", result[0]);

  EXPECT_TRUE(GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "fr", kAllInputMethods, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:fr:fra", result[0]);
  EXPECT_TRUE(GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "fr", kKeyboardLayoutsOnly, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:fr:fra", result[0]);

  EXPECT_FALSE(GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "invalid_lang", kAllInputMethods, &result));
  EXPECT_FALSE(GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "invalid_lang", kKeyboardLayoutsOnly, &result));
}

}  // namespace input_method
}  // namespace chromeos
