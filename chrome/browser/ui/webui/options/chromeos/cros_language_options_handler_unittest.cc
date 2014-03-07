// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/options/language_options_handler.h"

#include <string>

#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/values.h"
#include "chrome/browser/chromeos/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/chromeos/input_method/mock_input_method_manager.h"
#include "chrome/browser/ui/webui/options/chromeos/cros_language_options_handler.h"
#include "chromeos/ime/input_method_descriptor.h"
#include "chromeos/system/statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

using chromeos::input_method::InputMethodDescriptor;
using chromeos::input_method::InputMethodDescriptors;
using chromeos::input_method::MockInputMethodManager;

namespace {

class MachineStatisticsInitializer {
 public:
  MachineStatisticsInitializer() {
    base::MessageLoop tmp_loop(base::MessageLoop::TYPE_DEFAULT);
    chromeos::system::StatisticsProvider::GetInstance()
        ->StartLoadingMachineStatistics(tmp_loop.message_loop_proxy(), false);
    tmp_loop.RunUntilIdle();
  }
  static MachineStatisticsInitializer* GetInstance();
};

MachineStatisticsInitializer* MachineStatisticsInitializer::GetInstance() {
  return Singleton<MachineStatisticsInitializer>::get();
}

class CrosLanguageOptionsHandlerTest : public testing::Test {
 public:
  CrosLanguageOptionsHandlerTest() {
    chromeos::input_method::InitializeForTesting(new MockInputMethodManager);
    MachineStatisticsInitializer::GetInstance();  // Ignore result
  }
  virtual ~CrosLanguageOptionsHandlerTest() {
    chromeos::input_method::Shutdown();
  }

 protected:
  InputMethodDescriptors CreateInputMethodDescriptors1() {
    InputMethodDescriptors descriptors;
    descriptors.push_back(GetDesc("xkb:us::eng", "us", "en-US"));
    descriptors.push_back(GetDesc("xkb:fr::fra", "fr", "fr"));
    descriptors.push_back(GetDesc("xkb:be::fra", "be", "fr"));
    descriptors.push_back(GetDesc("xkb:is::ice", "is", "is"));
    return descriptors;
  }

  InputMethodDescriptors CreateInputMethodDescriptors2() {
    InputMethodDescriptors descriptors;
    descriptors.push_back(GetDesc("xkb:us::eng", "us", "en-US"));
    descriptors.push_back(GetDesc("xkb:ch:fr:fra", "ch(fr)", "fr"));
    descriptors.push_back(GetDesc("xkb:ch::ger", "ch", "de"));
    descriptors.push_back(GetDesc("xkb:it::ita", "it", "it"));
    descriptors.push_back(GetDesc("xkb:is::ice", "is", "is"));
    return descriptors;
  }

 private:
  InputMethodDescriptor GetDesc(const std::string& id,
                                const std::string& raw_layout,
                                const std::string& language_code) {
    std::vector<std::string> layouts;
    layouts.push_back(raw_layout);
    std::vector<std::string> languages;
    languages.push_back(language_code);
    return InputMethodDescriptor(
        id, std::string(), std::string(), layouts, languages, true,
        GURL(), GURL());
  }

  base::ShadowingAtExitManager at_exit_manager_;
};

}  // namespace

void Test__InitStartupCustomizationDocument(const std::string& manifest) {
  chromeos::StartupCustomizationDocument::GetInstance()->LoadManifestFromString(
      manifest);
  chromeos::StartupCustomizationDocument::GetInstance()->Init(
      chromeos::system::StatisticsProvider::GetInstance());
}

TEST_F(CrosLanguageOptionsHandlerTest, GetInputMethodList) {
  InputMethodDescriptors descriptors = CreateInputMethodDescriptors1();
  scoped_ptr<base::ListValue> list(
      chromeos::options::CrosLanguageOptionsHandler::GetInputMethodList(
          descriptors));
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

TEST_F(CrosLanguageOptionsHandlerTest, GetUILanguageList) {
  // This requires initialized StatisticsProvider.
  // (see CrosLanguageOptionsHandlerTest() )
  InputMethodDescriptors descriptors = CreateInputMethodDescriptors1();
  scoped_ptr<base::ListValue> list(
      chromeos::options::CrosLanguageOptionsHandler::GetUILanguageList(
          descriptors));

  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict;
    ASSERT_TRUE(list->GetDictionary(i, &dict));
    std::string code;
    ASSERT_TRUE(dict->GetString("code", &code));
    EXPECT_NE("is", code)
        << "Icelandic is an example language which has input method "
        << "but can't use it as UI language.";
  }
}

const char kStartupManifest1[] =
    "{\n"
    "  \"version\": \"1.0\",\n"
    "  \"initial_locale\" : \"fr,en-US,de,is,it\",\n"
    "  \"initial_timezone\" : \"Europe/Zurich\",\n"
    "  \"keyboard_layout\" : \"xkb:ch:fr:fra\",\n"
    "  \"registration_url\" : \"http://www.google.com\",\n"
    "  \"setup_content\" : {\n"
    "    \"default\" : {\n"
    "      \"help_page\" : \"file:///opt/oem/help/en-US/help.html\",\n"
    "      \"eula_page\" : \"file:///opt/oem/eula/en-US/eula.html\",\n"
    "    },\n"
    "  },"
    "}";

#define EXPECT_LANGUAGE_CODE_AT(i, value)                                  \
  if (list->GetSize() > i) {                                               \
    ASSERT_TRUE(list->GetDictionary(i, &dict));                            \
    ASSERT_TRUE(dict->GetString("code", &code));                           \
    EXPECT_EQ(value, code) << "Wrong language code at index " << i << "."; \
  }

TEST_F(CrosLanguageOptionsHandlerTest, GetUILanguageListMulti) {
  Test__InitStartupCustomizationDocument(kStartupManifest1);

  // This requires initialized StatisticsProvider.
  // (see CrosLanguageOptionsHandlerTest() )
  InputMethodDescriptors descriptors = CreateInputMethodDescriptors2();
  scoped_ptr<base::ListValue> list(
      chromeos::options::CrosLanguageOptionsHandler::GetUILanguageList(
          descriptors));

  base::DictionaryValue* dict;
  std::string code;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    ASSERT_TRUE(list->GetDictionary(i, &dict));
    ASSERT_TRUE(dict->GetString("code", &code));
    EXPECT_NE("is", code)
        << "Icelandic is an example language which has input method "
        << "but can't use it as UI language.";
  }

  // (4 languages (except islandic) + divider)=5 + all other languages
  EXPECT_GT(list->GetSize(), 5u);

  EXPECT_LANGUAGE_CODE_AT(0, "fr")
  EXPECT_LANGUAGE_CODE_AT(1, "en-US")
  EXPECT_LANGUAGE_CODE_AT(2, "de")
  EXPECT_LANGUAGE_CODE_AT(3, "it")
  EXPECT_LANGUAGE_CODE_AT(4,
                          chromeos::options::kVendorOtherLanguagesListDivider);
}
