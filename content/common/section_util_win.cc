// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/section_util_win.h"

namespace chrome {

HANDLE GetSectionFromProcess(HANDLE section, HANDLE process, bool read_only) {
  HANDLE valid_section = NULL;
  DWORD access = STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ;
  if (!read_only)
    access |= FILE_MAP_WRITE;
  DuplicateHandle(process, section, GetCurrentProcess(), &valid_section, access,
                  FALSE, 0);
  return valid_section;
}

HANDLE GetSectionForProcess(HANDLE section, HANDLE process, bool read_only) {
  HANDLE valid_section = NULL;
  DWORD access = STANDARD_RIGHTS_REQUIRED | FILE_MAP_READ;
  if (!read_only)
    access |= FILE_MAP_WRITE;
  DuplicateHandle(GetCurrentProcess(), section, process, &valid_section, access,
                  FALSE, 0);
  return valid_section;
}

}  // namespace chrome
