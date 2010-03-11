// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DIR_READER_POSIX_H_
#define BASE_DIR_READER_POSIX_H_

#include "build/build_config.h"

// This header provides a class, DirReaderPosix, which allows one to open and
// read from directories without allocating memory. For the interface, see
// the generic fallback in dir_reader_fallback.h.

#if defined(OS_LINUX)
#include "base/dir_reader_linux.h"
#else
#include "base/dir_reader_fallback.h"
#endif

namespace base {

#if defined(OS_LINUX)
typedef DirReaderLinux DirReaderPosix;
#else
typedef DirReaderFallback DirReaderPosix;
#endif

}  // namespace base

#endif // BASE_DIR_READER_POSIX_H_
