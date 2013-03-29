// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_util.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chromeos/ime/input_method_whitelist.h"
#include "chromeos/ime/mock_input_method_delegate.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace chromeos {

extern const char* kExtensionImePrefix;

namespace input_method {

namespace {

class TestableInputMethodUtil : public InputMethodUtil {
 public:
  explicit TestableInputMethodUtil(InputMethodDelegate* delegate,
                                   scoped_ptr<InputMethodDescriptors> methods)
      : InputMethodUtil(delegate, methods.Pass()) {
  }
  // Change access rights.
  using InputMethodUtil::StringIsSupported;
  using InputMethodUtil::GetInputMethodIdsFromLanguageCodeInternal;
  using InputMethodUtil::ReloadInternalMaps;
  using InputMethodUtil::SortLanguageCodesByNames;
  using InputMethodUtil::supported_input_methods_;
};

}  // namespace

class InputMethodUtilTest : public testing::Test {
 public:
  InputMethodUtilTest()
      : util_(&delegate_, whitelist_.GetSupportedInputMethods()) {
  }

  InputMethodDescriptor GetDesc(const std::string& id,
                                const std::string& raw_layout,
                                const std::string& language_code) {
    return InputMethodDescriptor(id,
                                 "",
                                 raw_layout,
                                 language_code,
                                 false);
  }

