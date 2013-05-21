// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_delegate.h"

#include "base/command_line.h"
#include "base/debug/trace_event.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate_idle.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate_share_group.h"
#include "gpu/command_buffer/service/async_pixel_transfer_delegate_stub.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {

AsyncPixelTransferDelegate* AsyncPixelTransferDelegate::Create(
    gfx::GLContext* context) {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableShareGroupAsyncTextureUpload)) {
    DCHECK(context);
    return static_cast<AsyncPixelTransferDelegate*> (
        new AsyncPixelTransferDelegateShareGroup(context));
  }

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
