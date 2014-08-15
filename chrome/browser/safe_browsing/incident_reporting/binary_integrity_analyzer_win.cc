// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/binary_integrity_analyzer.h"

#include "base/files/file_path.h"
#include "base/logging.h"
#include "base/path_service.h"
#include "version.h"  // NOLINT

namespace safe_browsing {

std::vector<base::FilePath> GetCriticalBinariesPath() {
  static const base::FilePath::CharType* const kUnversionedFiles[] = {
      FILE_PATH_LITERAL("chrome.exe"),
  };
  static const base::FilePath::CharType* const kVersionedFiles[] = {
      FILE_PATH_LITERAL("chrome.dll"),
      FILE_PATH_LITERAL("chrome_child.dll"),
      FILE_PATH_LITERAL("chrome_elf.dll"),
  };

  // Find where chrome.exe is installed.
  base::FilePath chrome_exe_dir;
  if (!PathService::Get(base::DIR_EXE, &chrome_exe_dir))
    NOTREACHED();

  std::vector<base::FilePath> critical_binaries;
  critical_binaries.reserve(arraysize(kUnversionedFiles) +
                            arraysize(kVersionedFiles));

  for (size_t i = 0; i < arraysize(kUnversionedFiles); ++i) {
    critical_binaries.push_back(chrome_exe_dir.Append(kUnversionedFiles[i]));
  }

  base::FilePath version_dir(
      chrome_exe_dir.AppendASCII(CHROME_VERSION_STRING));
  for (size_t i = 0; i < arraysize(kVersionedFiles); ++i) {
    critical_binaries.push_back(version_dir.Append(kVersionedFiles[i]));
  }

  return critical_binaries;
}

}  // namespace safe_browsing
