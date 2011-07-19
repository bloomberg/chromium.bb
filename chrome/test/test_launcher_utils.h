// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_TEST_LAUNCHER_UTILS_H_
#define CHROME_TEST_TEST_LAUNCHER_UTILS_H_
#pragma once

#include <string>

#include "base/compiler_specific.h"
#include "base/file_path.h"

class CommandLine;

// A set of utilities for test code that launches separate processes.
namespace test_launcher_utils {

// Appends browser switches to provided |command_line| to be used
// when running under tests.
void PrepareBrowserCommandLineForTests(CommandLine* command_line);

// Overrides the current process' user data dir.
bool OverrideUserDataDir(const FilePath& user_data_dir) WARN_UNUSED_RESULT;

// Override the GL implementation. The names are the same as for the --use-gl
// command line switch. Use the constants in the gfx namespace.
bool OverrideGLImplementation(
    CommandLine* command_line,
    const std::string& implementation_name) WARN_UNUSED_RESULT;

// Returns test termination tiemout based on test name
// and a default timeout. Makes it possible to override
// the default timeout using command-line or test prefix.
int GetTestTerminationTimeout(const std::string& test_name,
                              int default_timeout_ms);

}  // namespace test_launcher_utils

#endif  // CHROME_TEST_TEST_LAUNCHER_UTILS_H_
