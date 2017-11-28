// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/gpu_switches.h"

#include "base/macros.h"

namespace gpu {

const char kCmdDecoderValidatingName[] = "validating";
const char kCmdDecoderPassthroughName[] = "passthrough";

}  // namespace gpu

namespace switches {

// Always return success when compiling a shader. Linking will still fail.
const char kCompileShaderAlwaysSucceeds[]   = "compile-shader-always-succeeds";

// Disable the GL error log limit.
const char kDisableGLErrorLimit[]           = "disable-gl-error-limit";

// Disable the GLSL translator.
const char kDisableGLSLTranslator[]         = "disable-glsl-translator";

// Turn off user-defined name hashing in shaders.
const char kDisableShaderNameHashing[]      = "disable-shader-name-hashing";

// Turn on Logging GPU commands.
const char kEnableGPUCommandLogging[]       = "enable-gpu-command-logging";

// Turn on Calling GL Error after every command.
const char kEnableGPUDebugging[]            = "enable-gpu-debugging";

// Enable GPU service logging. Note: This is the same switch as the one in
// gl_switches.cc. It's defined here again to avoid dependencies between
// dlls.
const char kEnableGPUServiceLoggingGPU[]    = "enable-gpu-service-logging";

// Enable logging of GPU driver debug messages.
const char kEnableGPUDriverDebugLogging[] = "enable-gpu-driver-debug-logging";

// Turn off gpu program caching
const char kDisableGpuProgramCache[]        = "disable-gpu-program-cache";

// Enforce GL minimums.
const char kEnforceGLMinimums[]             = "enforce-gl-minimums";

// Sets the total amount of memory that may be allocated for GPU resources
const char kForceGpuMemAvailableMb[]        = "force-gpu-mem-available-mb";

// Sets the maximum size of the in-memory gpu program cache, in kb
const char kGpuProgramCacheSizeKb[]         = "gpu-program-cache-size-kb";

// Disables the GPU shader on disk cache.
const char kDisableGpuShaderDiskCache[]     = "disable-gpu-shader-disk-cache";

// Simulates shared textures when share groups are not available. Not available
// everywhere.
const char kEnableThreadedTextureMailboxes[] =
    "enable-threaded-texture-mailboxes";

// Include ANGLE's intermediate representation (AST) output in shader
// compilation info logs.
const char kGLShaderIntermOutput[] = "gl-shader-interm-output";

// Emulate ESSL lowp and mediump float precisions by mutating the shaders to
// round intermediate values in ANGLE.
const char kEmulateShaderPrecision[] = "emulate-shader-precision";

// Use the Pass-through command decoder, skipping all validation and state
// tracking.
const char kUseCmdDecoder[] = "use-cmd-decoder";

// Use a dedicated RasterInterfaceDecoder instead of generic GLES2Decoder.
const char kEnableRasterDecoder[] = "enable-raster-decoder";

}  // namespace switches
