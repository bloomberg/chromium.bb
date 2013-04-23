// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CLOUD_PRINT_SERVICE_INSTALLER_H_
#define CLOUD_PRINT_SERVICE_INSTALLER_H_

#include <wtypes.h>

// Installs/uninstalls product if related switches provided.
// Returns S_OK on success or error code.
// Returns S_FALE if no related switches were provided.
HRESULT ProcessInstallerSwitches();

#endif  // CLOUD_PRINT_SERVICE_INSTALLER_H_