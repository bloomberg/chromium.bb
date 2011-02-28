// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_CHROME_MAIN_H_
#define CHROME_APP_CHROME_MAIN_H_
#pragma once

#include "build/build_config.h"

// See
//   http://dev.chromium.org/developers/design-documents/startup
// for a discussion of the various pieces involved in startup.
// This header is for pulling some platform-specific initialization
// parts out of chrome_main.cc and into chrome_main_*.cc.

class FilePath;

namespace chrome_main {

// Perform low-level initialization that occurs before we set up any
// objects or even consider the command line; for example, make us
// abort if we run out of memory.
void LowLevelInit();

// Checks if the UserDataDir policy has been set and returns its value in the
// |user_data_dir| parameter. If no policy is set the parameter is not changed.
void CheckUserDataDirPolicy(FilePath* user_data_dir);

}  // namespace chrome_main

#endif  // CHROME_APP_CHROME_MAIN_H_
