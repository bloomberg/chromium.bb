// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
#define CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_

#include <guiddef.h>

#include <vector>

#include "base/strings/string16.h"
#include "base/win/windows_types.h"

namespace base {
class CommandLine;
class FilePath;
}  // namespace base

namespace updater {

bool RegisterUpdateAppsTask(const base::CommandLine& run_command);
void UnregisterUpdateAppsTask();

base::string16 GetComServerClsidRegistryPath(REFCLSID clsid);
base::string16 GetComServiceClsid();
base::string16 GetComServiceClsidRegistryPath();
base::string16 GetComServiceAppidRegistryPath();
base::string16 GetComIidRegistryPath(REFIID iid);
base::string16 GetComTypeLibRegistryPath(REFIID iid);

// Parses the run time dependency file which contains all dependencies of
// the `updater` target. This file is a text file, where each line of
// text represents a single dependency. Some dependencies are not needed for
// updater to run, and are filtered out from the return value of this function.
std::vector<base::FilePath> ParseFilesFromDeps(const base::FilePath& deps);

}  // namespace updater

#endif  // CHROME_UPDATER_WIN_SETUP_SETUP_UTIL_H_
