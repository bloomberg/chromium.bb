// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/command_buffer/service/async_pixel_transfer_manager.h"

#include "base/debug/trace_event.h"
#include "base/sys_info.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_egl.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_idle.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_stub.h"
#include "gpu/command_buffer/service/async_pixel_transfer_manager_sync.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"

namespace gpu {
namespace {

enum GpuType {
  GPU_BROADCOM,
  GPU_IMAGINATION,
  GPU_NVIDIA_ES31,
  GPU_ADRENO_420,
  GPU_OTHER,
};

std::string MakeString(const char* s) {
  return std::string(s ? s : "");
}

GpuType GetGpuType() {
  const std::string vendor = MakeString(
      reinterpret_cast<const char*>(glGetString(GL_VENDOR)));
  const std::string renderer = MakeString(
      reinterpret_cast<const char*>(glGetString(GL_RENDERER)));
  const std::string version = MakeString(
      reinterpret_cast<const char*>(glGetString(GL_VERSION)));

  if (vendor.find("Broadcom") != std::string::npos)
    return GPU_BROADCOM;

  if (vendor.find("Imagination") != std::string::npos)
    return GPU_IMAGINATION;

  if (vendor.find("NVIDIA") != std::string::npos &&
      version.find("OpenGL ES 3.1") != std::string::npos) {
    return GPU_NVIDIA_ES31;
  }

  if (vendor.find("Qualcomm") != std::string::npos &&
      renderer.find("Adreno (TM) 420") != std::string::npos) {
    return GPU_ADRENO_420;
  }

  return GPU_OTHER;
}

bool AllowTransferThreadForGpu() {
  GpuType gpu = GetGpuType();
  return gpu != GPU_BROADCOM && gpu != GPU_IMAGINATION &&
         gpu != GPU_NVIDIA_ES31 && gpu != GPU_ADRENO_420;
}

}

// We only used threaded uploads when we can:
// - Create EGLImages out of OpenGL textures (EGL_KHR_gl_texture_2D_image)
// - Bind EGLImages to OpenGL textures (GL_OES_EGL_image)
// - Use fences (to test for upload completion).
// - The heap size is large enough.
// TODO(kaanb|epenner): Remove the IsImagination() check pending the
// resolution of crbug.com/249147
// TODO(kaanb|epenner): Remove the IsLowEndDevice() check pending the
// resolution of crbug.com/271929
AsyncPixelTransferManager* AsyncPixelTransferManager::Create(
    gfx::GLContext* context) {
  DCHECK(context->IsCurrent(NULL));
  switch (gfx::GetGLImplementation()) {
    case gfx::kGLImplementationEGLGLES2:
      DCHECK(context);
      if (!base::SysInfo::IsLowEndDevice() &&
          context->HasExtension("EGL_KHR_fence_sync") &&
          context->HasExtension("EGL_KHR_image") &&
          context->HasExtension("EGL_KHR_image_base") &&
          context->HasExtension("EGL_KHR_gl_texture_2D_image") &&
          context->HasExtension("GL_OES_EGL_image") &&
          AllowTransferThreadForGpu()) {
        TRACE_EVENT0("gpu", "AsyncPixelTransferManager_CreateWithThread");
        return new AsyncPixelTransferManagerEGL;
      }
      return new AsyncPixelTransferManagerIdle;
    case gfx::kGLImplementationOSMesaGL: {
      TRACE_EVENT0("gpu", "AsyncPixelTransferManager_CreateIdle");
      return new AsyncPixelTransferManagerIdle;
    }
    case gfx::kGLImplementationMockGL:
      return new AsyncPixelTransferManagerStub;
    default:
      NOTREACHED();
      return NULL;
  }
}

}  // namespace gpu
