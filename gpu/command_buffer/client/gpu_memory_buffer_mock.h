// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MOCK_H_
#define GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MOCK_H_

#include "gpu/command_buffer/client/gpu_memory_buffer.h"
#include "base/basictypes.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class GpuMemoryBufferMock : public GpuMemoryBuffer {
 public:
  GpuMemoryBufferMock(int width, int height);
  virtual ~GpuMemoryBufferMock();

  MOCK_METHOD2(Map, void(GpuMemoryBuffer::AccessMode, void**));
  MOCK_METHOD0(Unmap, void());
  MOCK_METHOD0(IsMapped, bool());
  MOCK_METHOD0(GetNativeBuffer, void*());
  MOCK_METHOD0(GetStride, uint32());
  MOCK_METHOD0(Die, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(GpuMemoryBufferMock);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_CLIENT_GPU_MEMORY_BUFFER_MOCK_H_
