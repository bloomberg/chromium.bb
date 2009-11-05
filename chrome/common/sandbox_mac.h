// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SANDBOX_MAC_H_
#define CHROME_COMMON_SANDBOX_MAC_H_

namespace sandbox {

// Warm up System APIs that empirically need to be accessed before the Sandbox
// is turned on.
void SandboxWarmup();

// Turns on the OS X sandbox for this process.
bool EnableSandbox();

}  // namespace sandbox

#endif  // CHROME_COMMON_SANDBOX_MAC_H_
