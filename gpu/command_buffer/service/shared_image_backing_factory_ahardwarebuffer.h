// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_AHARDWAREBUFFER_H_
#define GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_AHARDWAREBUFFER_H_

#include "base/macros.h"
#include "components/viz/common/resources/resource_format.h"
#include "gpu/command_buffer/service/shared_image_backing_factory.h"
#include "gpu/gpu_gles2_export.h"

namespace gfx {
class Size;
class ColorSpace;
}  // namespace gfx

namespace gpu {
class SharedImageBacking;
class GpuDriverBugWorkarounds;
struct Mailbox;

// Implementation of SharedImageBackingFactory that produces AHardwareBuffer
// backed SharedImages. This is meant to be used on Android only.
class GPU_GLES2_EXPORT SharedImageBackingFactoryAHardwareBuffer
    : public SharedImageBackingFactory {
 public:
  SharedImageBackingFactoryAHardwareBuffer(
      const GpuDriverBugWorkarounds& workarounds);
  ~SharedImageBackingFactoryAHardwareBuffer() override;

  // SharedImageBackingFactory implementation.
  std::unique_ptr<SharedImageBacking> CreateSharedImage(
      const Mailbox& mailbox,
      viz::ResourceFormat format,
      const gfx::Size& size,
      const gfx::ColorSpace& color_space,
      uint32_t usage) override;

 private:
  // Used to limit the max size of AHardwareBuffer.
  int32_t max_gl_texture_size_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SharedImageBackingFactoryAHardwareBuffer);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_SHARED_IMAGE_BACKING_FACTORY_AHARDWAREBUFFER_H_
