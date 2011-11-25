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
#elif defined(OS_MACOSX)
class FilePath;
#endif

namespace content {

#if defined(OS_WIN)
// Initialize the sandbox for renderer, gpu, utility, worker, nacl, and plug-in
// processes, depending on the command line flags. Although The browser process
// is not sandboxed, this also needs to be called because it will initialize
// the broker code.
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing true is always
// returned.
CONTENT_EXPORT bool InitializeSandbox(
    sandbox::SandboxInterfaceInfo* sandbox_info);
#elif defined(OS_MACOSX)
// Initialize the sandbox of the given |sandbox_type|, optionally specifying a
// directory to allow access to. Note specifying a directory needs to be
// supported by the sandbox profile associated with the given |sandbox_type|.
// Valid values for |sandbox_type| are defined either by the enum SandboxType,
// or by ContentClient::GetSandboxProfileForSandboxType().
//
// If the |sandbox_type| isn't one of the ones defined by content then the
// embedder is queried using ContentClient::GetSandboxPolicyForSandboxType().
// The embedder can use values for |sandbox_type| starting from
// content::sandbox::SANDBOX_PROCESS_TYPE_AFTER_LAST_TYPE.
//
// Returns true if the sandbox was initialized succesfully, false if an error
// occurred.  If process_type isn't one that needs sandboxing, no action is
// taken and true is always returned.
CONTENT_EXPORT bool InitializeSandbox(int sandbox_type,
                                      const FilePath& allowed_path);
#endif

}  // namespace content

#endif  // CONTENT_PUBLIC_COMMON_SANDBOX_INIT_H_
