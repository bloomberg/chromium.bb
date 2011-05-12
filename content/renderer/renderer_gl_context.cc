// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/renderer_gl_context.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/shared_memory.h"
#include "content/common/view_messages.h"
#include "content/renderer/command_buffer_proxy.h"
#include "content/renderer/gpu_channel_host.h"
#include "content/renderer/gpu_video_service_host.h"
#include "content/renderer/media/gles2_video_decode_context.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_widget.h"
#include "content/renderer/transport_texture_host.h"
#include "content/renderer/transport_texture_service.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel_handle.h"

#if defined(ENABLE_GPU)
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/common/constants.h"
#include "gpu/GLES2/gles2_command_buffer.h"
#endif  // ENABLE_GPU

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const int32 kTransferBufferSize = 1024 * 1024;

const uint32 kMaxLatchesPerRenderer = 2048;
const uint32 kInvalidLatchId = 0xffffffffu;

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

// Shared memory allocator for latches. Creates a block of shared memory for
// each renderer process.
class LatchAllocator {
 public:
  static LatchAllocator* GetInstance();
  static uint32 size() { return kMaxLatchesPerRenderer*sizeof(uint32); }
  static const uint32_t kFreeLatch = 0xffffffffu;

  LatchAllocator();
  ~LatchAllocator();

  base::SharedMemoryHandle handle() const { return shm_->handle(); }
  base::SharedMemory* shared_memory() { return shm_.get(); }

  bool AllocateLatch(uint32* latch_id);
  bool FreeLatch(uint32 latch_id);

 private:
  friend struct DefaultSingletonTraits<LatchAllocator>;

  scoped_ptr<base::SharedMemory> shm_;
  // Pointer to mapped shared memory.
  volatile uint32* latches_;

  DISALLOW_COPY_AND_ASSIGN(LatchAllocator);
};

////////////////////////////////////////////////////////////////////////////////
/// LatchAllocator implementation

LatchAllocator* LatchAllocator::GetInstance() {
  return Singleton<LatchAllocator>::get();
}

LatchAllocator::LatchAllocator() {
  base::SharedMemoryHandle handle;
  RenderThread* render_thread = RenderThread::current();
  if (!render_thread->Send(
      new ViewHostMsg_AllocateSharedMemoryBuffer(size(), &handle))) {
    NOTREACHED() << "failed to send sync IPC";
  }

  if (!base::SharedMemory::IsHandleValid(handle)) {
    NOTREACHED() << "failed to create shared memory";
  }

  // Handle is closed by the SharedMemory object below. This stops
  // base::FileDescriptor from closing it as well.
#if defined(OS_POSIX)
  handle.auto_close = false;
#endif

  shm_.reset(new base::SharedMemory(handle, false));
  if (!shm_->Map(size())) {
    NOTREACHED() << "failed to map shared memory";
  }

  latches_ = static_cast<uint32*>(shm_->memory());
  // Mark all latches as unallocated.
  for (uint32 i = 0; i < kMaxLatchesPerRenderer; ++i)
    latches_[i] = kFreeLatch;
}

LatchAllocator::~LatchAllocator() {
}

bool LatchAllocator::AllocateLatch(uint32* latch_id) {
  for (uint32 i = 0; i < kMaxLatchesPerRenderer; ++i) {
    if (latches_[i] == kFreeLatch) {
      // mark latch as taken and blocked.
      // 0 means waiter will block, 1 means waiter will pass.
      latches_[i] = 0;
      *latch_id = i;
      return true;
    }
  }
  return false;
}

bool LatchAllocator::FreeLatch(uint32 latch_id) {
  if (latch_id < kMaxLatchesPerRenderer && latches_[latch_id] != kFreeLatch) {
    latches_[latch_id] = kFreeLatch;
    return true;
  }
  return false;
}

////////////////////////////////////////////////////////////////////////////////

