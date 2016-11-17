// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_INSTALLER_ERRORS_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_INSTALLER_ERRORS_H_

#include "components/update_client/update_client.h"

namespace component_updater {

// Errors specific to the Flash installer.
enum class FlashError {
  MISSING_VERSION_IN_MANIFEST = 1,
  HINT_FILE_RECORD_ERROR = 2,
};

update_client::CrxInstaller::Result ToInstallerResult(const FlashError& err) {
  return update_client::CrxInstaller::Result(
      static_cast<int>(update_client::InstallError::CUSTOM_ERROR_BASE) +
      static_cast<int>(err));
}

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_COMPONENT_INSTALLER_ERRORS_H_
