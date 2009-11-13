// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file declares util functions for setup project.

#ifndef CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
#define CHROME_INSTALLER_SETUP_SETUP_UTIL_H_

#include "base/command_line.h"
#include "base/values.h"
#include "chrome/installer/util/version.h"

namespace setup_util {
  // Apply a diff patch to source file. First tries to apply it using courgette
  // since it checks for courgette header and fails quickly. If that fails
  // tries to apply the patch using regular bsdiff. Returns status code.
  int ApplyDiffPatch(const std::wstring& src,
                     const std::wstring& patch,
                     const std::wstring& dest);

  // Parse command line and read master preferences, if present, to get
  // distribution related install options. Merge them if any command line
  // options present (command line value takes precedence).
  DictionaryValue* GetInstallPreferences(const CommandLine& cmd_line);

  // Find the version of Chrome from an install source directory.
  // Chrome_path should contain a version folder.
  // Returns the first version found or NULL if no version is found.
  installer::Version* GetVersionFromDir(const std::wstring& chrome_path);
}  // namespace setup_util

#endif  // CHROME_INSTALLER_SETUP_SETUP_UTIL_H_
