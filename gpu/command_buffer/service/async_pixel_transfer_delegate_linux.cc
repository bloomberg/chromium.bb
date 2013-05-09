// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"

#include "base/debug/trace_event.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate_idle.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate_stub.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {

AsyncPixelTransferDelegate* AsyncPixelTransferDelegate::Create(
    gfx::GLContext* context) {
  TRACE_EVENT0("gpu", "AsyncPixelTransferDelegate::Create");
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationOSMesaGL:
    case gfx::kGLImplementationDesktopGL:
    case gfx::kGLImplementationEGLGLES2:
      return new AsyncPixelTransferDelegateIdle;
    case gfx::kGLImplementationMockGL:
      return new AsyncPixelTransferDelegateStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gpu
