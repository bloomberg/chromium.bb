// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/whitelist/whitelist_file.h"

#include "windows.h"

#include <assert.h>
#include <stdio.h>

#include <algorithm>
#include <limits>

#include "chrome/install_static/user_data_dir.h"
#include "chrome_elf/sha1/sha1.h"
#include "chrome_elf/whitelist/whitelist_file_format.h"

namespace whitelist {
namespace {

// TODO(pennymac): update subdir and filename with final shared defines.
constexpr wchar_t kFileSubdir[] =
    L"\\ThirdPartyModuleList"
#if defined(_WIN64)
    L"64";
#else
    L"32";
#endif
constexpr wchar_t kFileName[] = L"\\dbfile";

// No concern about concurrency control in chrome_elf.
bool g_initialized = false;

// This will hold a packed whitelist module array, read directly from a
// component-update file during InitComponent().
PackedWhitelistModule* g_module_array = nullptr;
size_t g_module_array_size = 0;

// NOTE: this "global" is only initialized once during InitComponent().
// NOTE: this is wrapped in a function to prevent exit-time dtors.
std::wstring& GetFilePath() {
  static std::wstring* const file_path = new std::wstring();
  return *file_path;
}

//------------------------------------------------------------------------------
// Private functions
//------------------------------------------------------------------------------

// Returns -1 if |first| < |second|
// Returns 0 if |first| == |second|
// Returns 1 if |first| > |second|
int CompareHashes(const uint8_t* first, const uint8_t* second) {
  // Compare bytes, high-order to low-order.
  for (size_t i = 0; i < elf_sha1::kSHA1Length; ++i) {
    if (first[i] < second[i])
      return -1;
    if (first[i] > second[i])
      return 1;
    // else they are equal, continue;
  }

  return 0;
}

// Binary predicate compare function for use with
// std::equal_range/std::is_sorted. Must return TRUE if lhs < rhs.
bool HashBinaryPredicate(const PackedWhitelistModule& lhs,
                         const PackedWhitelistModule& rhs) {
  return CompareHashes(lhs.basename_hash, rhs.basename_hash) < 0;
}

// Given a file opened for read, pull in the packed whitelist.
//
// - Returns kSuccess or kArraySizeZero on success.
FileStatus ReadInArray(HANDLE file) {
  PackedWhitelistMetadata metadata;
  DWORD bytes_read = 0;

  if (!::ReadFile(file, &metadata, sizeof(PackedWhitelistMetadata), &bytes_read,
                  FALSE) ||
      bytes_read != sizeof(PackedWhitelistMetadata)) {
    return FileStatus::kMetadataReadFail;
  }

  // Careful of versioning.  For now, only support the latest version.
  if (metadata.version != PackedWhitelistVersion::kCurrent)
    return FileStatus::kInvalidFormatVersion;

  g_module_array_size = metadata.module_count;
  // Check for size 0.
  if (!g_module_array_size)
    return FileStatus::kArraySizeZero;

  // Sanity check the array fits in a DWORD.
  if (g_module_array_size >
      (std::numeric_limits<DWORD>::max() / sizeof(PackedWhitelistModule))) {
    assert(false);
    return FileStatus::kArrayTooBig;
  }

  DWORD buffer_size =
      static_cast<DWORD>(g_module_array_size * sizeof(PackedWhitelistModule));
  g_module_array =
      reinterpret_cast<PackedWhitelistModule*>(new uint8_t[buffer_size]);

  // Read in the array.
  // NOTE: Ignore the rest of the file - other data could be stored at the end.
  if (!::ReadFile(file, g_module_array, buffer_size, &bytes_read, FALSE) ||
      bytes_read != buffer_size) {
    delete[] g_module_array;
    g_module_array = nullptr;
    g_module_array_size = 0;
    return FileStatus::kArrayReadFail;
  }
  // TODO(pennymac): calculate cost of is_sorted() call against real database
  // array. Is it too expensive for startup?  Maybe do a delayed check?
  if (!std::is_sorted(g_module_array, g_module_array + g_module_array_size,
                      HashBinaryPredicate)) {
    delete[] g_module_array;
    g_module_array = nullptr;
    g_module_array_size = 0;
    return FileStatus::kArrayNotSorted;
  }

  return FileStatus::kSuccess;
}

// Example file location, relative to user data dir.
// %localappdata% / Google / Chrome SxS / User Data / ThirdPartyModuleList64 /
//
// TODO(pennymac): Missing or malformed component file shouldn't happen.
//                 Possible UMA log in future.
FileStatus InitInternal() {
  std::wstring& file_path = GetFilePath();
  // The path may have been overridden for testing.
  if (file_path.empty()) {
    if (!install_static::GetUserDataDirectory(&file_path, nullptr))
      return FileStatus::kUserDataDirFail;
    file_path.append(kFileSubdir);
    file_path.append(kFileName);
  }

  // See if file exists.  INVALID_HANDLE_VALUE alert!
  HANDLE file =
      ::CreateFileW(file_path.c_str(), FILE_READ_DATA,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE)
    return FileStatus::kFileNotFound;

  FileStatus status = ReadInArray(file);
  ::CloseHandle(file);

  return status;
}

}  // namespace

//------------------------------------------------------------------------------
// Public defines & functions
//------------------------------------------------------------------------------

bool IsModuleWhitelisted(const std::string& basename,
                         DWORD image_size,
                         DWORD time_date_stamp) {
  assert(g_initialized);
  if (!g_module_array_size)
    return false;

  // Max hex 32-bit value is 8 characters long.  2*8+1.
  char buffer[17] = {};
  ::snprintf(buffer, sizeof(buffer), "%08lX%lx", time_date_stamp, image_size);
  std::string code_id(buffer);
  code_id = elf_sha1::SHA1HashString(code_id);
  std::string basename_hash = elf_sha1::SHA1HashString(basename);
  PackedWhitelistModule target = {};
  ::memcpy(target.basename_hash, basename_hash.data(), elf_sha1::kSHA1Length);
  ::memcpy(target.code_id_hash, code_id.data(), elf_sha1::kSHA1Length);

  // Binary search for primary hash (basename).  There can be more than one
  // match.
  auto pair =
      std::equal_range(g_module_array, g_module_array + g_module_array_size,
                       target, HashBinaryPredicate);

  // Search for secondary hash.
  for (PackedWhitelistModule* i = pair.first; i != pair.second; ++i) {
    if (!CompareHashes(target.code_id_hash, i->code_id_hash))
      return true;
  }

  // No match.
  return false;
}

std::wstring GetFilePathUsed() {
  return GetFilePath();
}

// Grab the latest whitelist file.
FileStatus InitFromFile() {
  // Debug check: InitFromFile should not be called more than once.
  assert(!g_initialized);

  // TODO(pennymac): Should kArraySizeZero be a failure?  Or just UMA?
  FileStatus status = InitInternal();

  if (status == FileStatus::kSuccess)
    g_initialized = true;

  return status;
}

void OverrideFilePathForTesting(const std::wstring& new_path) {
  GetFilePath().assign(new_path);
}

}  // namespace whitelist
