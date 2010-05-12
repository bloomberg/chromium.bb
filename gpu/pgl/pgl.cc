// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/pgl/pgl.h"

#include "build/build_config.h"
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/command_buffer/common/thread_local.h"
#include "gpu/pgl/command_buffer_pepper.h"

namespace {
const int32 kTransferBufferSize = 512 * 1024;

class PGLContextImpl {
 public:
  PGLContextImpl(NPP npp,
                 NPDevice* device,
                 NPDeviceContext3D* device_context);
  ~PGLContextImpl();

  // Initlaize a PGL context with a transfer buffer of a particular size.
  PGLBoolean Initialize(int32 transfer_buffer_size);

  // Destroy all resources associated with the PGL context.
  void Destroy();

  // Make a PGL context current for the calling thread.
  static PGLBoolean MakeCurrent(PGLContextImpl* pgl_context);

  // Display all content rendered since last call to SwapBuffers.
  PGLBoolean SwapBuffers();

  // Get the current error code.
  PGLInt GetError();

 private:
  PGLContextImpl(const PGLContextImpl&);
  void operator=(const PGLContextImpl&);

  NPP npp_;
  NPDevice* device_;
  NPDeviceContext3D* device_context_;
  CommandBufferPepper* command_buffer_;
  gpu::gles2::GLES2CmdHelper* gles2_helper_;
  int32 transfer_buffer_id_;
  gpu::gles2::GLES2Implementation* gles2_implementation_;
};

gpu::ThreadLocalKey g_pgl_context_key;
bool g_pgl_context_key_allocated = false;

PGLContextImpl::PGLContextImpl(NPP npp,
                               NPDevice* device,
                               NPDeviceContext3D* device_context)
    : npp_(npp),
      device_(device),
      device_context_(device_context),
      command_buffer_(NULL),
      gles2_helper_(NULL),
      transfer_buffer_id_(0),
      gles2_implementation_(NULL) {
}

PGLContextImpl::~PGLContextImpl() {
  Destroy();
}

PGLBoolean PGLContextImpl::Initialize(int32 transfer_buffer_size) {
  // Create and initialize the objects required to issue GLES2 calls.
  command_buffer_ = new CommandBufferPepper(
      npp_, device_, device_context_);
  gles2_helper_ = new gpu::gles2::GLES2CmdHelper(command_buffer_);
  gpu::Buffer buffer = command_buffer_->GetRingBuffer();
  if (gles2_helper_->Initialize(buffer.size)) {
    transfer_buffer_id_ =
        command_buffer_->CreateTransferBuffer(kTransferBufferSize);
    gpu::Buffer transfer_buffer =
        command_buffer_->GetTransferBuffer(transfer_buffer_id_);
    if (transfer_buffer.ptr) {
      gles2_implementation_ = new gpu::gles2::GLES2Implementation(
          gles2_helper_,
          transfer_buffer.size,
          transfer_buffer.ptr,
          transfer_buffer_id_,
          false);
      return PGL_TRUE;
    }
  }

  // Tear everything down if initialization failed.
  Destroy();
  return PGL_FALSE;
}

void PGLContextImpl::Destroy() {
  delete gles2_implementation_;
  gles2_implementation_ = NULL;

  if (command_buffer_ && transfer_buffer_id_ != 0) {
    command_buffer_->DestroyTransferBuffer(transfer_buffer_id_);
    transfer_buffer_id_ = 0;
  }

  delete gles2_helper_;
  gles2_helper_ = NULL;

  delete command_buffer_;
  command_buffer_ = NULL;
}

PGLBoolean PGLContextImpl::MakeCurrent(PGLContextImpl* pgl_context) {
  if (!g_pgl_context_key_allocated)
    return PGL_FALSE;

  gpu::ThreadLocalSetValue(g_pgl_context_key, pgl_context);
  if (pgl_context) {
    gles2::SetGLContext(pgl_context->gles2_implementation_);

    // Don't request latest error status from service. Just use the locally
    // cached information from the last flush.
    // TODO(apatrick): I'm not sure if this should actually change the
    // current context if it fails. For now it gets changed even if it fails
    // becuase making GL calls with a NULL context crashes.
#if defined(ENABLE_NEW_NPDEVICE_API)
    if (pgl_context->command_buffer_->GetCachedError() != gpu::error::kNoError)
      return PGL_FALSE;
#else
    if (pgl_context->device_context_->error != NPDeviceContext3DError_NoError)
      return PGL_FALSE;
#endif
  } else {
    gles2::SetGLContext(NULL);
  }

  return PGL_TRUE;
}

PGLBoolean PGLContextImpl::SwapBuffers() {
  // Don't request latest error status from service. Just use the locally cached
  // information from the last flush.
#if defined(ENABLE_NEW_NPDEVICE_API)
  if (command_buffer_->GetCachedError() != gpu::error::kNoError)
    return PGL_FALSE;
#else
  if (device_context_->error != NPDeviceContext3DError_NoError)
    return PGL_FALSE;
#endif

  gles2_implementation_->SwapBuffers();
  return PGL_TRUE;
}

PGLInt PGLContextImpl::GetError() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kNoError) {
    return PGL_SUCCESS;
  } else {
    // All command buffer errors are unrecoverable. The error is treated as a
    // lost context: destroy the context and create another one.
    return PGL_CONTEXT_LOST;
  }
}
}  // namespace anonymous

