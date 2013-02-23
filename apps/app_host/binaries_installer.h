// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_HOST_BINARIES_INSTALLER_H_
#define APPS_APP_HOST_BINARIES_INSTALLER_H_

#include <windows.h>

namespace app_host {

// Attempts to install the Chrome Binaries. Blocks until the installation
// process is complete. The AP value, if any, from the currently installed App
// Host will be used.
HRESULT InstallBinaries();

}  // namespace app_host

#endif  // APPS_APP_HOST_BINARIES_INSTALLER_H_
