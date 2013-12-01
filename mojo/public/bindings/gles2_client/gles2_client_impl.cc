// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/bindings/gles2_client/gles2_client_impl.h"

#include <assert.h>

#include "gpu/command_buffer/client/gles2_lib.h"

namespace mojo {
namespace {

bool g_gles2_initialized = false;

}  // namespace

GLES2Delegate::~GLES2Delegate() {
}

void GLES2Delegate::DidCreateContext(
    GLES2* gl, uint32_t width, uint32_t height) {
}

void GLES2Delegate::ContextLost(GLES2* gl) {
}

GLES2ClientImpl::GLES2ClientImpl(GLES2Delegate* delegate,
                                 ScopedMessagePipeHandle gl)
    : delegate_(delegate),
      gl_(gl.Pass()) {
  assert(g_gles2_initialized);
  gl_.SetPeer(this);
}

GLES2ClientImpl::~GLES2ClientImpl() {
}

void GLES2ClientImpl::Initialize() {
  assert(!g_gles2_initialized);
  gles2::Initialize();
  g_gles2_initialized = true;
}

void GLES2ClientImpl::Terminate() {
  assert(g_gles2_initialized);
  gles2::Terminate();
  g_gles2_initialized = false;
}

void GLES2ClientImpl::DidCreateContext(
    uint64_t encoded, uint32_t width, uint32_t height) {
  // Ack, Hans! It's the giant hack.
  // TODO(abarth): Replace this hack with something more disciplined. Most
  // likley, we should receive a MojoHandle that we use to wire up the
  // client side of an out-of-process command buffer. Given that we're
  // still in-process, we just reinterpret_cast the value into a GL interface.
  gpu::gles2::GLES2Interface* gl_interface =
      reinterpret_cast<gpu::gles2::GLES2Interface*>(
          static_cast<uintptr_t>(encoded));
  gles2::SetGLContext(gl_interface);

  delegate_->DidCreateContext(gl(), width, height);
}

void GLES2ClientImpl::ContextLost() {
  delegate_->ContextLost(gl());
}

}  // mojo
