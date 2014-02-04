// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/gles2/gles2_client_impl.h"

#include "mojo/public/bindings/sync_dispatcher.h"
#include "mojo/public/gles2/gles2.h"
#include "mojo/public/system/core_cpp.h"

namespace mojo {
namespace gles2 {

GLES2ClientImpl::GLES2ClientImpl(MojoAsyncWaiter* async_waiter,
                                 ScopedMessagePipeHandle pipe,
                                 MojoGLES2ContextLost lost_callback,
                                 MojoGLES2DrawAnimationFrame animation_callback,
                                 void* closure)
    : service_(pipe.Pass(), this, NULL, async_waiter),
      implementation_(NULL),
      lost_callback_(lost_callback),
      animation_callback_(animation_callback),
      closure_(closure) {
}

GLES2ClientImpl::~GLES2ClientImpl() {
  service_->Destroy();
}

bool GLES2ClientImpl::Initialize() {
  MessagePipe sync_pipe;
  sync_dispatcher_.reset(
      new SyncDispatcher<GLES2SyncClient>(sync_pipe.handle1.Pass(), this));
  service_->Initialize(sync_pipe.handle0.Pass());
  // Wait for DidCreateContext to come on the sync client pipe.
  if (!sync_dispatcher_->WaitAndDispatchOneMessage())
    return false;
  return !!implementation_;
}

void GLES2ClientImpl::RequestAnimationFrames() {
  service_->RequestAnimationFrames();
}

void GLES2ClientImpl::CancelAnimationFrames() {
  service_->CancelAnimationFrames();
}

void GLES2ClientImpl::DidCreateContext(uint64_t encoded) {
  // Ack, Hans! It's the giant hack.
  // TODO(abarth): Replace this hack with something more disciplined. Most
  // likley, we should receive a MojoHandle that we use to wire up the
  // client side of an out-of-process command buffer. Given that we're
  // still in-process, we just reinterpret_cast the value into a GL interface.
  implementation_ = reinterpret_cast<gpu::gles2::GLES2Implementation*>(
      static_cast<uintptr_t>(encoded));
}

void GLES2ClientImpl::ContextLost() {
  lost_callback_(closure_);
}

void GLES2ClientImpl::DrawAnimationFrame() {
  animation_callback_(closure_);
}

}  // namespace gles2
}  // namespace mojo
