// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// File compatibility routines. Useful to delete database files with.

#ifndef CHROME_BROWSER_SYNC_UTIL_COMPAT_FILE_H_
#define CHROME_BROWSER_SYNC_UTIL_COMPAT_FILE_H_

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "build/build_config.h"
#include "chrome/browser/sync/util/sync_types.h"

extern const PathChar* const kPathSeparator;

// Path calls for all OSes.
// Returns 0 on success, non-zero on failure.
int PathRemove(const PathString& path);

#if defined(OS_WIN)
inline int PathRemove(const PathString& path) {
  return _wremove(path.c_str());
}
#elif (defined(OS_MACOSX) || defined(OS_LINUX))
inline int PathRemove(const PathString& path) {
  return unlink(path.c_str());
}
#endif

#endif  // CHROME_BROWSER_SYNC_UTIL_COMPAT_FILE_H_
