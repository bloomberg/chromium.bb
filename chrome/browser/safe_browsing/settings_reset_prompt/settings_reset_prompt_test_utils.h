// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_TEST_UTILS_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_TEST_UTILS_H_

#include <memory>
#include <string>
#include <unordered_set>

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

class Profile;

namespace safe_browsing {

class SettingsResetPromptModel;

class MockSettingsResetPromptConfig : public SettingsResetPromptConfig {
 public:
  MockSettingsResetPromptConfig();
  ~MockSettingsResetPromptConfig() override;

  MOCK_CONST_METHOD1(UrlToResetDomainId, int(const GURL& URL));
};

// Returns a |SettingsResetPromptModel| with a mock |SettingsResetPromptConfig|
// that will return positive reset domain IDs for each URL in |reset_urls| and
// negative IDs otherwise.
std::unique_ptr<SettingsResetPromptModel> CreateModelForTesting(
    Profile* profile,
    const std::unordered_set<std::string>& reset_urls);

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_SETTINGS_RESET_PROMPT_TEST_UTILS_H_
