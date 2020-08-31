// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/platform_keys/mock_platform_keys_service.h"

namespace chromeos {
namespace platform_keys {

MockPlatformKeysService::MockPlatformKeysService() = default;
MockPlatformKeysService::~MockPlatformKeysService() = default;

std::unique_ptr<KeyedService> BuildMockPlatformKeysService(
    content::BrowserContext*) {
  return std::make_unique<MockPlatformKeysService>();
}

}  // namespace platform_keys
}  // namespace chromeos
