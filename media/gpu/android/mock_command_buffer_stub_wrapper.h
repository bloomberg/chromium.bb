// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_MOCK_COMMAND_BUFFER_STUB_WRAPPER_H_
#define MEDIA_GPU_ANDROID_MOCK_COMMAND_BUFFER_STUB_WRAPPER_H_

#include "media/gpu/android/command_buffer_stub_wrapper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class MockCommandBufferStubWrapper
    : public ::testing::NiceMock<CommandBufferStubWrapper> {
 public:
  MockCommandBufferStubWrapper();
  virtual ~MockCommandBufferStubWrapper();

  // CommandBufferStubWrapper
  MOCK_METHOD0(MakeCurrent, bool());

  void AddDestructionObserver(
      gpu::CommandBufferStub::DestructionObserver* observer);
  void RemoveDestructionObserver(
      gpu::CommandBufferStub::DestructionObserver* observer);

  // Notify the observer that we will be destroyed.
  void NotifyDestruction();

 private:
  gpu::CommandBufferStub::DestructionObserver* observer_ = nullptr;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MOCK_COMMAND_BUFFER_STUB_WRAPPER_H_
