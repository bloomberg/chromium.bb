// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELF_THIRD_PARTY_DLLS_PACKED_LIST_FILE_H_
#define CHROME_ELF_THIRD_PARTY_DLLS_PACKED_LIST_FILE_H_

#include <windows.h>

#include <string>

namespace third_party_dlls {

// "static_cast<int>(FileStatus::value)" to access underlying value.
enum class FileStatus {
  kSuccess = 0,
  kUserDataDirFail = 1,
  kFileNotFound = 2,
  kFileAccessDenied = 3,
  kFileUnexpectedFailure = 4,
  kMetadataReadFail = 5,
  kInvalidFormatVersion = 6,
  kArraySizeZero = 7,
  kArrayTooBig = 8,
  kArrayReadFail = 9,
  kArrayNotSorted = 10,
  COUNT
};

// Look up a binary based on the required data points.
// - Returns true if match found in the list.
bool IsModuleListed(const std::string& basename,
                    DWORD image_size,
                    DWORD time_date_stamp);

// Get the full path of the blacklist file used.
std::wstring GetBlFilePathUsed();

// Initialize internal module list from file.
FileStatus InitFromFile();

// Overrides the blacklist path for use by tests.
void OverrideFilePathForTesting(const std::wstring& new_bl_path);

// Removes initialization for use by tests.
void DeinitFromFileForTesting();

}  // namespace third_party_dlls

#endif  // CHROME_ELF_THIRD_PARTY_DLLS_PACKED_LIST_FILE_H_
