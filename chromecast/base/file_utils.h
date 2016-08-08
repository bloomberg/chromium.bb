// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BASE_FILE_UTILS_H_
#define CHROMECAST_BASE_FILE_UTILS_H_

#include "base/files/file_path.h"

namespace chromecast {

// Lock a file (blocking request) with filepath |path|. Returns the file
// descriptor on success, an invalid file descriptor (-1) on failure. Failure
// indicates that |path| does not exist, or the locking procedure failed.
// A true |write| value opens the file in RW mode, false opens in R-only.
int OpenAndLockFile(const base::FilePath& path, bool write);

// Unlock a file (blocking request) denoted by file descriptor |fd|. Returns
// true on success, false on failure.
bool UnlockAndCloseFile(const int fd);

}  // namespace chromecast

#endif  // CHROMECAST_BASE_FILE_UTILS_H_