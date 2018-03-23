// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "command_buffer_stub_wrapper_impl.h"

#include "gpu/ipc/service/command_buffer_stub.h"

namespace media {

CommandBufferStubWrapperImpl::CommandBufferStubWrapperImpl(
    gpu::CommandBufferStub* stub)
    : stub_(stub) {}

bool CommandBufferStubWrapperImpl::MakeCurrent() {
  // Support |!stub_| as a convenience.
  return stub_ && stub_->decoder_context()->MakeCurrent();
}

void CommandBufferStubWrapperImpl::AddDestructionObserver(
    gpu::CommandBufferStub::DestructionObserver* observer) {
  stub_->AddDestructionObserver(observer);
}

void CommandBufferStubWrapperImpl::RemoveDestructionObserver(
    gpu::CommandBufferStub::DestructionObserver* observer) {
  stub_->RemoveDestructionObserver(observer);
}

}  // namespace media
