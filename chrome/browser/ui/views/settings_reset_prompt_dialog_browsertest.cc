// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/profile_resetter/brandcoded_default_settings.h"
#include "chrome/browser/profile_resetter/resettable_settings_snapshot.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_controller.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_model.h"
#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using safe_browsing::MockProfileResetter;
using safe_browsing::MockSettingsResetPromptConfig;
using testing::_;
using testing::ElementsAre;
using testing::NiceMock;
using testing::Not;
using testing::Return;
using testing::ReturnRef;

constexpr char kHomepageUrl[] = "http://www.a-homepage.com/some/path";
constexpr char kSearchUrl[] = "http://www.a-search.com/some/path/?q={%s}";
constexpr char kStartupUrl1[] = "http://www.a-startup-1.com/some/path";
constexpr char kStartupUrl2[] = "http://www.a-startup-2.com/some/other/path";
constexpr char kExtensionId1[] = "abcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionId2[] = "bbcdefghijklmnopabcdefghijklmnop";
constexpr char kExtensionName1[] = "Extensions1";
constexpr char kExtensionName2[] = "Extensions2";

enum class SettingType {
  DEFAULT_SEARCH_ENGINE,
  STARTUP_PAGE,
  HOMEPAGE,
  EXTENSIONS,  // Also disable some extensions.
};

class MockSettingsResetPromptModel
    : public safe_browsing::SettingsResetPromptModel {
 public:
  // Create a mock model that pretends that the settings passed in via
  // |settings_to_reset| require resetting. |settings_to_reset| must not be
  // empty or have |EXTENSIONS| as the only member.
  explicit MockSettingsResetPromptModel(Profile* profile,
                                        std::set<SettingType> settings_to_reset)
      : SettingsResetPromptModel(
            profile,
            base::MakeUnique<NiceMock<MockSettingsResetPromptConfig>>(),
            base::MakeUnique<ResettableSettingsSnapshot>(profile),
            base::MakeUnique<BrandcodedDefaultSettings>(),
            base::MakeUnique<NiceMock<MockProfileResetter>>(profile)) {
    EXPECT_FALSE(settings_to_reset.empty());
    EXPECT_THAT(settings_to_reset, Not(ElementsAre(SettingType::EXTENSIONS)));

    // Set up startup URLs and extensions to be returned by member functions
    // based on which settings should require reset.
    startup_urls_.push_back(GURL(kStartupUrl1));
    startup_urls_.push_back(GURL(kStartupUrl2));
    if (settings_to_reset.find(SettingType::STARTUP_PAGE) !=
        settings_to_reset.end())
      startup_urls_to_reset_ = startup_urls_;

    if (settings_to_reset.find(SettingType::EXTENSIONS) !=
        settings_to_reset.end()) {
      extensions_ = {{kExtensionId1, safe_browsing::ExtensionInfo(
                                         kExtensionId1, kExtensionName1)},
                     {kExtensionId2, safe_browsing::ExtensionInfo(
                                         kExtensionId2, kExtensionName2)}};
    }

    ON_CALL(*this, PerformReset(_)).WillByDefault(Return());

    ON_CALL(*this, ShouldPromptForReset())
        .WillByDefault(Return(!settings_to_reset.empty()));

    ON_CALL(*this, homepage()).WillByDefault(Return(GURL(kHomepageUrl)));
    ON_CALL(*this, homepage_reset_state())
        .WillByDefault(
            Return(ChooseResetState(settings_to_reset, SettingType::HOMEPAGE)));

    ON_CALL(*this, default_search()).WillByDefault(Return(GURL(kSearchUrl)));
    ON_CALL(*this, default_search_reset_state())
        .WillByDefault(Return(ChooseResetState(
            settings_to_reset, SettingType::DEFAULT_SEARCH_ENGINE)));

    ON_CALL(*this, startup_urls()).WillByDefault(ReturnRef(startup_urls_));
    ON_CALL(*this, startup_urls_to_reset())
        .WillByDefault(ReturnRef(startup_urls_to_reset_));
    ON_CALL(*this, startup_urls_reset_state())
        .WillByDefault(Return(
            ChooseResetState(settings_to_reset, SettingType::STARTUP_PAGE)));

    ON_CALL(*this, extensions_to_disable())
        .WillByDefault(ReturnRef(extensions_));
  }
  ~MockSettingsResetPromptModel() override {}

  MOCK_METHOD1(PerformReset, void(const base::Closure&));
  MOCK_CONST_METHOD0(ShouldPromptForReset, bool());
  MOCK_CONST_METHOD0(homepage, GURL());
  MOCK_CONST_METHOD0(homepage_reset_state, ResetState());
  MOCK_CONST_METHOD0(default_search, GURL());
  MOCK_CONST_METHOD0(default_search_reset_state, ResetState());
  MOCK_CONST_METHOD0(startup_urls, const std::vector<GURL>&());
  MOCK_CONST_METHOD0(startup_urls_to_reset, const std::vector<GURL>&());
  MOCK_CONST_METHOD0(startup_urls_reset_state, ResetState());
  MOCK_CONST_METHOD0(extensions_to_disable, const ExtensionMap&());

 private:
  static ResetState ChooseResetState(const std::set<SettingType>& settings,
                                     SettingType setting) {
    if (settings.find(setting) != settings.end())
      return RESET_REQUIRED;
    return NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED;
  }

  std::vector<GURL> startup_urls_;
  std::vector<GURL> startup_urls_to_reset_;
  ExtensionMap extensions_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsResetPromptModel);
};

