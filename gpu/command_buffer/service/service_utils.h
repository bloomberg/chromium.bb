// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_

#include "gpu/gpu_export.h"
#include "ui/gl/gl_context.h"

namespace gpu {

namespace gles2 {
struct ContextCreationAttribHelper;
class ContextGroup;

GPU_EXPORT gl::GLContextAttribs GenerateGLContextAttribs(
    const ContextCreationAttribHelper& attribs_helper,
    const ContextGroup* context_group);

// Returns true if the driver supports creating passthrough command decoders
GPU_EXPORT bool PassthroughCommandDecoderSupported();

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SERVICE_UTILS_H_
