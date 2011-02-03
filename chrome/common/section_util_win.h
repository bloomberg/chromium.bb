// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SECTION_UTIL_WIN_H_
#define CHROME_COMMON_SECTION_UTIL_WIN_H_
#pragma once

#include <windows.h>

namespace chrome {

// Duplicates a section handle from another process to the current process.
// Returns the new valid handle if the function succeed. NULL otherwise.
HANDLE GetSectionFromProcess(HANDLE section, HANDLE process, bool read_only);

// Duplicates a section handle from the current process for use in another
// process. Returns the new valid handle or NULL on failure.
HANDLE GetSectionForProcess(HANDLE section, HANDLE process, bool read_only);

}  // namespace chrome

#endif  // CHROME_COMMON_SECTION_UTIL_WIN_H_
