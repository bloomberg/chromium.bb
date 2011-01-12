// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/input_method/input_method_util.h"

#include <string>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "grit/generated_resources.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace input_method {

TEST(InputMethodUtilTest, GetStringUTF8) {
  EXPECT_EQ("Pinyin input method",
            GetStringUTF8("Pinyin"));
  EXPECT_EQ("Japanese input method (for US Dvorak keyboard)",
            GetStringUTF8("Mozc (US Dvorak keyboard layout)"));
  EXPECT_EQ("Google Japanese Input (for US Dvorak keyboard)",
            GetStringUTF8("Google Japanese Input (US Dvorak keyboard layout)"));
}

TEST(InputMethodUtilTest, StringIsSupported) {
  EXPECT_TRUE(StringIsSupported("Hiragana"));
  EXPECT_TRUE(StringIsSupported("Latin"));
  EXPECT_TRUE(StringIsSupported("Direct input"));
  EXPECT_FALSE(StringIsSupported("####THIS_STRING_IS_NOT_SUPPORTED####"));
}

TEST(InputMethodUtilTest, NormalizeLanguageCode) {
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

TEST(InputMethodUtilTest, IsKeyboardLayout) {
  EXPECT_TRUE(IsKeyboardLayout("xkb:us::eng"));
  EXPECT_FALSE(IsKeyboardLayout("anthy"));
}

TEST(InputMethodUtilTest, GetLanguageCodeFromDescriptor) {
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

TEST(InputMethodUtilTest, GetKeyboardLayoutName) {
  // Unsupported cases
  EXPECT_EQ("", GetKeyboardLayoutName("UNSUPPORTED_ID"));
  EXPECT_EQ("", GetKeyboardLayoutName("chewing"));
  EXPECT_EQ("", GetKeyboardLayoutName("hangul"));
  EXPECT_EQ("", GetKeyboardLayoutName("mozc"));
  EXPECT_EQ("", GetKeyboardLayoutName("mozc-jp"));
  EXPECT_EQ("", GetKeyboardLayoutName("pinyin"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:ar:kbd"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:he:kbd"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:hi:itrans"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:fa:isiri"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:th:kesmanee"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:th:pattachote"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:th:tis820"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:vi:tcvn"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:vi:telex"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:vi:viqr"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:vi:vni"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:zh:cangjie"));
  EXPECT_EQ("", GetKeyboardLayoutName("m17n:zh:quick"));

  // Supported cases
  EXPECT_EQ("be", GetKeyboardLayoutName("xkb:be::fra"));
  EXPECT_EQ("br", GetKeyboardLayoutName("xkb:br::por"));
  EXPECT_EQ("bg", GetKeyboardLayoutName("xkb:bg::bul"));
  EXPECT_EQ("cz", GetKeyboardLayoutName("xkb:cz::cze"));
  EXPECT_EQ("de", GetKeyboardLayoutName("xkb:de::ger"));
  EXPECT_EQ("ee", GetKeyboardLayoutName("xkb:ee::est"));
  EXPECT_EQ("es", GetKeyboardLayoutName("xkb:es::spa"));
  EXPECT_EQ("es", GetKeyboardLayoutName("xkb:es:cat:cat"));
  EXPECT_EQ("dk", GetKeyboardLayoutName("xkb:dk::dan"));
  EXPECT_EQ("gr", GetKeyboardLayoutName("xkb:gr::gre"));
  EXPECT_EQ("lt", GetKeyboardLayoutName("xkb:lt::lit"));
  EXPECT_EQ("lv", GetKeyboardLayoutName("xkb:lv::lav"));
  EXPECT_EQ("hr", GetKeyboardLayoutName("xkb:hr::scr"));
  EXPECT_EQ("nl", GetKeyboardLayoutName("xkb:nl::nld"));
  EXPECT_EQ("gb", GetKeyboardLayoutName("xkb:gb::eng"));
  EXPECT_EQ("fi", GetKeyboardLayoutName("xkb:fi::fin"));
  EXPECT_EQ("fr", GetKeyboardLayoutName("xkb:fr::fra"));
  EXPECT_EQ("hu", GetKeyboardLayoutName("xkb:hu::hun"));
  EXPECT_EQ("it", GetKeyboardLayoutName("xkb:it::ita"));
  EXPECT_EQ("jp", GetKeyboardLayoutName("xkb:jp::jpn"));
  EXPECT_EQ("no", GetKeyboardLayoutName("xkb:no::nor"));
  EXPECT_EQ("pl", GetKeyboardLayoutName("xkb:pl::pol"));
  EXPECT_EQ("pt", GetKeyboardLayoutName("xkb:pt::por"));
  EXPECT_EQ("ro", GetKeyboardLayoutName("xkb:ro::rum"));
  EXPECT_EQ("se", GetKeyboardLayoutName("xkb:se::swe"));
  EXPECT_EQ("sk", GetKeyboardLayoutName("xkb:sk::slo"));
  EXPECT_EQ("si", GetKeyboardLayoutName("xkb:si::slv"));
  EXPECT_EQ("rs", GetKeyboardLayoutName("xkb:rs::srp"));
  EXPECT_EQ("ch", GetKeyboardLayoutName("xkb:ch::ger"));
  EXPECT_EQ("ru", GetKeyboardLayoutName("xkb:ru::rus"));
  EXPECT_EQ("tr", GetKeyboardLayoutName("xkb:tr::tur"));
  EXPECT_EQ("ua", GetKeyboardLayoutName("xkb:ua::ukr"));
  EXPECT_EQ("us", GetKeyboardLayoutName("xkb:us::eng"));
  EXPECT_EQ("us", GetKeyboardLayoutName("xkb:us:dvorak:eng"));
  EXPECT_EQ("us", GetKeyboardLayoutName("xkb:us:colemak:eng"));
}

TEST(InputMethodUtilTest, GetKeyboardOverlayId) {
  // Invalid IDs
  EXPECT_EQ("", GetKeyboardOverlayId(""));
  EXPECT_EQ("", GetKeyboardOverlayId("aaa"));

  // Valid IDs
  EXPECT_EQ("nl", GetKeyboardOverlayId("xkb:nl::nld"));
  EXPECT_EQ("nl", GetKeyboardOverlayId("xkb:be::nld"));
  EXPECT_EQ("fr", GetKeyboardOverlayId("xkb:fr::fra"));
  EXPECT_EQ("fr", GetKeyboardOverlayId("xkb:be::fra"));
  EXPECT_EQ("fr_CA", GetKeyboardOverlayId("xkb:ca::fra"));
  EXPECT_EQ("fr", GetKeyboardOverlayId("xkb:ch:fr:fra"));
  EXPECT_EQ("de", GetKeyboardOverlayId("xkb:de::ger"));
  EXPECT_EQ("de", GetKeyboardOverlayId("xkb:be::ger"));
  EXPECT_EQ("de", GetKeyboardOverlayId("xkb:ch::ger"));
  EXPECT_EQ("en_US", GetKeyboardOverlayId("mozc"));
  EXPECT_EQ("ja", GetKeyboardOverlayId("mozc-jp"));
  EXPECT_EQ("en_US_dvorak", GetKeyboardOverlayId("mozc-dv"));
  EXPECT_EQ("ja", GetKeyboardOverlayId("xkb:jp::jpn"));
  EXPECT_EQ("ru", GetKeyboardOverlayId("xkb:ru::rus"));
  EXPECT_EQ("ru", GetKeyboardOverlayId("xkb:ru:phonetic:rus"));
  EXPECT_EQ("th", GetKeyboardOverlayId("m17n:th:kesmanee"));
  EXPECT_EQ("th", GetKeyboardOverlayId("m17n:th:pattachote"));
  EXPECT_EQ("th", GetKeyboardOverlayId("m17n:th:tis820"));
  EXPECT_EQ("zh_TW", GetKeyboardOverlayId("chewing"));
  EXPECT_EQ("zh_TW", GetKeyboardOverlayId("m17n:zh:cangjie"));
  EXPECT_EQ("zh_TW", GetKeyboardOverlayId("m17n:zh:quick"));
  EXPECT_EQ("vi", GetKeyboardOverlayId("m17n:vi:tcvn"));
  EXPECT_EQ("vi", GetKeyboardOverlayId("m17n:vi:telex"));
  EXPECT_EQ("vi", GetKeyboardOverlayId("m17n:vi:viqr"));
  EXPECT_EQ("vi", GetKeyboardOverlayId("m17n:vi:vni"));
  EXPECT_EQ("en_US", GetKeyboardOverlayId("xkb:us::eng"));
  EXPECT_EQ("en_US", GetKeyboardOverlayId("xkb:us:intl:eng"));
  EXPECT_EQ("en_US", GetKeyboardOverlayId("xkb:us:altgr-intl:eng"));
  EXPECT_EQ("en_US_dvorak", GetKeyboardOverlayId("xkb:us:dvorak:eng"));
  // TODO(mazda): Add keyboard overlay definition for US Colemak.
  EXPECT_EQ("en_US", GetKeyboardOverlayId("xkb:us:colemak:eng"));
  EXPECT_EQ("ko", GetKeyboardOverlayId("hangul"));
  EXPECT_EQ("zh_CN", GetKeyboardOverlayId("pinyin"));
  EXPECT_EQ("ar", GetKeyboardOverlayId("m17n:ar:kbd"));
  EXPECT_EQ("hi", GetKeyboardOverlayId("m17n:hi:itrans"));
  EXPECT_EQ("ar", GetKeyboardOverlayId("m17n:fa:isiri"));
  EXPECT_EQ("pt_BR", GetKeyboardOverlayId("xkb:br::por"));
  EXPECT_EQ("bg", GetKeyboardOverlayId("xkb:bg::bul"));
  EXPECT_EQ("bg", GetKeyboardOverlayId("xkb:bg:phonetic:bul"));
  EXPECT_EQ("ca", GetKeyboardOverlayId("xkb:ca:eng:eng"));
  EXPECT_EQ("cs", GetKeyboardOverlayId("xkb:cz::cze"));
  EXPECT_EQ("et", GetKeyboardOverlayId("xkb:ee::est"));
  EXPECT_EQ("es", GetKeyboardOverlayId("xkb:es::spa"));
  EXPECT_EQ("ca", GetKeyboardOverlayId("xkb:es:cat:cat"));
  EXPECT_EQ("da", GetKeyboardOverlayId("xkb:dk::dan"));
  EXPECT_EQ("el", GetKeyboardOverlayId("xkb:gr::gre"));
  EXPECT_EQ("iw", GetKeyboardOverlayId("xkb:il::heb"));
  EXPECT_EQ("ko", GetKeyboardOverlayId("xkb:kr:kr104:kor"));
  EXPECT_EQ("es_419", GetKeyboardOverlayId("xkb:latam::spa"));
  EXPECT_EQ("lt", GetKeyboardOverlayId("xkb:lt::lit"));
  EXPECT_EQ("lv", GetKeyboardOverlayId("xkb:lv:apostrophe:lav"));
  EXPECT_EQ("hr", GetKeyboardOverlayId("xkb:hr::scr"));
  EXPECT_EQ("en_GB", GetKeyboardOverlayId("xkb:gb:extd:eng"));
  EXPECT_EQ("fi", GetKeyboardOverlayId("xkb:fi::fin"));
  EXPECT_EQ("hu", GetKeyboardOverlayId("xkb:hu::hun"));
  EXPECT_EQ("it", GetKeyboardOverlayId("xkb:it::ita"));
  EXPECT_EQ("no", GetKeyboardOverlayId("xkb:no::nob"));
  EXPECT_EQ("pl", GetKeyboardOverlayId("xkb:pl::pol"));
  EXPECT_EQ("pt_PT", GetKeyboardOverlayId("xkb:pt::por"));
  EXPECT_EQ("ro", GetKeyboardOverlayId("xkb:ro::rum"));
  EXPECT_EQ("sv", GetKeyboardOverlayId("xkb:se::swe"));
  EXPECT_EQ("sk", GetKeyboardOverlayId("xkb:sk::slo"));
  EXPECT_EQ("sl", GetKeyboardOverlayId("xkb:si::slv"));
  EXPECT_EQ("sr", GetKeyboardOverlayId("xkb:rs::srp"));
  EXPECT_EQ("tr", GetKeyboardOverlayId("xkb:tr::tur"));
  EXPECT_EQ("uk", GetKeyboardOverlayId("xkb:ua::ukr"));
}

TEST(InputMethodUtilTest, GetLanguageDisplayNameFromCode) {
  EXPECT_EQ(UTF8ToUTF16("Finnish"), GetLanguageDisplayNameFromCode("fi"));
}

TEST(InputMethodUtilTest, GetLanguageNativeDisplayNameFromCode) {
  EXPECT_EQ(UTF8ToUTF16("suomi"), GetLanguageNativeDisplayNameFromCode("fi"));
}

TEST(InputMethodUtilTest, SortLanguageCodesByNames) {
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

TEST(LanguageConfigModelTest, SortInputMethodIdsByNamesInternal) {
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

TEST(LanguageConfigModelTest, GetInputMethodIdsForLanguageCode) {
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
