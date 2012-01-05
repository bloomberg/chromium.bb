// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_MINI_INSTALLER_TEST_INSTALLER_PATH_PROVIDER_H_
#define CHROME_TEST_MINI_INSTALLER_TEST_INSTALLER_PATH_PROVIDER_H_
#pragma once

#include "base/basictypes.h"
#include "base/file_path.h"

namespace installer_test {

// Locate and provides path for installers.
// Search for latest installer binaries in the filer directory defined by
// mini_installer_constants::kChromeInstallersLocation.
class InstallerPathProvider {
 public:
  // Search for latest installer binaries in filer.
  InstallerPathProvider();

  explicit InstallerPathProvider(const std::string& build_under_test);
  ~InstallerPathProvider();

  bool GetFullInstaller(FilePath* path);
  bool GetDiffInstaller(FilePath* path);
  bool GetMiniInstaller(FilePath* path);
  bool GetPreviousInstaller(FilePath* path);
  bool GetStandaloneInstaller(FilePath* path);
  bool GetSignedStandaloneInstaller(FilePath* path);

  std::string GetCurrentBuild();
  std::string GetPreviousBuild();

 private:
  // Returns the local file path for the given file |name|.
  // Assumes file is located in the current working directory.
  FilePath PathFromExeDir(const FilePath::StringType& name);

  bool GetInstaller(const std::string& pattern, FilePath* path);

  // Build numbers.
  std::string current_build_, previous_build_;

  DISALLOW_COPY_AND_ASSIGN(InstallerPathProvider);
};

}  // namespace

#endif  // CHROME_TEST_MINI_INSTALLER_TEST_INSTALLER_PATH_PROVIDER_H_
