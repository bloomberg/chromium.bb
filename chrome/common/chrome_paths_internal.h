// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
#define CHROME_COMMON_CHROME_PATHS_INTERNAL_H_

#include "build/build_config.h"

#include "base/file_path.h"

namespace chrome {

// Get the path to the user's data directory, regardless of whether
// DIR_USER_DATA has been overridden by a command-line option.
bool GetDefaultUserDataDirectory(FilePath* result);

#if defined(OS_LINUX)
// Get the path to the user's cache directory.
bool GetUserCacheDirectory(FilePath* result);
#endif

// Get the path to the user's documents directory.
bool GetUserDocumentsDirectory(FilePath* result);

// Get the path to the user's downloads directory.
bool GetUserDownloadsDirectory(FilePath* result);

// The path to the user's desktop.
bool GetUserDesktop(FilePath* result);

#if defined(OS_MACOSX)
// Retrieves the browser bundle path.  It is only valid to call this from a
// helper process, as it makes assumptions about the location of the enclosing
// bundle on disk.
FilePath GetBrowserBundlePath();
#endif  // defined(OS_MACOSX)

}  // namespace chrome


#endif  // CHROME_COMMON_CHROME_PATHS_INTERNAL_H_
