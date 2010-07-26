// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Handle passing definitions for NaCl
#ifndef CHROME_COMMON_NACL_TYPES_H_
#define CHROME_COMMON_NACL_TYPES_H_
#pragma once

#if defined(OS_POSIX)
#include "base/file_descriptor_posix.h"
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
}

#endif  // CHROME_COMMON_NACL_TYPES_H_