static base::LazyInstance<GLES2Initializer> g_gles2_initializer(
    base::LINKER_INITIALIZED);

}  // namespace anonymous

RendererGLContext::~RendererGLContext() {
  Destroy();
}

RendererGLContext* RendererGLContext::CreateViewContext(
    GpuChannelHost* channel,
    gfx::PluginWindowHandle render_surface,
    int render_view_id,
    const char* allowed_extensions,
    const int32* attrib_list,
    const GURL& active_url) {
#if defined(ENABLE_GPU)
  scoped_ptr<RendererGLContext> context(new RendererGLContext(channel, NULL));
  if (!context->Initialize(
      true,
      render_surface,
      render_view_id,
      gfx::Size(),
      allowed_extensions,
      attrib_list,
      active_url))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

#if defined(OS_MACOSX)
void RendererGLContext::ResizeOnscreen(const gfx::Size& size) {
  DCHECK(size.width() > 0 && size.height() > 0);
  size_ = size;
  command_buffer_->SetWindowSize(size);
}
#endif

RendererGLContext* RendererGLContext::CreateOffscreenContext(
    GpuChannelHost* channel,
    RendererGLContext* parent,
    const gfx::Size& size,
    const char* allowed_extensions,
    const int32* attrib_list,
    const GURL& active_url) {
#if defined(ENABLE_GPU)
  scoped_ptr<RendererGLContext> context(new RendererGLContext(channel, parent));
  if (!context->Initialize(
      false,
      gfx::kNullPluginWindow,
      0,
      size,
      allowed_extensions,
      attrib_list,
      active_url))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

void RendererGLContext::ResizeOffscreen(const gfx::Size& size) {
  DCHECK(size.width() > 0 && size.height() > 0);
  if (size_ != size) {
    command_buffer_->ResizeOffscreenFrameBuffer(size);
    size_ = size;
  }
}

uint32 RendererGLContext::GetParentTextureId() {
  return parent_texture_id_;
}

uint32 RendererGLContext::CreateParentTexture(const gfx::Size& size) {
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

void RendererGLContext::DeleteParentTexture(uint32 texture) {
  if (parent_.get()) {
    if (!MakeCurrent(parent_.get()))
      return;
    parent_->gles2_implementation_->DeleteTextures(1, &texture);
  }
}

void RendererGLContext::SetSwapBuffersCallback(Callback0::Type* callback) {
  swap_buffers_callback_.reset(callback);
}

void RendererGLContext::SetContextLostCallback(Callback0::Type* callback) {
  context_lost_callback_.reset(callback);
}

bool RendererGLContext::MakeCurrent(RendererGLContext* context) {
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

bool RendererGLContext::SwapBuffers() {
  TRACE_EVENT1("gpu", "RendererGLContext::SwapBuffers", "frame", frame_number_);
  frame_number_++;

  // Don't request latest error status from service. Just use the locally cached
  // information from the last flush.
  if (command_buffer_->GetLastState().error != gpu::error::kNoError)
    return false;

  gles2_implementation_->SwapBuffers();
  return true;
}

media::VideoDecodeEngine* RendererGLContext::CreateVideoDecodeEngine() {
  return channel_->gpu_video_service_host()->CreateVideoDecoder(
      command_buffer_->route_id());
}

media::VideoDecodeContext* RendererGLContext::CreateVideoDecodeContext(
    MessageLoop* message_loop, bool hardware_decoder) {
  return new Gles2VideoDecodeContext(message_loop, hardware_decoder, this);
}

scoped_refptr<TransportTextureHost>
RendererGLContext::CreateTransportTextureHost() {
  return channel_->transport_texture_service()->CreateTransportTextureHost(
      this, command_buffer_->route_id());
}

RendererGLContext::Error RendererGLContext::GetError() {
  gpu::CommandBuffer::State state = command_buffer_->GetState();
  if (state.error == gpu::error::kNoError) {
    Error old_error = last_error_;
    last_error_ = SUCCESS;
    return old_error;
  } else {
    // All command buffer errors are unrecoverable. The error is treated as a
    // lost context: destroy the context and create another one.
    return CONTEXT_LOST;
  }
}

bool RendererGLContext::IsCommandBufferContextLost() {
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  return state.error == gpu::error::kLostContext;
}

CommandBufferProxy* RendererGLContext::GetCommandBufferProxy() {
  return command_buffer_;
}

// TODO(gman): Remove This
void RendererGLContext::DisableShaderTranslation() {
  gles2_implementation_->CommandBufferEnableCHROMIUM(
      PEPPER3D_SKIP_GLSL_TRANSLATION);
}

gpu::gles2::GLES2Implementation* RendererGLContext::GetImplementation() {
  return gles2_implementation_;
}

RendererGLContext::RendererGLContext(GpuChannelHost* channel,
                                     RendererGLContext* parent)
    : channel_(channel),
      parent_(parent ?
          parent->AsWeakPtr() : base::WeakPtr<RendererGLContext>()),
      parent_texture_id_(0),
      child_to_parent_latch_(kInvalidLatchId),
      parent_to_child_latch_(kInvalidLatchId),
      latch_transfer_buffer_id_(-1),
      command_buffer_(NULL),
      gles2_helper_(NULL),
      transfer_buffer_id_(-1),
      gles2_implementation_(NULL),
      last_error_(SUCCESS),
      frame_number_(0) {
  DCHECK(channel);
}

bool RendererGLContext::Initialize(bool onscreen,
                                   gfx::PluginWindowHandle render_surface,
                                   int render_view_id,
                                   const gfx::Size& size,
                                   const char* allowed_extensions,
                                   const int32* attrib_list,
                                   const GURL& active_url) {
  DCHECK(size.width() >= 0 && size.height() >= 0);
  TRACE_EVENT2("gpu", "RendererGLContext::Initialize",
                   "on_screen", onscreen, "num_pixels", size.GetArea());

  if (channel_->state() != GpuChannelHost::kConnected)
    return false;

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  // Allocate a frame buffer ID with respect to the parent.
  if (parent_.get()) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    int32 token = parent_->gles2_helper_->InsertToken();
    parent_->gles2_helper_->WaitForToken(token);
    parent_texture_id_ = parent_->gles2_implementation_->MakeTextureId();
  }

  std::vector<int32> attribs;
  while (attrib_list) {
    int32 attrib = *attrib_list++;
    switch (attrib) {
      // Known attributes
      case ALPHA_SIZE:
      case BLUE_SIZE:
      case GREEN_SIZE:
      case RED_SIZE:
      case DEPTH_SIZE:
      case STENCIL_SIZE:
      case SAMPLES:
      case SAMPLE_BUFFERS:
        attribs.push_back(attrib);
        attribs.push_back(*attrib_list++);
        break;
      case NONE:
        attribs.push_back(attrib);
        attrib_list = NULL;
        break;
      default:
        last_error_ = BAD_ATTRIBUTE;
        attribs.push_back(NONE);
        attrib_list = NULL;
        break;
    }
  }

  // Create a proxy to a command buffer in the GPU process.
  if (onscreen) {
    if (render_surface == gfx::kNullPluginWindow) {
      LOG(ERROR) << "Invalid surface handle for onscreen context.";
      command_buffer_ = NULL;
    } else {
      command_buffer_ = channel_->CreateViewCommandBuffer(
          render_surface,
          render_view_id,
          allowed_extensions,
          attribs,
          active_url);
    }
  } else {
    CommandBufferProxy* parent_command_buffer =
        parent_.get() ? parent_->command_buffer_ : NULL;
    command_buffer_ = channel_->CreateOffscreenCommandBuffer(
        parent_command_buffer,
        size,
        allowed_extensions,
        attribs,
        parent_texture_id_,
        active_url);
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

  command_buffer_->SetSwapBuffersCallback(
      NewCallback(this, &RendererGLContext::OnSwapBuffers));

  command_buffer_->SetChannelErrorCallback(
      NewCallback(this, &RendererGLContext::OnContextLost));

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ = new gpu::gles2::GLES2CmdHelper(command_buffer_);
  if (!gles2_helper_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  // Create a transfer buffer used to copy resources between the renderer
  // process and the GPU process.
  transfer_buffer_id_ =
      command_buffer_->CreateTransferBuffer(kTransferBufferSize,
                                            gpu::kCommandBufferSharedMemoryId);
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

  // Register transfer buffer so that the context can access latches.
  LatchAllocator* latch_shm = LatchAllocator::GetInstance();
  latch_transfer_buffer_id_ = command_buffer_->RegisterTransferBuffer(
      latch_shm->shared_memory(), LatchAllocator::size(),
      gpu::kLatchSharedMemoryId);
  if (latch_transfer_buffer_id_ != gpu::kLatchSharedMemoryId) {
    Destroy();
    return false;
  }

  // If this is a child context, setup latches for synchronization between child
  // and parent.
  if (parent_.get()) {
    if (!CreateLatch(&child_to_parent_latch_) ||
        !CreateLatch(&parent_to_child_latch_)) {
      Destroy();
      return false;
    }
  }

  // Create the object exposing the OpenGL API.
  gles2_implementation_ = new gpu::gles2::GLES2Implementation(
      gles2_helper_,
      transfer_buffer.size,
      transfer_buffer.ptr,
      transfer_buffer_id_,
      false);

  size_ = size;

  return true;
}

void RendererGLContext::Destroy() {
  if (parent_.get() && parent_texture_id_ != 0) {
    parent_->gles2_implementation_->FreeTextureId(parent_texture_id_);
    parent_texture_id_ = 0;
  }

  delete gles2_implementation_;
  gles2_implementation_ = NULL;

  if (child_to_parent_latch_ != kInvalidLatchId) {
    DestroyLatch(child_to_parent_latch_);
    child_to_parent_latch_ = kInvalidLatchId;
  }
  if (parent_to_child_latch_ != kInvalidLatchId) {
    DestroyLatch(parent_to_child_latch_);
    parent_to_child_latch_ = kInvalidLatchId;
  }
  if (command_buffer_ && latch_transfer_buffer_id_ != -1) {
    command_buffer_->DestroyTransferBuffer(latch_transfer_buffer_id_);
    latch_transfer_buffer_id_ = -1;
  }

  if (command_buffer_ && transfer_buffer_id_ != -1) {
    command_buffer_->DestroyTransferBuffer(transfer_buffer_id_);
    transfer_buffer_id_ = -1;
  }

  delete gles2_helper_;
  gles2_helper_ = NULL;

  if (channel_ && command_buffer_) {
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;
}

void RendererGLContext::OnSwapBuffers() {
  if (swap_buffers_callback_.get())
    swap_buffers_callback_->Run();
}

void RendererGLContext::OnContextLost() {
  if (context_lost_callback_.get())
    context_lost_callback_->Run();
}

bool RendererGLContext::CreateLatch(uint32* ret_latch) {
  return LatchAllocator::GetInstance()->AllocateLatch(ret_latch);
}

bool RendererGLContext::DestroyLatch(uint32 latch) {
  return LatchAllocator::GetInstance()->FreeLatch(latch);
}

bool RendererGLContext::GetParentToChildLatch(uint32* parent_to_child_latch) {
  if (parent_.get()) {
    *parent_to_child_latch = parent_to_child_latch_;
    return true;
  }
  return false;
}

bool RendererGLContext::GetChildToParentLatch(uint32* child_to_parent_latch) {
  if (parent_.get()) {
    *child_to_parent_latch = child_to_parent_latch_;
    return true;
  }
  return false;
}
