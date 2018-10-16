// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_
#define GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_

#include "gpu/gpu_gles2_export.h"

// Forwardly declare a few GL types to avoid including GL header files.
typedef int GLint;

class GrBackendTexture;

namespace gl {
struct GLVersionInfo;
}  // namespace gl

namespace gpu {
class TextureBase;

namespace gles2 {
class ErrorState;
}  // namespace gles2

// Wraps a gpu::Texture into Skia API as a GrBackendTexture. Skia does not take
// ownership.  Returns true on success. GL errors may be reported in
// |error_state| on failure.
GPU_GLES2_EXPORT bool GetGrBackendTexture(const gl::GLVersionInfo& version_info,
                                          const char* function_name,
                                          gles2::ErrorState* error_state,
                                          const TextureBase& texture,
                                          GLint sk_color_type,
                                          GrBackendTexture* gr_texture);

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SKIA_UTILS_H_
