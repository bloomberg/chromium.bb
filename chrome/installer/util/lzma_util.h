// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_INSTALLER_UTIL_LZMA_UTIL_H_
#define CHROME_INSTALLER_UTIL_LZMA_UTIL_H_

#include <windows.h>

#include <set>
#include <string>

#include "base/basictypes.h"

namespace base {
class FilePath;
}

// This is a utility class that acts as a wrapper around LZMA SDK library
class LzmaUtil {
 public:
  // Utility method that does the job of calling OpenArchive(), UnPack()
  // and CloseArchive() in order. Returns error code (NO_ERROR if successful).
  static int32 UnPackArchive(const std::wstring& archive,
                             const std::wstring& output_dir,
                             std::wstring* output_file);

  LzmaUtil();
  ~LzmaUtil();

  DWORD OpenArchive(const std::wstring& archivePath);

  // Unpacks the archive to the given location
  DWORD UnPack(const std::wstring& location);

  // Unpacks the archive to the given location and returns the last file
  // extracted from archive. |single_file| is set to true iff only a single
  // file is extracted from archive.
  DWORD UnPack(const std::wstring& location,
               std::wstring* output_file);

  void CloseArchive();

 protected:
  bool CreateDirectory(const base::FilePath& dir);

 private:
  HANDLE archive_handle_;
  std::set<std::wstring> directories_created_;

  DISALLOW_COPY_AND_ASSIGN(LzmaUtil);
};

#endif  // CHROME_INSTALLER_UTIL_LZMA_UTIL_H_
