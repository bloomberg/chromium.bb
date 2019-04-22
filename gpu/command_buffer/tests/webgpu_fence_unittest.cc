// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <dawn/dawncpp.h>

#include "gpu/command_buffer/client/webgpu_implementation.h"
#include "gpu/command_buffer/tests/webgpu_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockFenceOnCompletionCallback {
 public:
  MOCK_METHOD2(Call,
               void(DawnFenceCompletionStatus status,
                    DawnCallbackUserdata userdata));
};

std::unique_ptr<MockFenceOnCompletionCallback> mockFenceOnCompletionCallback;
void ToMockFenceOnCompletionCallback(DawnFenceCompletionStatus status,
                                     DawnCallbackUserdata userdata) {
  mockFenceOnCompletionCallback->Call(status, userdata);
}

}  // namespace

namespace gpu {

class WebGPUFenceTest : public WebGPUTest {
 protected:
  void SetUp() override {
    WebGPUTest::SetUp();
    Initialize(WebGPUTest::Options());
    mockFenceOnCompletionCallback =
        std::make_unique<MockFenceOnCompletionCallback>();
  }

  void TearDown() override {
    mockFenceOnCompletionCallback = nullptr;
    WebGPUTest::TearDown();
  }

  void WaitForFence(dawn::Device device, dawn::Fence fence, uint64_t value) {
    while (fence.GetCompletedValue() < value) {
      device.Tick();
      webgpu()->FlushCommands();
      RunPendingTasks();
    }
  }
};

// Test that getting the value of the fence is the initial value.
TEST_F(WebGPUFenceTest, InitialValue) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped";
    return;
  }
  dawn::Device device = dawn::Device::Acquire(webgpu()->GetDefaultDevice());
  dawn::Queue queue = device.CreateQueue();
  {
    dawn::FenceDescriptor fence_desc{nullptr, 0};
    dawn::Fence fence = queue.CreateFence(&fence_desc);
    EXPECT_EQ(fence.GetCompletedValue(), 0u);
  }
  {
    dawn::FenceDescriptor fence_desc{nullptr, 2};
    dawn::Fence fence = queue.CreateFence(&fence_desc);
    EXPECT_EQ(fence.GetCompletedValue(), 2u);
  }
}

// Test that after signaling a fence, its completed value gets updated.
TEST_F(WebGPUFenceTest, GetCompletedValue) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped";
    return;
  }
  dawn::Device device = dawn::Device::Acquire(webgpu()->GetDefaultDevice());
  dawn::Queue queue = device.CreateQueue();
  dawn::FenceDescriptor fence_desc{nullptr, 0};
  dawn::Fence fence = queue.CreateFence(&fence_desc);
  queue.Signal(fence, 2u);
  WaitForFence(device, fence, 2u);
  EXPECT_EQ(fence.GetCompletedValue(), 2u);
}

// Test that a fence's OnCompletion handler is called after the signal value
// is completed.
TEST_F(WebGPUFenceTest, OnCompletion) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped";
    return;
  }
  dawn::Device device = dawn::Device::Acquire(webgpu()->GetDefaultDevice());
  dawn::Queue queue = device.CreateQueue();
  dawn::FenceDescriptor fence_desc{nullptr, 0};
  dawn::Fence fence = queue.CreateFence(&fence_desc);
  queue.Signal(fence, 2u);

  DawnCallbackUserdata userdata = 9847;
  EXPECT_CALL(*mockFenceOnCompletionCallback,
              Call(DAWN_FENCE_COMPLETION_STATUS_SUCCESS, userdata))
      .Times(1);
  fence.OnCompletion(2u, ToMockFenceOnCompletionCallback, userdata);
  WaitForFence(device, fence, 2u);
}

// Test signaling a fence a million times.
TEST_F(WebGPUFenceTest, SignalManyTimes) {
  if (!WebGPUSupported()) {
    LOG(ERROR) << "Test skipped";
    return;
  }
  dawn::Device device = dawn::Device::Acquire(webgpu()->GetDefaultDevice());
  dawn::Queue queue = device.CreateQueue();
  dawn::FenceDescriptor fence_desc{nullptr, 0};
  dawn::Fence fence = queue.CreateFence(&fence_desc);

  uint64_t max_value = 1000000u;
  for (uint64_t i = 1; i <= max_value; ++i) {
    queue.Signal(fence, i);
  }
  WaitForFence(device, fence, max_value);
  EXPECT_EQ(fence.GetCompletedValue(), max_value);
}

}  // namespace gpu
