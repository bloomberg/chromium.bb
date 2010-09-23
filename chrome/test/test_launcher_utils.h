// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_LAUNCHER_UTILS_H_
#define CHROME_TEST_TEST_LAUNCHER_UTILS_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/file_path.h"

// A set of utilities for test code that launches separate processes.
namespace test_launcher_utils {

// Overrides the current process' user data dir.
bool OverrideUserDataDir(const FilePath& user_data_dir) WARN_UNUSED_RESULT;

}  // namespace test_launcher_utils

#endif  // CHROME_TEST_TEST_LAUNCHER_UTILS_H_
