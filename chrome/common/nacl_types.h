// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_NACL_TYPES_H_
#define CHROME_COMMON_NACL_TYPES_H_
#pragma once

#include <string>
#include <vector>

#include "build/build_config.h"

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
#endif

#if defined(OS_WIN)
#include <windows.h>   // for HANDLE
#endif

// TODO(gregoryd): add a Windows definition for base::FileDescriptor
namespace nacl {

#if defined(OS_WIN)
// We assume that HANDLE always uses less than 32 bits
typedef int FileDescriptor;
inline HANDLE ToNativeHandle(const FileDescriptor& desc) {
  return reinterpret_cast<HANDLE>(desc);
}
#elif defined(OS_POSIX)
typedef base::FileDescriptor FileDescriptor;
inline int ToNativeHandle(const FileDescriptor& desc) {
  return desc.fd;
}
#endif


// Parameters sent to the NaCl process when we start it.
//
// If you change this, you will also need to update the IPC serialization in
// nacl_messages.h.
struct NaClStartParams {
  NaClStartParams();
  ~NaClStartParams();

  std::vector<FileDescriptor> handles;

  bool validation_cache_enabled;
  std::string validation_cache_key;
  // Chrome version string. Sending the version string over IPC avoids linkage
  // issues in cases where NaCl is not compiled into the main Chromium
  // executable or DLL.
  std::string version;

  bool enable_exception_handling;
};

}  // namespace nacl

#endif  // CHROME_COMMON_NACL_TYPES_H_
