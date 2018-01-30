// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome_elf/third_party_dlls/packed_list_format.h"

#include <stddef.h>

namespace third_party_dlls {

// Subdir relative to install_static::GetUserDataDirectory().
const wchar_t kFileSubdir[] =
    L"\\ThirdPartyModuleList"
#if defined(_WIN64)
    L"64";
#else
    L"32";
#endif

// Packed module data cache file.
const wchar_t kBlFileName[] = L"\\bldata";

uint32_t GetLogEntrySize(uint32_t path_len) {
  // Include padding in the size to fill it to a 32-bit boundary (add one byte
  // for the string terminator and up to three bytes of padding, which will be
  // truncated down to the proper amount).
  return ((offsetof(LogEntry, path) + path_len + 4) & ~3U);
}

}  // namespace third_party_dlls
