// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_DEBUG_FLAGS_H_
#define CONTENT_COMMON_DEBUG_FLAGS_H_

#include "content/public/common/process_type.h"

class CommandLine;

namespace content {

// Updates the command line arguments with debug-related flags. If
// debug flags have been used with this process, they will be
// filtered and added to command_line as needed. is_in_sandbox must
// be true if the child process will be in a sandbox.
//
// Returns true if the caller should "help" the child process by
// calling the JIT debugger on it. It may only happen if
// is_in_sandbox is true.
bool ProcessDebugFlags(CommandLine* command_line,
                       ProcessType type,
                       bool is_in_sandbox);

};  // namespace content

#endif  // CONTENT_COMMON_DEBUG_FLAGS_H_
