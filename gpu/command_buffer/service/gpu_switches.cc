// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_switches.h"
#include "base/basictypes.h"

namespace switches {

// Turn on Calling GL Error after every command.
const char kCompileShaderAlwaysSucceeds[]   = "compile-shader-always-succeeds";

// Disable the GL error log limit.
const char kDisableGLErrorLimit[]           = "disable-gl-error-limit";

// Disable the GLSL translator.
const char kDisableGLSLTranslator[]         = "disable-glsl-translator";

// Disable workarounds for various GPU driver bugs.
const char kDisableGpuDriverBugWorkarounds[] =
    "disable-gpu-driver-bug-workarounds";

// Turn on Logging GPU commands.
const char kEnableGPUCommandLogging[]       = "enable-gpu-command-logging";

// Turn on Calling GL Error after every command.
const char kEnableGPUDebugging[]            = "enable-gpu-debugging";

// Enforce GL minimums.
const char kEnforceGLMinimums[]             = "enforce-gl-minimums";

// Force the use of a workaround for graphics hangs seen on certain
// Mac OS systems. Enabled by default (and can't be disabled) on known
// affected systems.
const char kForceGLFinishWorkaround[]       = "force-glfinish-workaround";

const char* kGpuSwitches[] = {
  kCompileShaderAlwaysSucceeds,
  kDisableGLErrorLimit,
  kDisableGLSLTranslator,
  kDisableGpuDriverBugWorkarounds,
  kEnableGPUCommandLogging,
  kEnableGPUDebugging,
  kEnforceGLMinimums,
  kForceGLFinishWorkaround,
};

const int kNumGpuSwitches = arraysize(kGpuSwitches);

}  // namespace switches
