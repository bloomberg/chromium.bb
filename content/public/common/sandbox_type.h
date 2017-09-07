// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_TYPE_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_TYPE_H_

#include <string>

#include "base/command_line.h"
#include "content/common/content_export.h"

namespace content {

// Defines the sandbox types known within content. Embedders can add additional
// sandbox types with IDs starting with SANDBOX_TYPE_AFTER_LAST_TYPE.

enum SandboxType {
  // Not a valid sandbox type.
  SANDBOX_TYPE_INVALID = -1,

  SANDBOX_TYPE_FIRST_TYPE = 0,  // Placeholder to ease iteration.

  // Do not apply any sandboxing to the process.
  SANDBOX_TYPE_NO_SANDBOX = SANDBOX_TYPE_FIRST_TYPE,

  // Renderer or worker process. Most common case.
  SANDBOX_TYPE_RENDERER,

  // Utility process is as restrictive as the worker process except full
  // access is allowed to one configurable directory.
  SANDBOX_TYPE_UTILITY,

  // GPU process.
  SANDBOX_TYPE_GPU,

  // The PPAPI plugin process.
  SANDBOX_TYPE_PPAPI,

  // The network process.
  SANDBOX_TYPE_NETWORK,

  SANDBOX_TYPE_AFTER_LAST_TYPE,  // Placeholder to ease iteration.
};

inline bool IsUnsandboxedSandboxType(SandboxType sandbox_type) {
  // TODO(tsepez): Sandbox network process.
  return sandbox_type == SANDBOX_TYPE_NO_SANDBOX ||
         sandbox_type == SANDBOX_TYPE_NETWORK;
}

CONTENT_EXPORT SandboxType
SandboxTypeFromCommandLine(const base::CommandLine& command_line);

CONTENT_EXPORT SandboxType
UtilitySandboxTypeFromString(const std::string& sandbox_string);

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_TYPE_H_
