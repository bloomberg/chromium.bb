// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_INSTALL_VERIFICATION_WIN_LOADED_MODULES_SNAPSHOT_H_
#define CHROME_BROWSER_INSTALL_VERIFICATION_WIN_LOADED_MODULES_SNAPSHOT_H_

#include <Windows.h>

#include <vector>

// Takes a snapshot of the modules loaded in the current process. The returned
// HMODULEs are not add-ref'd, so they should not be closed and may be
// invalidated at any time (should a module be unloaded).
bool GetLoadedModulesSnapshot(std::vector<HMODULE>* snapshot);

#endif  // CHROME_BROWSER_INSTALL_VERIFICATION_WIN_LOADED_MODULES_SNAPSHOT_H_
