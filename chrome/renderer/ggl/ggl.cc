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
#include "chrome/renderer/gpu_video_service_host.h"
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
  bool Initialize(gfx::NativeViewId view,
                  int render_view_id,
                  const gfx::Size& size);

#if defined(OS_MACOSX)
  // Asynchronously resizes an onscreen frame buffer.
  void ResizeOnscreen(const gfx::Size& size);
#endif

  // Asynchronously resizes an offscreen frame buffer.
  void ResizeOffscreen(const gfx::Size& size);

  // Provides a callback that will be invoked when SwapBuffers has completed
  // service side.
  void SetSwapBuffersCallback(Callback1<Context*>::Type* callback) {
    swap_buffers_callback_.reset(callback);
  }

  // For an offscreen frame buffer context, return the frame buffer ID with
  // respect to the parent.
  uint32 parent_texture_id() const {
    return parent_texture_id_;
  }

  uint32 CreateParentTexture(const gfx::Size& size) const;
  void DeleteParentTexture(uint32 texture) const;

  // Destroy all resources associated with the GGL context.
  void Destroy();

  // Make a GGL context current for the calling thread.
  static bool MakeCurrent(Context* context);

  // Display all content rendered since last call to SwapBuffers.
  // TODO(apatrick): support rendering to browser window. This function is
  // not useful at this point.
  bool SwapBuffers();

  // Create a hardware accelerated video decoder associated with this context.
  GpuVideoDecoderHost* CreateVideoDecoder();

  // Get the current error code.
  Error GetError();

  // TODO(gman): Remove this.
  void DisableShaderTranslation();

 private:
  void OnSwapBuffers();

  scoped_refptr<GpuChannelHost> channel_;
  base::WeakPtr<Context> parent_;
  scoped_ptr<Callback1<Context*>::Type> swap_buffers_callback_;
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

bool Context::Initialize(gfx::NativeViewId view,
                         int render_view_id,
                         const gfx::Size& size) {
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
    command_buffer_ =
        channel_->CreateViewCommandBuffer(view, render_view_id);
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

  command_buffer_->SetSwapBuffersCallback(NewCallback(this,
                                                      &Context::OnSwapBuffers));

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

#if defined(OS_MACOSX)
void Context::ResizeOnscreen(const gfx::Size& size) {
  DCHECK(size.width() > 0 && size.height() > 0);
  command_buffer_->SetWindowSize(size);
}
#endif

void Context::ResizeOffscreen(const gfx::Size& size) {
  DCHECK(size.width() > 0 && size.height() > 0);
  command_buffer_->ResizeOffscreenFrameBuffer(size);
}

uint32 Context::CreateParentTexture(const gfx::Size& size) const {
  // Allocate a texture ID with respect to the parent.
  if (parent_.get()) {
    if (!MakeCurrent(parent_.get()))
      return 0;
    uint32 texture_id = parent_->gles2_implementation_->MakeTextureId();
    parent_->gles2_implementation_->BindTexture(GL_TEXTURE_2D, texture_id);
    parent_->gles2_implementation_->TexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    parent_->gles2_implementation_->TexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    parent_->gles2_implementation_->TexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    parent_->gles2_implementation_->TexParameteri(
        GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    parent_->gles2_implementation_->TexImage2D(GL_TEXTURE_2D,
        0,  // mip level
        GL_RGBA,
        size.width(),
        size.height(),
        0,  // border
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        NULL);
    // Make sure that the parent texture's storage is allocated before we let
    // the caller attempt to use it.
    int32 token = parent_->gles2_helper_->InsertToken();
    parent_->gles2_helper_->WaitForToken(token);
    return texture_id;
  }
  return 0;
}

void Context::DeleteParentTexture(uint32 texture) const {
  if (parent_.get()) {
    if (!MakeCurrent(parent_.get()))
      return;
    parent_->gles2_implementation_->DeleteTextures(1, &texture);
  }
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

GpuVideoDecoderHost* Context::CreateVideoDecoder() {
  return GpuVideoServiceHost::get()->CreateVideoDecoder(
      command_buffer_->route_id());
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

void Context::OnSwapBuffers() {
  if (swap_buffers_callback_.get())
    swap_buffers_callback_->Run(this);
}

#endif  // ENABLE_GPU

Context* CreateViewContext(GpuChannelHost* channel,
                           gfx::NativeViewId view,
                           int render_view_id) {
#if defined(ENABLE_GPU)
  scoped_ptr<Context> context(new Context(channel, NULL));
  if (!context->Initialize(view, render_view_id, gfx::Size()))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

#if defined(OS_MACOSX)
void ResizeOnscreenContext(Context* context, const gfx::Size& size) {
#if defined(ENABLE_GPU)
  context->ResizeOnscreen(size);
#endif
}
#endif

Context* CreateOffscreenContext(GpuChannelHost* channel,
                                Context* parent,
                                const gfx::Size& size) {
#if defined(ENABLE_GPU)
  scoped_ptr<Context> context(new Context(channel, parent));
  if (!context->Initialize(0, 0, size))
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

uint32 CreateParentTexture(Context* context, const gfx::Size& size) {
#if defined(ENABLE_GPU)
  return context->CreateParentTexture(size);
#else
  return 0;
#endif
}

void DeleteParentTexture(Context* context, uint32 texture) {
#if defined(ENABLE_GPU)
  context->DeleteParentTexture(texture);
#endif
}

void SetSwapBuffersCallback(Context* context,
                            Callback1<Context*>::Type* callback) {
#if defined(ENABLE_GPU)
  context->SetSwapBuffersCallback(callback);
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

GpuVideoDecoderHost* CreateVideoDecoder(Context* context) {
  return context->CreateVideoDecoder();
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
