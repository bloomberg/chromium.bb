// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IPC_IPC_PLATFORM_FILE_H_
#define IPC_IPC_PLATFORM_FILE_H_

#include "base/basictypes.h"

#include "base/platform_file.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

namespace IPC {

#if defined(OS_WIN)
typedef base::PlatformFile PlatformFileForTransit;
#elif defined(OS_POSIX)
typedef base::FileDescriptor PlatformFileForTransit;
#endif

inline PlatformFileForTransit InvalidPlatformFileForTransit() {
#if defined(OS_WIN)
  return base::kInvalidPlatformFileValue;
#elif defined(OS_POSIX)
  return base::FileDescriptor();
#endif
}

inline base::PlatformFile PlatformFileForTransitToPlatformFile(
    const PlatformFileForTransit& transit) {
#if defined(OS_WIN)
  return transit;
#elif defined(OS_POSIX)
  return transit.fd;
#endif
}

}  // namespace IPC

#endif  // IPC_IPC_PLATFORM_FILE_H_
