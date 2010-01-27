// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/thread_local.h"

namespace gles2 {
namespace {
gpu::ThreadLocalKey g_gl_context_key;
}  // namespace anonymous

void Initialize() {
  g_gl_context_key = gpu::ThreadLocalAlloc();
}

void Terminate() {
  gpu::ThreadLocalFree(g_gl_context_key);
}

gpu::gles2::GLES2Implementation* GetGLContext() {
  return static_cast<gpu::gles2::GLES2Implementation*>(
    gpu::ThreadLocalGetValue(g_gl_context_key));
}

void SetGLContext(gpu::gles2::GLES2Implementation* context) {
  gpu::ThreadLocalSetValue(g_gl_context_key, context);
}
}  // namespace gles2




