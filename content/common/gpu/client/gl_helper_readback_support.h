// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_GPU_CLIENT_GL_HELPER_READBACK_SUPPORT_H_
#define CONTENT_COMMON_GPU_CLIENT_GL_HELPER_READBACK_SUPPORT_H_

#include "content/common/gpu/client/gl_helper.h"

namespace content {

class CONTENT_EXPORT GLHelperReadbackSupport {
 public:
  GLHelperReadbackSupport(gpu::gles2::GLES2Interface* gl);

  ~GLHelperReadbackSupport();

  // Checks whether the readback (Color read format and type) is supported
  // for the requested texture format  by the hardware or not.
  // For ex: some hardwares have rgb565 readback
  // support when binded with the frame buffer for others it may fail.
  // Here we pass the internal textureformat as skia config.
  bool IsReadbackConfigSupported(SkBitmap::Config texture_format);

 private:
  enum FormatSupport { FORMAT_NOT_SUPPORTED = 0, FORMAT_SUPPORTED, };

  // This populates the format_support_table with the list of supported
  // formats.
  void InitializeReadbackSupport();

  // This api is called  once per format and it is done in the
  // InitializeReadbackSupport. We should not use this any where
  // except the InitializeReadbackSupport.Calling this at other places
  // can distrub the state of normal gl operations.
  void CheckForReadbackSupport(SkBitmap::Config texture_format);

  // Helper functions for checking the supported texture formats.
  // Avoid using this API in between texture operations, as this does some
  // teture opertions (bind, attach) internally.
  bool SupportsFormat(GLint format, GLint type);

  FormatSupport format_support_table_[SkBitmap::kConfigCount];

  gpu::gles2::GLES2Interface* gl_;

};

}  // namespace content

#endif  // CONTENT_COMMON_GPU_CLIENT_GL_HELPER_READBACK_SUPPORT_H_
