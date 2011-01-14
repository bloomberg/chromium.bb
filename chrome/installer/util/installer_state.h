// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
#define CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/installer/util/browser_distribution.h"

namespace installer {

class InstallationState;
class MasterPreferences;

// Encapsulates the state of the current installation operation.  Only valid
// for installs and upgrades (not for uninstalls or non-install commands)
class InstallerState {
 public:
  enum Operation {
    UNINITIALIZED,
    SINGLE_INSTALL_OR_UPDATE,
    MULTI_INSTALL,
    MULTI_UPDATE,
  };

  InstallerState();

  // Initializes |this| based on the current operation.
  void Initialize(const MasterPreferences& prefs,
                  const InstallationState& machine_state);

  // true if system-level, false if user-level.
  bool system_install() const { return system_install_; }

  Operation operation() const { return operation_; }

  // The ClientState key by which we interact with Google Update.
  const std::wstring& state_key() const { return state_key_; }

 protected:
  bool IsMultiInstallUpdate(const MasterPreferences& prefs,
                            const InstallationState& machine_state);

  Operation operation_;
  std::wstring state_key_;
  bool system_install_;

 private:
  DISALLOW_COPY_AND_ASSIGN(InstallerState);
};  // class InstallerState

}  // namespace installer

#endif  // CHROME_INSTALLER_UTIL_INSTALLER_STATE_H_
