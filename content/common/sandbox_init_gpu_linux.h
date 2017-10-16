// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_SANDBOX_INIT_GPU_LINUX_H_
#define CONTENT_COMMON_SANDBOX_INIT_GPU_LINUX_H_

namespace content {

class GpuProcessPolicy;

// Provide calllbacks into GPU code for the sandbox to use to warm up the
// libraries and files required by GPU drivers prior to engaging the sandbox.
bool GpuPreSandboxHook(GpuProcessPolicy* policy);
bool CrosArmGpuPreSandboxHook(GpuProcessPolicy* policy);
bool CrosAmdGpuPreSandboxHook(GpuProcessPolicy* policy);

}  // namespace content

#endif  // CONTENT_COMMON_SANDBOX_INIT_GPU_LINUX_H_
