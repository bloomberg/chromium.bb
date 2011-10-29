// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
#define CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
#pragma once

#include "build/build_config.h"
#include "content/common/content_export.h"

#if defined(OS_WIN)
namespace sandbox {
struct SandboxInterfaceInfo;
}
#endif

namespace content {

// Initialize the sandbox for renderer, gpu, utility, worker, nacl, and plug-in
// processes, depending on the command line flags. Although The browser process
// is not sandboxed, this also needs to be called because it will initialize
// the broker code.
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing true is always
// returned.
#if defined(OS_WIN)
CONTENT_EXPORT bool InitializeSandbox(
    sandbox::SandboxInterfaceInfo* sandbox_info);
#elif defined(OS_MACOSX)
CONTENT_EXPORT bool InitializeSandbox();
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
