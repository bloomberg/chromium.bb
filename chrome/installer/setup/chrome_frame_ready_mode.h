// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file contains the specification of setup main functions.

#ifndef CHROME_INSTALLER_SETUP_CHROME_FRAME_READY_MODE_H_
#define CHROME_INSTALLER_SETUP_CHROME_FRAME_READY_MODE_H_
#pragma once

class CommandLine;

namespace installer {

enum InstallStatus;
class InstallationState;
class InstallerState;

// Removes the ChromeFrameReadyMode flag from the registry, updates Chrome's
// uninstallation commands to only uninstall Chrome, and adds an entry to the
// Add/Remove Programs list for GCF.
InstallStatus ChromeFrameReadyModeOptIn(const InstallationState& machine_state,
                                        const InstallerState& installer_state);

// Unregisters the ChromeFrame user agent modification, sets a timestamp for
// restoring it.
InstallStatus ChromeFrameReadyModeTempOptOut(
    const InstallationState& machine_state,
    const InstallerState& installer_state);

// Re-registers the ChromeFrame user agent modification, restores Ready Mode
// active state flag.
InstallStatus ChromeFrameReadyModeEndTempOptOut(
    const InstallationState& machine_state,
    const InstallerState& installer_state);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_CHROME_FRAME_READY_MODE_H_
