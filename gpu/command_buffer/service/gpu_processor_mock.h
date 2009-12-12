// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_MOCK_H_
#define GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_MOCK_H_

#include "gpu/command_buffer/service/gpu_processor.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace gpu {

class MockGPUProcessor : public GPUProcessor {
 public:
  explicit MockGPUProcessor(CommandBuffer* command_buffer)
      : GPUProcessor(command_buffer) {
  }

  MOCK_METHOD1(Initialize, bool(gfx::PluginWindowHandle handle));
  MOCK_METHOD0(Destroy, void());
  MOCK_METHOD0(ProcessCommands, void());
  MOCK_METHOD3(SetWindow, bool(gfx::PluginWindowHandle handle,
                               int width,
                               int height));
  MOCK_METHOD1(GetSharedMemoryAddress, void*(int32 shm_id));
  MOCK_METHOD1(GetSharedMemorySize, size_t(int32 shm_id));
  MOCK_METHOD1(set_token, void(int32 token));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockGPUProcessor);
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_SERVICE_GPU_PROCESSOR_MOCK_H_
