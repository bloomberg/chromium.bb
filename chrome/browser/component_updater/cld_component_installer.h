// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPONENT_UPDATER_CLD_COMPONENT_INSTALLER_H_
#define CHROME_BROWSER_COMPONENT_UPDATER_CLD_COMPONENT_INSTALLER_H_

#include "base/files/file_path.h"

namespace component_updater {

class ComponentUpdateService;

void RegisterCldComponent(ComponentUpdateService* cus);

// Places the path to the latest CLD data file into the specified path object.
// Returns true if and only if the file has been observed to exist at least
// once and was valid when it was observed; if the function returns false, the
// path parameter is not modified. This function is threadsafe.
bool GetLatestCldDataFile(base::FilePath* path);

}  // namespace component_updater

#endif  // CHROME_BROWSER_COMPONENT_UPDATER_CLD_COMPONENT_INSTALLER_H_
