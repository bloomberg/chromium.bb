// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_COMMAND_BUFFER_STUB_WRAPPER_IMPL_H_
#define MEDIA_GPU_ANDROID_COMMAND_BUFFER_STUB_WRAPPER_IMPL_H_

#include "media/gpu/android/command_buffer_stub_wrapper.h"

namespace media {

// Implementation that actually talks to a CommandBufferStub
class CommandBufferStubWrapperImpl : public CommandBufferStubWrapper {
 public:
  explicit CommandBufferStubWrapperImpl(gpu::CommandBufferStub* stub);
  ~CommandBufferStubWrapperImpl() override = default;

  // CommandBufferStubWrapper
  bool MakeCurrent() override;
  void AddDestructionObserver(
      gpu::CommandBufferStub::DestructionObserver* observer) override;
  void RemoveDestructionObserver(
      gpu::CommandBufferStub::DestructionObserver* observer) override;

 private:
  gpu::CommandBufferStub* stub_;
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_MEDIA_COMMAND_BUFFER_STUB_WRAPPER_IMPL_H_
