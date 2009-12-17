// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains cross-platform basic type definitions

#ifndef GPU_COMMAND_BUFFER_COMMON_TYPES_H_
#define GPU_COMMAND_BUFFER_COMMON_TYPES_H_

#include <build/build_config.h>
#if !defined(COMPILER_MSVC)
#include <stdint.h>
#endif
#include <string>

namespace gpu {
#if defined(COMPILER_MSVC)
typedef short Int16;
typedef unsigned short Uint16;
typedef int Int32;
typedef unsigned int Uint32;
#else
typedef int16_t Int16;
typedef uint16_t Uint16;
typedef int32_t Int32;
typedef uint32_t Uint32;
#endif

typedef std::string String;
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_COMMON_TYPES_H_
