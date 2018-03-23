// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_ANDROID_COMMAND_BUFFER_STUB_WRAPPER_H_
#define MEDIA_GPU_ANDROID_COMMAND_BUFFER_STUB_WRAPPER_H_

#include "gpu/ipc/service/command_buffer_stub.h"

namespace media {

// Helpful class to wrap a CommandBufferStub that we can mock out more easily.
// Mocking out a CommandBufferStub + DecoderContext is quite annoying, since we
// really need very little.
// TODO(liberato): consider making this refcounted, so that one injected mock
// can be re-used as its passed from class to class.  In that case, it likely
// has to keep its own DestructionObserver list, and register itself as a
// DestructionObserver on the stub.
// TODO(liberato): once this interface is stable, move this to media/gpu and
// use it on non-android platforms.
class CommandBufferStubWrapper {
 public:
  virtual ~CommandBufferStubWrapper() = default;

  // Make the stub's context current.  Return true on success.
  virtual bool MakeCurrent() = 0;

  // Add or remove a destruction observer on the underlying stub.
  virtual void AddDestructionObserver(
      gpu::CommandBufferStub::DestructionObserver* observer) = 0;
  virtual void RemoveDestructionObserver(
      gpu::CommandBufferStub::DestructionObserver* observer) = 0;

  // To support VideoFrameFactoryImpl, we need at least GetTextureManager().
};

}  // namespace media

#endif  // MEDIA_GPU_ANDROID_COMMAND_BUFFER_STUB_WRAPPER_H_