  MockInputMethodDelegate delegate_;
  InputMethodWhitelist whitelist_;
  TestableInputMethodUtil util_;
};

TEST_F(InputMethodUtilTest, GetInputMethodShortNameTest) {
  // Test normal cases. Two-letter language code should be returned.
  {
    InputMethodDescriptor desc = GetDesc("m17n:fa:isiri",  // input method id
                                         "us",  // keyboard layout name
                                         "fa");  // language name
    EXPECT_EQ(ASCIIToUTF16("FA"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("mozc-hangul", "us", "ko");
    EXPECT_EQ(UTF8ToUTF16("\xed\x95\x9c"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("invalid-id", "us", "xx");
    // Upper-case string of the unknown language code, "xx", should be returned.
    EXPECT_EQ(ASCIIToUTF16("XX"), util_.GetInputMethodShortName(desc));
  }

  // Test special cases.
  {
    InputMethodDescriptor desc = GetDesc("xkb:us:dvorak:eng", "us", "en-US");
    EXPECT_EQ(ASCIIToUTF16("DV"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:us:colemak:eng", "us", "en-US");
    EXPECT_EQ(ASCIIToUTF16("CO"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:altgr-intl:eng", "us", "en-US");
    EXPECT_EQ(ASCIIToUTF16("EXTD"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:us:intl:eng", "us", "en-US");
    EXPECT_EQ(ASCIIToUTF16("INTL"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:de:neo:ger", "de(neo)", "de");
    EXPECT_EQ(ASCIIToUTF16("NEO"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:es:cat:cat", "es(cat)", "ca");
    EXPECT_EQ(ASCIIToUTF16("CAS"), util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("mozc", "us", "ja");
    EXPECT_EQ(UTF8ToUTF16("\xe3\x81\x82"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("mozc-jp", "jp", "ja");
    EXPECT_EQ(UTF8ToUTF16("\xe3\x81\x82"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("pinyin", "us", "zh-CN");
    EXPECT_EQ(UTF8ToUTF16("\xe6\x8b\xbc"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("pinyin-dv", "us(dvorak)", "zh-CN");
    EXPECT_EQ(UTF8ToUTF16("\xe6\x8b\xbc"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("mozc-chewing", "us", "zh-TW");
    EXPECT_EQ(UTF8ToUTF16("\xe9\x85\xb7"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("m17n:zh:cangjie", "us", "zh-TW");
    EXPECT_EQ(UTF8ToUTF16("\xe5\x80\x89"),
              util_.GetInputMethodShortName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("m17n:zh:quick", "us", "zh-TW");
    EXPECT_EQ(UTF8ToUTF16("\xe9\x80\x9f"),
              util_.GetInputMethodShortName(desc));
  }
}

TEST_F(InputMethodUtilTest, GetInputMethodMediumNameTest) {
  {
    // input methods with medium name equal to short name
    const char * input_method_id[] = {
      "xkb:us:altgr-intl:eng",
      "xkb:us:dvorak:eng",
      "xkb:us:intl:eng",
      "xkb:us:colemak:eng",
      "english-m",
      "xkb:de:neo:ger",
      "xkb:es:cat:cat",
      "xkb:gb:dvorak:eng",
    };
    const int len = ARRAYSIZE_UNSAFE(input_method_id);
    for (int i=0; i<len; ++i) {
      InputMethodDescriptor desc = GetDesc(input_method_id[i], "", "");
      string16 medium_name = util_.GetInputMethodMediumName(desc);
      string16 short_name = util_.GetInputMethodShortName(desc);
      EXPECT_EQ(medium_name,short_name);
    }
  }
  {
    // input methods with medium name not equal to short name
    const char * input_method_id[] = {
      "m17n:zh:cangjie",
      "m17n:zh:quick",
      "mozc",
      "mozc-chewing",
      "mozc-dv",
      "mozc-hangul",
      "mozc-jp",
      "pinyin",
      "pinyin-dv",
    };
    const int len = ARRAYSIZE_UNSAFE(input_method_id);
    for (int i=0; i<len; ++i) {
      InputMethodDescriptor desc = GetDesc(input_method_id[i], "", "");
      string16 medium_name = util_.GetInputMethodMediumName(desc);
      string16 short_name = util_.GetInputMethodShortName(desc);
      EXPECT_NE(medium_name,short_name);
    }
  }
}

TEST_F(InputMethodUtilTest, GetInputMethodLongNameTest) {
  // For most languages input method or keyboard layout name is returned.
  // See below for exceptions.
  {
    InputMethodDescriptor desc = GetDesc("m17n:fa:isiri", "us", "fa");
    EXPECT_EQ(ASCIIToUTF16("Persian input method (ISIRI 2901 layout)"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("mozc-hangul", "us", "ko");
    EXPECT_EQ(ASCIIToUTF16("Korean input method"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("m17n:vi:tcvn", "us", "vi");
    EXPECT_EQ(ASCIIToUTF16("Vietnamese input method (TCVN6064)"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("mozc", "us", "ja");
#if !defined(GOOGLE_CHROME_BUILD)
    EXPECT_EQ(ASCIIToUTF16("Japanese input method (for US keyboard)"),
#else
    EXPECT_EQ(ASCIIToUTF16("Google Japanese Input (for US keyboard)"),
#endif  // defined(GOOGLE_CHROME_BUILD)
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:jp::jpn", "jp", "ja");
    EXPECT_EQ(ASCIIToUTF16("Japanese keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:us:dvorak:eng", "us(dvorak)", "en-US");
    EXPECT_EQ(ASCIIToUTF16("US Dvorak keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc =
        GetDesc("xkb:gb:dvorak:eng", "gb(dvorak)", "en-US");
    EXPECT_EQ(ASCIIToUTF16("UK Dvorak keyboard"),
              util_.GetInputMethodLongName(desc));
  }

  // For Arabic, Dutch, French, German and Hindi,
  // "language - keyboard layout" pair is returned.
  {
    InputMethodDescriptor desc = GetDesc("m17n:ar:kbd", "us", "ar");
    EXPECT_EQ(ASCIIToUTF16("Arabic - Standard input method"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:be::nld", "be", "nl");
    EXPECT_EQ(ASCIIToUTF16("Dutch - Belgian keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:fr::fra", "fr", "fr");
    EXPECT_EQ(ASCIIToUTF16("French - French keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:be::fra", "be", "fr");
    EXPECT_EQ(ASCIIToUTF16("French - Belgian keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:de::ger", "de", "de");
    EXPECT_EQ(ASCIIToUTF16("German - German keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("xkb:be::ger", "be", "de");
    EXPECT_EQ(ASCIIToUTF16("German - Belgian keyboard"),
              util_.GetInputMethodLongName(desc));
  }
  {
    InputMethodDescriptor desc = GetDesc("m17n:hi:itrans", "us", "hi");
    EXPECT_EQ(ASCIIToUTF16("Hindi - Standard input method"),
              util_.GetInputMethodLongName(desc));
  }

  {
    InputMethodDescriptor desc = GetDesc("invalid-id", "us", "xx");
    // You can safely ignore the "Resouce ID is not found for: invalid-id"
    // error.
    EXPECT_EQ(ASCIIToUTF16("invalid-id"),
              util_.GetInputMethodLongName(desc));
  }
}

TEST_F(InputMethodUtilTest, TestGetStringUTF8) {
  EXPECT_EQ(UTF8ToUTF16("Pinyin input method"),
            util_.TranslateString("pinyin"));
#if !defined(GOOGLE_CHROME_BUILD)
  EXPECT_EQ(UTF8ToUTF16("Japanese input method (for US Dvorak keyboard)"),
            util_.TranslateString("mozc-dv"));
#endif
}

TEST_F(InputMethodUtilTest, TestStringIsSupported) {
  EXPECT_TRUE(util_.StringIsSupported("Hiragana"));
  EXPECT_TRUE(util_.StringIsSupported("Latin"));
  EXPECT_TRUE(util_.StringIsSupported("Direct input"));
  EXPECT_FALSE(util_.StringIsSupported("####THIS_STRING_IS_NOT_SUPPORTED####"));
  EXPECT_TRUE(util_.StringIsSupported("Chinese"));
  EXPECT_TRUE(util_.StringIsSupported("_Chinese"));
}

TEST_F(InputMethodUtilTest, TestIsValidInputMethodId) {
  EXPECT_TRUE(util_.IsValidInputMethodId("xkb:us:colemak:eng"));
  EXPECT_TRUE(util_.IsValidInputMethodId("mozc"));
  EXPECT_FALSE(util_.IsValidInputMethodId("unsupported-input-method"));
}

TEST_F(InputMethodUtilTest, TestIsKeyboardLayout) {
  EXPECT_TRUE(InputMethodUtil::IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(InputMethodUtil::IsKeyboardLayout("mozc"));
}

TEST_F(InputMethodUtilTest, TestGetKeyboardLayoutName) {
  // Unsupported case.
  EXPECT_EQ("", util_.GetKeyboardLayoutName("UNSUPPORTED_ID"));

  // Supported cases (samples).
  EXPECT_EQ("jp", util_.GetKeyboardLayoutName("mozc-jp"));
  EXPECT_EQ("us", util_.GetKeyboardLayoutName("pinyin"));
  EXPECT_EQ("us(dvorak)", util_.GetKeyboardLayoutName("pinyin-dv"));
  EXPECT_EQ("us", util_.GetKeyboardLayoutName("m17n:ar:kbd"));
  EXPECT_EQ("es", util_.GetKeyboardLayoutName("xkb:es::spa"));
  EXPECT_EQ("es(cat)", util_.GetKeyboardLayoutName("xkb:es:cat:cat"));
  EXPECT_EQ("gb(extd)", util_.GetKeyboardLayoutName("xkb:gb:extd:eng"));
  EXPECT_EQ("us", util_.GetKeyboardLayoutName("xkb:us::eng"));
  EXPECT_EQ("us(dvorak)", util_.GetKeyboardLayoutName("xkb:us:dvorak:eng"));
  EXPECT_EQ("us(colemak)", util_.GetKeyboardLayoutName("xkb:us:colemak:eng"));
  EXPECT_EQ("de(neo)", util_.GetKeyboardLayoutName("xkb:de:neo:ger"));
}

TEST_F(InputMethodUtilTest, TestGetLanguageCodeFromInputMethodId) {
  // Make sure that the -CN is added properly.
  EXPECT_EQ("zh-CN", util_.GetLanguageCodeFromInputMethodId("pinyin"));
}

TEST_F(InputMethodUtilTest, TestGetInputMethodDisplayNameFromId) {
  EXPECT_EQ("Pinyin input method",
            util_.GetInputMethodDisplayNameFromId("pinyin"));
  EXPECT_EQ("US keyboard",
            util_.GetInputMethodDisplayNameFromId("xkb:us::eng"));
  EXPECT_EQ("", util_.GetInputMethodDisplayNameFromId("nonexistent"));
}

TEST_F(InputMethodUtilTest, TestGetInputMethodDescriptorFromId) {
  EXPECT_EQ(NULL, util_.GetInputMethodDescriptorFromId("non_existent"));

  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("pinyin");
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  EXPECT_EQ("pinyin", descriptor->id());
  EXPECT_EQ("us", descriptor->keyboard_layout());
  // This used to be "zh" but now we have "zh-CN" in input_methods.h,
  // hence this should be zh-CN now.
  EXPECT_EQ("zh-CN", descriptor->language_code());
}

TEST_F(InputMethodUtilTest, TestGetInputMethodDescriptorFromXkbId) {
  EXPECT_EQ(NULL, util_.GetInputMethodDescriptorFromXkbId("non_existent"));

  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromXkbId("us(dvorak)");
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  EXPECT_EQ("xkb:us:dvorak:eng", descriptor->id());
  EXPECT_EQ("us(dvorak)", descriptor->keyboard_layout());
  EXPECT_EQ("en-US", descriptor->language_code());
}

TEST_F(InputMethodUtilTest, TestGetLanguageNativeDisplayNameFromCode) {
  EXPECT_EQ(UTF8ToUTF16("suomi"),
            InputMethodUtil::GetLanguageNativeDisplayNameFromCode("fi"));
}

TEST_F(InputMethodUtilTest, TestSortLanguageCodesByNames) {
  std::vector<std::string> language_codes;
  // Check if this function can handle an empty list.
  util_.SortLanguageCodesByNames(&language_codes);

  language_codes.push_back("ja");
  language_codes.push_back("fr");
  // For "t", see the comment in NormalizeLanguageCode test.
  language_codes.push_back("t");
  util_.SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(3U, language_codes.size());
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("t",  language_codes[2]);  // Others

  // Add a duplicate entry and see if it works.
  language_codes.push_back("ja");
  util_.SortLanguageCodesByNames(&language_codes);
  ASSERT_EQ(4U, language_codes.size());
  ASSERT_EQ("fr", language_codes[0]);  // French
  ASSERT_EQ("ja", language_codes[1]);  // Japanese
  ASSERT_EQ("ja", language_codes[2]);  // Japanese
  ASSERT_EQ("t",  language_codes[3]);  // Others
}

TEST_F(InputMethodUtilTest, TestGetInputMethodIdsForLanguageCode) {
  std::multimap<std::string, std::string> language_code_to_ids_map;
  language_code_to_ids_map.insert(std::make_pair("ja", "mozc"));
  language_code_to_ids_map.insert(std::make_pair("ja", "mozc-jp"));
  language_code_to_ids_map.insert(std::make_pair("ja", "xkb:jp:jpn"));
  language_code_to_ids_map.insert(std::make_pair("fr", "xkb:fr:fra"));

  std::vector<std::string> result;
  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "ja", kAllInputMethods, &result));
  EXPECT_EQ(3U, result.size());
  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "ja", kKeyboardLayoutsOnly, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:jp:jpn", result[0]);

  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "fr", kAllInputMethods, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:fr:fra", result[0]);
  EXPECT_TRUE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "fr", kKeyboardLayoutsOnly, &result));
  ASSERT_EQ(1U, result.size());
  EXPECT_EQ("xkb:fr:fra", result[0]);

  EXPECT_FALSE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "invalid_lang", kAllInputMethods, &result));
  EXPECT_FALSE(util_.GetInputMethodIdsFromLanguageCodeInternal(
      language_code_to_ids_map, "invalid_lang", kKeyboardLayoutsOnly, &result));
}

// US keyboard + English US UI = US keyboard only.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_EnUs) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:us::eng");  // US keyboard.
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("en-US", *descriptor, &input_method_ids);
  ASSERT_EQ(1U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
}

// US keyboard + Japanese UI = US keyboard + mozc.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Ja) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:us::eng");  // US keyboard.
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ja", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("mozc", input_method_ids[1]);  // Mozc for US keybaord.
}

// JP keyboard + Japanese UI = JP keyboard + mozc-jp.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_JP_And_Ja) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:jp::jpn");  // Japanese keyboard
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ja", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:jp::jpn", input_method_ids[0]);
  EXPECT_EQ("mozc-jp", input_method_ids[1]);  // Mozc for JP keybaord.
}

// US dvorak keyboard + Japanese UI = US dvorak keyboard + mozc-dv.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Dvorak_And_Ja) {
  const InputMethodDescriptor* descriptor =
      // US Drovak keyboard.
      util_.GetInputMethodDescriptorFromId("xkb:us:dvorak:eng");
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ja", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us:dvorak:eng", input_method_ids[0]);
  EXPECT_EQ("mozc-dv", input_method_ids[1]);  // Mozc for US Dvorak keybaord.
}

// US keyboard + Russian UI = US keyboard + Russsian keyboard
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Ru) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:us::eng");  // US keyboard.
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("ru", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("xkb:ru::rus", input_method_ids[1]);  // Russian keyboard.
}

// US keyboard + Traditional Chinese = US keyboard + chewing.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_ZhTw) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:us::eng");  // US keyboard.
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("zh-TW", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("mozc-chewing", input_method_ids[1]);  // Chewing.
}

// US keyboard + Thai = US keyboard + kesmanee.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Th) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:us::eng");  // US keyboard.
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("th", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("m17n:th:kesmanee", input_method_ids[1]);  // Kesmanee.
}

// US keyboard + Vietnamese = US keyboard + TCVN6064.
TEST_F(InputMethodUtilTest, TestGetFirstLoginInputMethodIds_Us_And_Vi) {
  const InputMethodDescriptor* descriptor =
      util_.GetInputMethodDescriptorFromId("xkb:us::eng");  // US keyboard.
  ASSERT_TRUE(NULL != descriptor);  // ASSERT_NE doesn't compile.
  std::vector<std::string> input_method_ids;
  util_.GetFirstLoginInputMethodIds("vi", *descriptor, &input_method_ids);
  ASSERT_EQ(2U, input_method_ids.size());
  EXPECT_EQ("xkb:us::eng", input_method_ids[0]);
  EXPECT_EQ("m17n:vi:tcvn", input_method_ids[1]);  // TCVN6064.
}

TEST_F(InputMethodUtilTest, TestGetLanguageCodesFromInputMethodIds) {
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back("xkb:us::eng");  // English US.
  input_method_ids.push_back("xkb:us:dvorak:eng");  // English US Dvorak.
  input_method_ids.push_back("mozc-jp");  // Japanese.
  input_method_ids.push_back("xkb:fr::fra");  // French France.
  std::vector<std::string> language_codes;
  util_.GetLanguageCodesFromInputMethodIds(input_method_ids, &language_codes);
  ASSERT_EQ(3U, language_codes.size());
  EXPECT_EQ("en-US", language_codes[0]);
  EXPECT_EQ("ja", language_codes[1]);
  EXPECT_EQ("fr", language_codes[2]);
}

// Test all supported descriptors to detect a typo in ibus_input_methods.txt.
TEST_F(InputMethodUtilTest, TestIBusInputMethodText) {
  for (size_t i = 0; i < util_.supported_input_methods_->size(); ++i) {
    const std::string language_code =
        util_.supported_input_methods_->at(i).language_code();
    const string16 display_name =
        l10n_util::GetDisplayNameForLocale(language_code, "en", false);
    // Only two formats, like "fr" (lower case) and "en-US" (lower-upper), are
    // allowed. See the text file for details.
    EXPECT_TRUE(language_code.length() == 2 ||
                (language_code.length() == 5 && language_code[2] == '-'))
        << "Invalid language code " << language_code;
    EXPECT_TRUE(l10n_util::IsValidLocaleSyntax(language_code))
        << "Invalid language code " << language_code;
    EXPECT_FALSE(display_name.empty())
        << "Invalid language code " << language_code;
    // On error, GetDisplayNameForLocale() returns the |language_code| as-is.
    EXPECT_NE(language_code, UTF16ToUTF8(display_name))
        << "Invalid language code " << language_code;
  }
}

}  // namespace input_method
}  // namespace chromeos
