// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <build/build_config.h>

#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/thread_local.h"
#include "gpu/pgl/command_buffer_pepper.h"
#include "gpu/pgl/pgl.h"

namespace {
const int32 kTransferBufferSize = 512 * 1024;

class PGLContextImpl {
 public:
  PGLContextImpl(NPP npp,
              NPDevice* device,
              NPDeviceContext3D* device_context);
  ~PGLContextImpl();

  // Initlaize a PGL context with a transfer buffer of a particular size.
  bool Initialize(int32 transfer_buffer_size);

  // Destroy all resources associated with the PGL context.
  void Destroy();

  // Make a PGL context current for the calling thread.
  static bool MakeCurrent(PGLContextImpl* pgl_context);

  // Display all content rendered since last call to SwapBuffers.
  bool SwapBuffers();

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

bool PGLContextImpl::Initialize(int32 transfer_buffer_size) {
  // Create and initialize the objects required to issue GLES2 calls.
  command_buffer_ = new CommandBufferPepper(
      npp_, device_, device_context_);
  gles2_helper_ = new gpu::gles2::GLES2CmdHelper(command_buffer_);
  if (gles2_helper_->Initialize()) {
    transfer_buffer_id_ =
        command_buffer_->CreateTransferBuffer(kTransferBufferSize);
    gpu::Buffer transfer_buffer =
        command_buffer_->GetTransferBuffer(transfer_buffer_id_);
    if (transfer_buffer.ptr) {
      gles2_implementation_ = new gpu::gles2::GLES2Implementation(
          gles2_helper_,
          transfer_buffer.size,
          transfer_buffer.ptr,
          transfer_buffer_id_);
      return true;
    }
  }

  // Tear everything down if initialization failed.
  Destroy();
  return false;
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

bool PGLContextImpl::MakeCurrent(PGLContextImpl* pgl_context) {
  if (!g_pgl_context_key)
    return false;

  gpu::ThreadLocalSetValue(g_pgl_context_key, pgl_context);
  if (pgl_context)
    gles2::SetGLContext(pgl_context->gles2_implementation_);
  else
    gles2::SetGLContext(NULL);

  return true;
}

bool PGLContextImpl::SwapBuffers() {
  gles2_implementation_->SwapBuffers();
  return true;
}
}  // namespace anonymous

extern "C" {

PGLBoolean pglInitialize() {
  if (g_pgl_context_key)
    return true;

  gles2::Initialize();
  g_pgl_context_key = gpu::ThreadLocalAlloc();
  return true;
}

PGLBoolean pglTerminate() {
  if (!g_pgl_context_key)
    return true;

  gpu::ThreadLocalFree(g_pgl_context_key);
  gles2::Terminate();
  return true;
}

PGLContext pglCreateContext(NPP npp,
                            NPDevice* device,
                            NPDeviceContext3D* device_context) {
  if (!g_pgl_context_key)
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
  if (!g_pgl_context_key)
    return NULL;

  return static_cast<PGLContext>(gpu::ThreadLocalGetValue(g_pgl_context_key));
}

PGLBoolean pglSwapBuffers(void) {
  PGLContextImpl* context = static_cast<PGLContextImpl*>(
      pglGetCurrentContext());
  if (!context)
    return false;

  return context->SwapBuffers();
}

PGLBoolean pglDestroyContext(PGLContext pgl_context) {
  if (!g_pgl_context_key)
    return NULL;

  if (!pgl_context)
    return false;

  if (pgl_context == pglGetCurrentContext())
    pglMakeCurrent(NULL);

  delete static_cast<PGLContextImpl*>(pgl_context);
  return true;
}

}  // extern "C"
