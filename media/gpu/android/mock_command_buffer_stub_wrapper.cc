// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/android/mock_command_buffer_stub_wrapper.h"

namespace media {

MockCommandBufferStubWrapper::MockCommandBufferStubWrapper() = default;
MockCommandBufferStubWrapper::~MockCommandBufferStubWrapper() = default;

void MockCommandBufferStubWrapper::AddDestructionObserver(
    gpu::CommandBufferStub::DestructionObserver* observer) {
  ASSERT_FALSE(observer_);
  ASSERT_TRUE(observer);
  observer_ = observer;
}

void MockCommandBufferStubWrapper::RemoveDestructionObserver(
    gpu::CommandBufferStub::DestructionObserver* observer) {
  ASSERT_EQ(observer_, observer);
  observer_ = nullptr;
}

void MockCommandBufferStubWrapper::NotifyDestruction() {
  if (observer_)
    observer_->OnWillDestroyStub();
}

}  // namespace media
