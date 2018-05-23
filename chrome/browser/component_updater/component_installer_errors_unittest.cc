// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/component_installer_errors.h"
#include "components/update_client/update_client.h"
#include "components/update_client/update_client_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace component_updater {

enum class CustomInstallerError {
  AN_ERROR = 1,
};

TEST(ComponentInstallerErrors, ToInstallerResult) {
  auto result = ToInstallerResult(CustomInstallerError::AN_ERROR);
  constexpr int kErrorBase =
      static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE);
  EXPECT_EQ(kErrorBase + 1, result.error);
}

}  // namespace component_updater