extern "C" {
PGLBoolean pglInitialize() {
  if (g_pgl_context_key_allocated)
    return PGL_TRUE;

  gles2::Initialize();
  g_pgl_context_key = gpu::ThreadLocalAlloc();
  g_pgl_context_key_allocated = true;
  return PGL_TRUE;
}

PGLBoolean pglTerminate() {
  if (!g_pgl_context_key_allocated)
    return PGL_TRUE;

  gpu::ThreadLocalFree(g_pgl_context_key);
  g_pgl_context_key_allocated = false;
  g_pgl_context_key = 0;

  gles2::Terminate();
  return PGL_TRUE;
}

PGLContext pglCreateContext(NPP npp,
                            NPDevice* device,
                            NPDeviceContext3D* device_context) {
  if (!g_pgl_context_key_allocated)
    return NULL;

  PGLContextImpl* pgl_context = new PGLContextImpl(
      npp, device, device_context);
  if (pgl_context->Initialize(kTransferBufferSize)) {
    return pgl_context;
  }

  delete pgl_context;
  return NULL;
}

PGLBoolean pglMakeCurrent(PGLContext pgl_context) {
  return PGLContextImpl::MakeCurrent(static_cast<PGLContextImpl*>(pgl_context));
}

PGLContext pglGetCurrentContext(void) {
  if (!g_pgl_context_key_allocated)
    return NULL;

  return static_cast<PGLContext>(gpu::ThreadLocalGetValue(g_pgl_context_key));
}

PGLBoolean pglSwapBuffers(void) {
  PGLContextImpl* context = static_cast<PGLContextImpl*>(
      pglGetCurrentContext());
  if (!context)
    return PGL_FALSE;

  return context->SwapBuffers();
}

PGLBoolean pglDestroyContext(PGLContext pgl_context) {
  if (!g_pgl_context_key_allocated)
    return PGL_FALSE;

  if (!pgl_context)
    return PGL_FALSE;

  if (pgl_context == pglGetCurrentContext())
    pglMakeCurrent(PGL_NO_CONTEXT);

  delete static_cast<PGLContextImpl*>(pgl_context);
  return PGL_TRUE;
}

PGLInt pglGetError() {
  if (!g_pgl_context_key)
    return PGL_NOT_INITIALIZED;

  PGLContextImpl* context = static_cast<PGLContextImpl*>(
      pglGetCurrentContext());
  if (!context)
    return PGL_BAD_CONTEXT;

  return context->GetError();
}
}  // extern "C"
