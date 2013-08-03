// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_IMAGE_FACTORY_H_
#define GPU_COMMAND_BUFFER_CLIENT_IMAGE_FACTORY_H_

#include <GLES2/gl2.h>

#include "base/memory/scoped_ptr.h"
#include "gles2_impl_export.h"

namespace gfx {
class GpuMemoryBuffer;
}

namespace gpu {
namespace gles2 {

class GLES2_IMPL_EXPORT ImageFactory {

 public:
  virtual ~ImageFactory() {}

  // Create a GpuMemoryBuffer and makes it available to the
  // service side by inserting it to the ImageManager.
  virtual scoped_ptr<gfx::GpuMemoryBuffer> CreateGpuMemoryBuffer(
      int width, int height, GLenum internalformat, unsigned* image_id) = 0;
  virtual void DeleteGpuMemoryBuffer(unsigned image_id) = 0;
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_IMAGE_FACTORY_H_
