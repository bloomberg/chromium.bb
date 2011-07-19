// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Support for Chrome Frame quick enable.

#ifndef CHROME_INSTALLER_SETUP_CHROME_FRAME_QUICK_ENABLE_H_
#define CHROME_INSTALLER_SETUP_CHROME_FRAME_QUICK_ENABLE_H_
#pragma once

#include "chrome/installer/util/util_constants.h"

namespace installer {

class InstallationState;
class InstallerState;

// Installs Chrome Frame in the same location as Chrome is currently installed
// in.  Setup is assumed to be running from the Installer directory.
InstallStatus ChromeFrameQuickEnable(const InstallationState& machine_state,
                                     InstallerState* installer_state);

}  // namespace installer

#endif  // CHROME_INSTALLER_SETUP_CHROME_FRAME_QUICK_ENABLE_H_
