// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_IMAGE_FACTORY_MOCK_H_
#define GPU_COMMAND_BUFFER_CLIENT_IMAGE_FACTORY_MOCK_H_

#include "base/memory/scoped_ptr.h"
#include "gpu/command_buffer/client/gpu_memory_buffer.h"
#include "gpu/command_buffer/client/image_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {
namespace gles2 {
class ImageManager;

// Mock implementation of ImageFactory
class ImageFactoryMock : public ImageFactory {
 public:
  ImageFactoryMock(ImageManager* image_manager);
  virtual ~ImageFactoryMock();

  MOCK_METHOD4(CreateGpuMemoryBufferMock, GpuMemoryBuffer*(
      int width, int height, GLenum internalformat, unsigned* image_id));
  MOCK_METHOD1(DeleteGpuMemoryBuffer, void(unsigned));
  // Workaround for mocking methods that return scoped_ptrs
  virtual scoped_ptr<GpuMemoryBuffer> CreateGpuMemoryBuffer(
      int width, int height, GLenum internalformat,
      unsigned* image_id) OVERRIDE {
    return scoped_ptr<GpuMemoryBuffer>(CreateGpuMemoryBufferMock(
        width, height, internalformat, image_id));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ImageFactoryMock);
};

}  // namespace gles2
}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_IMAGE_FACTORY_MOCK_H_
