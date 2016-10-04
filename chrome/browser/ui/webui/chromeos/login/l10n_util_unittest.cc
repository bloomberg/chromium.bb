// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/login/l10n_util.h"

#include <stddef.h>

#include <utility>

#include "base/at_exit.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/values.h"
#include "chrome/browser/chromeos/customization/customization_document.h"
#include "chrome/browser/chromeos/input_method/input_method_configuration.h"
#include "chrome/browser/ui/webui/chromeos/login/l10n_util_test_util.h"
#include "chromeos/system/statistics_provider.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/ime/chromeos/component_extension_ime_manager.h"

namespace chromeos {

namespace {

class MachineStatisticsInitializer {
 public:
  MachineStatisticsInitializer();

  static MachineStatisticsInitializer* GetInstance();

 private:
  DISALLOW_COPY_AND_ASSIGN(MachineStatisticsInitializer);
};

MachineStatisticsInitializer::MachineStatisticsInitializer() {
  base::MessageLoop loop;
  chromeos::system::StatisticsProvider::GetInstance()
      ->StartLoadingMachineStatistics(loop.task_runner(), false);
  base::RunLoop().RunUntilIdle();
}

// static
MachineStatisticsInitializer* MachineStatisticsInitializer::GetInstance() {
  return base::Singleton<MachineStatisticsInitializer>::get();
}

void VerifyOnlyUILanguages(const base::ListValue& list) {
  for (size_t i = 0; i < list.GetSize(); ++i) {
    const base::DictionaryValue* dict;
    ASSERT_TRUE(list.GetDictionary(i, &dict));
    std::string code;
    ASSERT_TRUE(dict->GetString("code", &code));
    EXPECT_NE("is", code)
        << "Icelandic is an example language which has input method "
        << "but can't use it as UI language.";
  }
}

void VerifyLanguageCode(const base::ListValue& list,
                        size_t index,
                        const std::string& expected_code) {
  const base::DictionaryValue* dict;
  ASSERT_TRUE(list.GetDictionary(index, &dict));
  std::string actual_code;
  ASSERT_TRUE(dict->GetString("code", &actual_code));
  EXPECT_EQ(expected_code, actual_code)
      << "Wrong language code at index " << index << ".";
}

}  // namespace

class L10nUtilTest : public testing::Test {
 public:
  L10nUtilTest();
  ~L10nUtilTest() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

  void SetInputMethods1();
  void SetInputMethods2();

 private:
  base::ShadowingAtExitManager at_exit_manager_;

  MockInputMethodManagerWithInputMethods* input_manager_;
};

L10nUtilTest::L10nUtilTest()
    : input_manager_(new MockInputMethodManagerWithInputMethods) {
}

L10nUtilTest::~L10nUtilTest() {
}

void L10nUtilTest::SetUp() {
  chromeos::input_method::InitializeForTesting(input_manager_);
  input_manager_->SetComponentExtensionIMEManager(
      base::WrapUnique(new ComponentExtensionIMEManager));
  MachineStatisticsInitializer::GetInstance();  // Ignore result.
}

void L10nUtilTest::TearDown() {
  chromeos::input_method::Shutdown();
}

void L10nUtilTest::SetInputMethods1() {
  input_manager_->AddInputMethod("xkb:us::eng", "us", "en-US");
  input_manager_->AddInputMethod("xkb:fr::fra", "fr", "fr");
  input_manager_->AddInputMethod("xkb:be::fra", "be", "fr");
  input_manager_->AddInputMethod("xkb:is::ice", "is", "is");
}

void L10nUtilTest::SetInputMethods2() {
  input_manager_->AddInputMethod("xkb:us::eng", "us", "en-US");
  input_manager_->AddInputMethod("xkb:ch:fr:fra", "ch(fr)", "fr");
  input_manager_->AddInputMethod("xkb:ch::ger", "ch", "de");
  input_manager_->AddInputMethod("xkb:it::ita", "it", "it");
  input_manager_->AddInputMethod("xkb:is::ice", "is", "is");
}

TEST_F(L10nUtilTest, GetUILanguageList) {
  SetInputMethods1();

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  std::unique_ptr<base::ListValue> list(GetUILanguageList(NULL, std::string()));

  VerifyOnlyUILanguages(*list);
}

TEST_F(L10nUtilTest, FindMostRelevantLocale) {
  base::ListValue available_locales;
  std::unique_ptr<base::DictionaryValue> dict(new base::DictionaryValue);
  dict->SetString("value", "de");
  available_locales.Append(std::move(dict));
  dict.reset(new base::DictionaryValue);
  dict->SetString("value", "fr");
  available_locales.Append(std::move(dict));
  dict.reset(new base::DictionaryValue);
  dict->SetString("value", "en-GB");
  available_locales.Append(std::move(dict));

  std::vector<std::string> most_relevant_language_codes;
  EXPECT_EQ("en-US", FindMostRelevantLocale(most_relevant_language_codes,
                                            available_locales,
                                            "en-US"));

  most_relevant_language_codes.push_back("xx");
  EXPECT_EQ("en-US", FindMostRelevantLocale(most_relevant_language_codes,
                                            available_locales,
                                            "en-US"));

  most_relevant_language_codes.push_back("fr");
  EXPECT_EQ("fr", FindMostRelevantLocale(most_relevant_language_codes,
                                         available_locales,
                                         "en-US"));

  most_relevant_language_codes.push_back("de");
  EXPECT_EQ("fr", FindMostRelevantLocale(most_relevant_language_codes,
                                         available_locales,
                                         "en-US"));
}

void InitStartupCustomizationDocumentForTesting(const std::string& manifest) {
  StartupCustomizationDocument::GetInstance()->LoadManifestFromString(manifest);
  StartupCustomizationDocument::GetInstance()->Init(
      chromeos::system::StatisticsProvider::GetInstance());
}

const char kStartupManifest[] =
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

TEST_F(L10nUtilTest, GetUILanguageListMulti) {
  InitStartupCustomizationDocumentForTesting(kStartupManifest);
  SetInputMethods2();

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  std::unique_ptr<base::ListValue> list(GetUILanguageList(NULL, std::string()));

  VerifyOnlyUILanguages(*list);

  // (4 languages (except Icelandic) + divider) = 5 + all other languages
  ASSERT_LE(5u, list->GetSize());

  VerifyLanguageCode(*list, 0, "fr");
  VerifyLanguageCode(*list, 1, "en-US");
  VerifyLanguageCode(*list, 2, "de");
  VerifyLanguageCode(*list, 3, "it");
  VerifyLanguageCode(*list, 4, kMostRelevantLanguagesDivider);
}

TEST_F(L10nUtilTest, GetUILanguageListWithMostRelevant) {
  std::vector<std::string> most_relevant_language_codes;
  most_relevant_language_codes.push_back("it");
  most_relevant_language_codes.push_back("de");
  most_relevant_language_codes.push_back("nonexistent");

  // This requires initialized StatisticsProvider (see L10nUtilTest()).
  std::unique_ptr<base::ListValue> list(
      GetUILanguageList(&most_relevant_language_codes, std::string()));

  VerifyOnlyUILanguages(*list);

  ASSERT_LE(3u, list->GetSize());

  VerifyLanguageCode(*list, 0, "it");
  VerifyLanguageCode(*list, 1, "de");
  VerifyLanguageCode(*list, 2, kMostRelevantLanguagesDivider);
}

}  // namespace chromeos
