// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_APP_SANDBOX_HELPER_WIN_H_
#define CONTENT_APP_SANDBOX_HELPER_WIN_H_
#pragma once

namespace sandbox {
struct SandboxInterfaceInfo;
}

namespace content {

// Initializes the sandbox code and turns on DEP.
void InitializeSandboxInfo(sandbox::SandboxInterfaceInfo* sandbox_info);

}  // namespace content

#endif  // CONTENT_APP_SANDBOX_HELPER_WIN_H_
