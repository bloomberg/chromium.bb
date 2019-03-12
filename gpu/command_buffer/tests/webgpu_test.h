// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
#define GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_

#include <memory>

#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gpu {

class InProcessGpuThreadHolder;
class WebGPUInProcessContext;

namespace webgpu {

class WebGPUInterface;

}  // namespace webgpu

class WebGPUTest : public testing::Test {
 protected:
  struct Options {
    Options();

    // Shared memory limits
    SharedMemoryLimits shared_memory_limits = {};
  };

  WebGPUTest();
  ~WebGPUTest() override;

  void SetUp() override;
  void TearDown() override;

  void Initialize(const Options& options);

  webgpu::WebGPUInterface* webgpu() const;
  void RunPendingTasks();

 private:
  std::unique_ptr<InProcessGpuThreadHolder> gpu_thread_holder_;
  std::unique_ptr<WebGPUInProcessContext> context_;
};

}  // namespace gpu

#endif  // GPU_COMMAND_BUFFER_TESTS_WEBGPU_TEST_H_
