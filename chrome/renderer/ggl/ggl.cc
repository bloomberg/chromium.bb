// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "build/build_config.h"

#include "base/ref_counted.h"
#include "base/singleton.h"
#include "base/thread_local.h"
#include "base/weak_ptr.h"
#include "chrome/renderer/command_buffer_proxy.h"
#include "chrome/renderer/ggl/ggl.h"
#include "chrome/renderer/gpu_channel_host.h"
#include "chrome/renderer/render_widget.h"
#include "ipc/ipc_channel_handle.h"

#if defined(ENABLE_GPU)
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#endif  // ENABLE_GPU

namespace ggl {

#if defined(ENABLE_GPU)

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const int32 kTransferBufferSize = 1024 * 1024;

base::ThreadLocalPointer<Context> g_current_context;

// Singleton used to initialize and terminate the gles2 library.
class GLES2Initializer {
 public:
  GLES2Initializer() {
    gles2::Initialize();
  }

  ~GLES2Initializer() {
    gles2::Terminate();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(GLES2Initializer);
};
}  // namespace anonymous

// Manages a GL context.
class Context : public base::SupportsWeakPtr<Context> {
 public:
  Context(GpuChannelHost* channel, Context* parent);
  ~Context();

  // Initialize a GGL context that can be used in association with a a GPU
  // channel acquired from a RenderWidget or RenderView.
  bool Initialize(gfx::NativeViewId view, const gfx::Size& size);

  // Asynchronously resizes an offscreen frame buffer.
  void ResizeOffscreen(const gfx::Size& size);

  // For an offscreen frame buffer context, return the frame buffer ID with
  // respect to the parent.
  uint32 parent_texture_id() const {
    return parent_texture_id_;
  }

  // Destroy all resources associated with the GGL context.
  void Destroy();

  // Make a GGL context current for the calling thread.
  static bool MakeCurrent(Context* context);

  // Display all content rendered since last call to SwapBuffers.
  // TODO(apatrick): support rendering to browser window. This function is
  // not useful at this point.
  bool SwapBuffers();

  // Get the current error code.
  Error GetError();

  // TODO(gman): Remove this.
  void DisableShaderTranslation();

 private:
  scoped_refptr<GpuChannelHost> channel_;
  base::WeakPtr<Context> parent_;
  uint32 parent_texture_id_;
  CommandBufferProxy* command_buffer_;
  gpu::gles2::GLES2CmdHelper* gles2_helper_;
  int32 transfer_buffer_id_;
  gpu::gles2::GLES2Implementation* gles2_implementation_;

  DISALLOW_COPY_AND_ASSIGN(Context);
};

Context::Context(GpuChannelHost* channel, Context* parent)
    : channel_(channel),
      parent_(parent ? parent->AsWeakPtr() : base::WeakPtr<Context>()),
      parent_texture_id_(0),
      command_buffer_(NULL),
      gles2_helper_(NULL),
      transfer_buffer_id_(0),
      gles2_implementation_(NULL) {
  DCHECK(channel);
}

Context::~Context() {
  Destroy();
}

bool Context::Initialize(gfx::NativeViewId view, const gfx::Size& size) {
  DCHECK(size.width() >= 0 && size.height() >= 0);

  if (channel_->state() != GpuChannelHost::CONNECTED)
    return false;

  // Ensure the gles2 library is initialized first in a thread safe way.
  Singleton<GLES2Initializer>::get();

  // Allocate a frame buffer ID with respect to the parent.
  if (parent_.get()) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    int32 token = parent_->gles2_helper_->InsertToken();
    parent_->gles2_helper_->WaitForToken(token);
    parent_texture_id_ = parent_->gles2_implementation_->MakeTextureId();
  }

  // Create a proxy to a command buffer in the GPU process.
  if (view) {
    command_buffer_ = channel_->CreateViewCommandBuffer(view);
  } else {
    CommandBufferProxy* parent_command_buffer =
        parent_.get() ? parent_->command_buffer_ : NULL;
    command_buffer_ = channel_->CreateOffscreenCommandBuffer(
        parent_command_buffer,
        size,
        parent_texture_id_);
  }
  if (!command_buffer_) {
    Destroy();
    return false;
  }

