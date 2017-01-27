// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/settings_reset_prompt/mock_settings_reset_prompt_config.h"

using testing::_;
using testing::Return;

namespace safe_browsing {

MockSettingsResetPromptConfig::MockSettingsResetPromptConfig() {
  // Define a default return value of -1 for |UrlToResetDomainId()|.
  EXPECT_CALL(*this, UrlToResetDomainId(_)).WillRepeatedly(Return(-1));
}

MockSettingsResetPromptConfig::~MockSettingsResetPromptConfig() {}

}  // namespace safe_browsing
