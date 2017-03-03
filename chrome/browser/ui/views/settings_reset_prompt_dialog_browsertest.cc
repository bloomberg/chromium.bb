// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <initializer_list>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
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

constexpr char kHomepageUrl[] = "http://www.some-homepage.com/some/path";
constexpr char kSearchUrl[] = "http://www.some-search.com/some/path/?q={%s}";
constexpr const char* kStartupUrls[] = {
    "http://www.some-startup-1.com/some/path",
    "http://www.some-startup-2.com/some/other/path",
    "http://www.some-startup-3.com/some/third/path"};

enum class SettingType {
  DEFAULT_SEARCH_ENGINE,
  STARTUP_PAGE,
  HOMEPAGE,
};

struct ModelParams {
  SettingType setting_to_reset;
  size_t startup_pages;
  size_t extensions_to_disable;
};

class MockSettingsResetPromptModel
    : public safe_browsing::SettingsResetPromptModel {
 public:
  // Create a mock model that pretends that the settings passed in via
  // |settings_to_reset| require resetting. |settings_to_reset| must not be
  // empty or have |EXTENSIONS| as the only member.
  explicit MockSettingsResetPromptModel(Profile* profile,
                                        const ModelParams& params)
      : SettingsResetPromptModel(
            profile,
            base::MakeUnique<NiceMock<MockSettingsResetPromptConfig>>(),
            base::MakeUnique<ResettableSettingsSnapshot>(profile),
            base::MakeUnique<BrandcodedDefaultSettings>(),
            base::MakeUnique<NiceMock<MockProfileResetter>>(profile)) {
    EXPECT_LE(params.startup_pages, arraysize(kStartupUrls));

    // Set up startup URLs and extensions to be returned by member functions
    // based on the constructor arguments.
    for (size_t i = 0;
         i < std::min(arraysize(kStartupUrls), params.startup_pages); ++i) {
      startup_urls_.push_back(GURL(kStartupUrls[i]));
    }

    if (params.setting_to_reset == SettingType::STARTUP_PAGE)
      startup_urls_to_reset_ = startup_urls_;

    for (size_t i = 0; i < params.extensions_to_disable; ++i) {
      std::string id = "id" + base::IntToString(i);
      extensions_.insert(
          std::make_pair(id, safe_browsing::ExtensionInfo(
                                 id, "Extension " + base::IntToString(i))));
    }

    ON_CALL(*this, ShouldPromptForReset()).WillByDefault(Return(true));
    ON_CALL(*this, PerformReset(_)).WillByDefault(Return());
    ON_CALL(*this, DialogShown()).WillByDefault(Return());

    ON_CALL(*this, homepage()).WillByDefault(Return(GURL(kHomepageUrl)));
    ON_CALL(*this, homepage_reset_state())
        .WillByDefault(
            Return(params.setting_to_reset == SettingType::HOMEPAGE
                       ? RESET_REQUIRED
                       : NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED));

    ON_CALL(*this, default_search()).WillByDefault(Return(GURL(kSearchUrl)));
    ON_CALL(*this, default_search_reset_state())
        .WillByDefault(
            Return(params.setting_to_reset == SettingType::DEFAULT_SEARCH_ENGINE
                       ? RESET_REQUIRED
                       : NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED));

    ON_CALL(*this, startup_urls()).WillByDefault(ReturnRef(startup_urls_));
    ON_CALL(*this, startup_urls_to_reset())
        .WillByDefault(ReturnRef(startup_urls_to_reset_));
    ON_CALL(*this, startup_urls_reset_state())
        .WillByDefault(
            Return(params.setting_to_reset == SettingType::STARTUP_PAGE
                       ? RESET_REQUIRED
                       : NO_RESET_REQUIRED_DUE_TO_DOMAIN_NOT_MATCHED));

    ON_CALL(*this, extensions_to_disable())
        .WillByDefault(ReturnRef(extensions_));
  }
  ~MockSettingsResetPromptModel() override {}

  MOCK_METHOD1(PerformReset, void(const base::Closure&));
  MOCK_CONST_METHOD0(ShouldPromptForReset, bool());
  MOCK_METHOD0(DialogShown, void());
  MOCK_CONST_METHOD0(homepage, GURL());
  MOCK_CONST_METHOD0(homepage_reset_state, ResetState());
  MOCK_CONST_METHOD0(default_search, GURL());
  MOCK_CONST_METHOD0(default_search_reset_state, ResetState());
  MOCK_CONST_METHOD0(startup_urls, const std::vector<GURL>&());
  MOCK_CONST_METHOD0(startup_urls_to_reset, const std::vector<GURL>&());
  MOCK_CONST_METHOD0(startup_urls_reset_state, ResetState());
  MOCK_CONST_METHOD0(extensions_to_disable, const ExtensionMap&());

 private:
  std::vector<GURL> startup_urls_;
  std::vector<GURL> startup_urls_to_reset_;
  ExtensionMap extensions_;

  DISALLOW_COPY_AND_ASSIGN(MockSettingsResetPromptModel);
};

class SettingsResetPromptDialogTest : public DialogBrowserTest {
 public:
  void ShowDialog(const std::string& name) override {
    const std::map<std::string, ModelParams> name_to_model_params = {
        {"dse", {SettingType::DEFAULT_SEARCH_ENGINE, 0, 0}},
        {"sp1", {SettingType::STARTUP_PAGE, 1, 0}},
        {"sp2", {SettingType::STARTUP_PAGE, 2, 0}},
        {"hp", {SettingType::HOMEPAGE, 0, 0}},
        {"dse_ext1", {SettingType::DEFAULT_SEARCH_ENGINE, 0, 1}},
        {"sp1_ext1", {SettingType::STARTUP_PAGE, 1, 1}},
        {"sp2_ext1", {SettingType::STARTUP_PAGE, 2, 1}},
        {"hp_ext1", {SettingType::HOMEPAGE, 0, 1}},
        {"dse_ext2", {SettingType::DEFAULT_SEARCH_ENGINE, 0, 2}},
        {"sp1_ext2", {SettingType::STARTUP_PAGE, 1, 2}},
        {"sp2_ext2", {SettingType::STARTUP_PAGE, 2, 2}},
        {"hp_ext2", {SettingType::HOMEPAGE, 0, 2}},
    };

    ASSERT_NE(name_to_model_params.find(name), name_to_model_params.end());
    auto model = base::MakeUnique<NiceMock<MockSettingsResetPromptModel>>(
        browser()->profile(), name_to_model_params.find(name)->second);

    safe_browsing::SettingsResetPromptController::ShowSettingsResetPrompt(
        browser(),
        new safe_browsing::SettingsResetPromptController(std::move(model)));
  }
};

IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp1) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp2) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_hp) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_ext1) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp1_ext1) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp2_ext1) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_hp_ext1) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_dse_ext2) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp1_ext2) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_sp2_ext2) {
  RunDialog();
}
IN_PROC_BROWSER_TEST_F(SettingsResetPromptDialogTest, InvokeDialog_hp_ext2) {
  RunDialog();
}

}  // namespace
