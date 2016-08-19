// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_DISABLE_OUTDATED_BUILD_DETECTOR_H_
#define CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_DISABLE_OUTDATED_BUILD_DETECTOR_H_

#include "chrome/tools/disable_outdated_build_detector/constants.h"

// Disables the outdated build detector for organic installs of Chrome by
// switching the install to the AOHY non-organic brand code. Operates on
// per-user Chrome unless |command_line| contains the "--system-level" switch,
// in which case it operates on per-machine Chrome. The result of the operation
// is written to the registry in Chrome's ClientState key in accordance with the
// Google Update installer result API. Returns the process exit code for the
// operation.
ExitCode DisableOutdatedBuildDetector(const wchar_t* command_line);

#endif  // CHROME_TOOLS_DISABLE_OUTDATED_BUILD_DETECTOR_DISABLE_OUTDATED_BUILD_DETECTOR_H_
