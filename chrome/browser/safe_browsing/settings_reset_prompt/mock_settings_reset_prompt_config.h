// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_MOCK_SETTINGS_RESET_PROMPT_CONFIG_H_
#define CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_MOCK_SETTINGS_RESET_PROMPT_CONFIG_H_

#include "chrome/browser/safe_browsing/settings_reset_prompt/settings_reset_prompt_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace safe_browsing {

class MockSettingsResetPromptConfig : public SettingsResetPromptConfig {
 public:
  MockSettingsResetPromptConfig();
  ~MockSettingsResetPromptConfig() override;

  MOCK_CONST_METHOD1(UrlToResetDomainId, int(const GURL& URL));
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_SETTINGS_RESET_PROMPT_MOCK_SETTINGS_RESET_PROMPT_CONFIG_H_