class SettingsResetPromptDialogTest : public DialogBrowserTest {
 public:
  void ShowDialog(const std::string& name) override {
    const std::map<std::string, std::set<SettingType>> name_to_settings = {
        {"dse", {SettingType::DEFAULT_SEARCH_ENGINE}},
        {"sp", {SettingType::STARTUP_PAGE}},
        {"hp", {SettingType::HOMEPAGE}},
        {"dse_sp",
         {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::STARTUP_PAGE}},
        {"dse_hp", {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::HOMEPAGE}},
        {"sp_hp", {SettingType::STARTUP_PAGE, SettingType::HOMEPAGE}},
        {"dse_sp_hp",
         {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::STARTUP_PAGE,
          SettingType::HOMEPAGE}},
        {"dse_ext",
         {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::EXTENSIONS}},
        {"sp_ext", {SettingType::STARTUP_PAGE, SettingType::EXTENSIONS}},
        {"hp_ext", {SettingType::HOMEPAGE, SettingType::EXTENSIONS}},
        {"dse_sp_ext",
         {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::STARTUP_PAGE,
          SettingType::EXTENSIONS}},
        {"dse_hp_ext",
         {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::HOMEPAGE,
          SettingType::EXTENSIONS}},
        {"sp_hp_ext",
         {SettingType::STARTUP_PAGE, SettingType::HOMEPAGE,
          SettingType::EXTENSIONS}},
        {"dse_sp_hp_ext",
         {SettingType::DEFAULT_SEARCH_ENGINE, SettingType::STARTUP_PAGE,
          SettingType::HOMEPAGE, SettingType::EXTENSIONS}}};

    auto model = base::MakeUnique<NiceMock<MockSettingsResetPromptModel>>(
        browser()->profile(), name_to_settings.find(name)->second);

    safe_browsing::SettingsResetPromptController::ShowSettingsResetPrompt(
        browser(),
        new safe_browsing::SettingsResetPromptController(std::move(model)));
  }
};

IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_hp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_sp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_hp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp_hp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_sp_hp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_ext) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp_ext) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_hp_ext) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_sp_ext) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_hp_ext) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp_hp_ext) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest,
                       InvokeDialog_dse_sp_hp_ext) {
  RunDialog();
}

}  // namespace