  // Initiaize the command buffer.
  if (!command_buffer_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ = new gpu::gles2::GLES2CmdHelper(command_buffer_);
  if (!gles2_helper_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_id_ =
      command_buffer_->CreateTransferBuffer(kTransferBufferSize);
  if (transfer_buffer_id_ < 0) {
    Destroy();
    return false;
  }

  // Map the buffer into the renderer process's address space.
  gpu::Buffer transfer_buffer =
      command_buffer_->GetTransferBuffer(transfer_buffer_id_);
  if (!transfer_buffer.ptr) {
    Destroy();
    return false;
  }

  // Create the object exposing the OpenGL API.
  gles2_implementation_ = new gpu::gles2::GLES2Implementation(
      gles2_helper_,
      transfer_buffer.size,
      transfer_buffer.ptr,
      transfer_buffer_id_,
      false);

  return true;
}

void Context::ResizeOffscreen(const gfx::Size& size) {
  DCHECK(size.width() > 0 && size.height() > 0);
  command_buffer_->ResizeOffscreenFrameBuffer(size);
}

void Context::Destroy() {
  if (parent_.get() && parent_texture_id_ != 0)
    parent_->gles2_implementation_->FreeTextureId(parent_texture_id_);

  delete gles2_implementation_;
  gles2_implementation_ = NULL;

  if (command_buffer_ && transfer_buffer_id_ != 0) {
    command_buffer_->DestroyTransferBuffer(transfer_buffer_id_);
    transfer_buffer_id_ = 0;
  }

  delete gles2_helper_;
  gles2_helper_ = NULL;

  if (channel_ && command_buffer_) {
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;
}

bool Context::MakeCurrent(Context* context) {
  g_current_context.Set(context);
  if (context) {
    gles2::SetGLContext(context->gles2_implementation_);

    // Don't request latest error status from service. Just use the locally
    // cached information from the last flush.
    // TODO(apatrick): I'm not sure if this should actually change the
    // current context if it fails. For now it gets changed even if it fails
    // because making GL calls with a NULL context crashes.
    if (context->command_buffer_->GetLastState().error != gpu::error::kNoError)
      return false;
  } else {
    gles2::SetGLContext(NULL);
  }

  return true;
}

bool Context::SwapBuffers() {
  // Don't request latest error status from service. Just use the locally cached
  // information from the last flush.
  if (command_buffer_->GetLastState().error != gpu::error::kNoError)
    return false;

  gles2_implementation_->SwapBuffers();
  return true;
}

Error Context::GetError() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kNoError) {
    return SUCCESS;
  } else {
    // All command buffer errors are unrecoverable. The error is treated as a
    // lost context: destroy the context and create another one.
    return CONTEXT_LOST;
  }
}

// TODO(gman): Remove This
void Context::DisableShaderTranslation() {
  gles2_implementation_->CommandBufferEnable(PEPPER3D_SKIP_GLSL_TRANSLATION);
}

#endif  // ENABLE_GPU

Context* CreateViewContext(GpuChannelHost* channel, gfx::NativeViewId view) {
#if defined(ENABLE_GPU)
  scoped_ptr<Context> context(new Context(channel, NULL));
  if (!context->Initialize(view, gfx::Size()))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

Context* CreateOffscreenContext(GpuChannelHost* channel,
                                Context* parent,
                                const gfx::Size& size) {
#if defined(ENABLE_GPU)
  scoped_ptr<Context> context(new Context(channel, parent));
  if (!context->Initialize(0, size))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

void ResizeOffscreenContext(Context* context, const gfx::Size& size) {
#if defined(ENABLE_GPU)
  context->ResizeOffscreen(size);
#endif
}

uint32 GetParentTextureId(Context* context) {
#if defined(ENABLE_GPU)
  return context->parent_texture_id();
#else
  return 0;
#endif
}

bool MakeCurrent(Context* context) {
#if defined(ENABLE_GPU)
  return Context::MakeCurrent(context);
#else
  return false;
#endif
}

Context* GetCurrentContext() {
#if defined(ENABLE_GPU)
  return g_current_context.Get();
#else
  return NULL;
#endif
}

bool SwapBuffers(Context* context) {
#if defined(ENABLE_GPU)
  if (!context)
    return false;

  return context->SwapBuffers();
#else
  return false;
#endif
}

bool DestroyContext(Context* context) {
#if defined(ENABLE_GPU)
  if (!context)
    return false;

  if (context == GetCurrentContext())
    MakeCurrent(NULL);

  delete context;
  return true;
#else
  return false;
#endif
}

Error GetError() {
#if defined(ENABLE_GPU)
  Context* context = GetCurrentContext();
  if (!context)
    return BAD_CONTEXT;

  return context->GetError();
#else
  return NOT_INITIALIZED;
#endif
}

// TODO(gman): Remove This
void DisableShaderTranslation(Context* context) {
#if defined(ENABLE_GPU)
  if (context) {
    context->DisableShaderTranslation();
  }
#endif
}
}  // namespace ggl
