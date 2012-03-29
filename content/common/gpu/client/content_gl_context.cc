// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/client/content_gl_context.h"

#include "base/bind.h"
#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/shared_memory.h"
#include "content/common/gpu/client/command_buffer_proxy_impl.h"
#include "content/common/gpu/client/gpu_channel_host.h"
#include "googleurl/src/gurl.h"
#include "ipc/ipc_channel_handle.h"

#if defined(ENABLE_GPU)
#include "gpu/command_buffer/client/gles2_cmd_helper.h"
#include "gpu/command_buffer/client/gles2_implementation.h"
#include "gpu/command_buffer/client/gles2_lib.h"
#include "gpu/command_buffer/client/transfer_buffer.h"
#include "gpu/command_buffer/common/constants.h"
#endif  // ENABLE_GPU

namespace {

const int32 kCommandBufferSize = 1024 * 1024;
// TODO(kbr): make the transfer buffer size configurable via context
// creation attributes.
const size_t kStartTransferBufferSize = 1 * 1024 * 1024;
const size_t kMinTransferBufferSize = 1 * 256 * 1024;
const size_t kMaxTransferBufferSize = 16 * 1024 * 1024;

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

////////////////////////////////////////////////////////////////////////////////

base::LazyInstance<GLES2Initializer> g_gles2_initializer =
    LAZY_INSTANCE_INITIALIZER;

////////////////////////////////////////////////////////////////////////////////

#if defined(ENABLE_GPU)
ContentGLContext::ContextLostReason ConvertReason(
    gpu::error::ContextLostReason reason) {
  switch (reason) {
  case gpu::error::kGuilty:
    return ContentGLContext::kGuilty;
  case gpu::error::kInnocent:
    return ContentGLContext::kInnocent;
  case gpu::error::kUnknown:
    return ContentGLContext::kUnknown;
  }
  NOTREACHED();
  return ContentGLContext::kUnknown;
}
#endif

}  // namespace

ContentGLContext::~ContentGLContext() {
  Destroy();
}

ContentGLContext* ContentGLContext::CreateViewContext(
    GpuChannelHost* channel,
    int32 surface_id,
    ContentGLContext* share_group,
    const char* allowed_extensions,
    const int32* attrib_list,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
#if defined(ENABLE_GPU)
  scoped_ptr<ContentGLContext> context(new ContentGLContext(channel));
  if (!context->Initialize(
      true,
      surface_id,
      gfx::Size(),
      share_group,
      allowed_extensions,
      attrib_list,
      active_url,
      gpu_preference))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

ContentGLContext* ContentGLContext::CreateOffscreenContext(
    GpuChannelHost* channel,
    const gfx::Size& size,
    ContentGLContext* share_group,
    const char* allowed_extensions,
    const int32* attrib_list,
    const GURL& active_url,
    gfx::GpuPreference gpu_preference) {
#if defined(ENABLE_GPU)
  scoped_ptr<ContentGLContext> context(new ContentGLContext(channel));
  if (!context->Initialize(
      false,
      0,
      size,
      share_group,
      allowed_extensions,
      attrib_list,
      active_url,
      gpu_preference))
    return NULL;

  return context.release();
#else
  return NULL;
#endif
}

bool ContentGLContext::SetParent(ContentGLContext* new_parent) {
  if (parent_.get() == new_parent)
    return true;

  // Allocate a texture ID with respect to the parent and change the parent.
  uint32 new_parent_texture_id = 0;
  if (command_buffer_) {
    if (new_parent) {
      TRACE_EVENT0("gpu", "ContentGLContext::SetParent::flushParent");
      // Flush any remaining commands in the parent context to make sure the
      // texture id accounting stays consistent.
      int32 token = new_parent->gles2_helper_->InsertToken();
      new_parent->gles2_helper_->WaitForToken(token);
      new_parent_texture_id =
        new_parent->gles2_implementation_->MakeTextureId();

      if (!command_buffer_->SetParent(new_parent->command_buffer_,
                                      new_parent_texture_id)) {
        new_parent->gles2_implementation_->FreeTextureId(parent_texture_id_);
        return false;
      }
    } else {
      if (!command_buffer_->SetParent(NULL, 0))
        return false;
    }
  }

  // Free the previous parent's texture ID.
  if (parent_.get() && parent_texture_id_ != 0) {
    // Flush any remaining commands in the parent context to make sure the
    // texture id accounting stays consistent.
    gpu::gles2::GLES2Implementation* parent_gles2 =
        parent_->GetImplementation();
    parent_gles2->helper()->CommandBufferHelper::Finish();
    parent_gles2->FreeTextureId(parent_texture_id_);
  }

  if (new_parent) {
    parent_ = new_parent->AsWeakPtr();
    parent_texture_id_ = new_parent_texture_id;
  } else {
    parent_.reset();
    parent_texture_id_ = 0;
  }

  return true;
}

uint32 ContentGLContext::GetParentTextureId() {
  return parent_texture_id_;
}

uint32 ContentGLContext::CreateParentTexture(const gfx::Size& size) {
  uint32 texture_id = 0;
  gles2_implementation_->GenTextures(1, &texture_id);
  gles2_implementation_->Flush();
  return texture_id;
}

void ContentGLContext::DeleteParentTexture(uint32 texture) {
  gles2_implementation_->DeleteTextures(1, &texture);
}

void ContentGLContext::SetContextLostCallback(
    const base::Callback<void (ContextLostReason)>& callback) {
  context_lost_callback_ = callback;
}

bool ContentGLContext::MakeCurrent(ContentGLContext* context) {
  if (context) {
    DCHECK(context->CalledOnValidThread());
    gles2::SetGLContext(context->gles2_implementation_);

    // Don't request latest error status from service. Just use the locally
    // cached information from the last flush.
    // TODO(apatrick): I'm not sure if this should actually change the
    // current context if it fails. For now it gets changed even if it fails
    // because making GL calls with a NULL context crashes.
    if (context->command_buffer_->GetLastError() != gpu::error::kNoError)
      return false;
  } else {
    gles2::SetGLContext(NULL);
  }

  return true;
}

bool ContentGLContext::SwapBuffers() {
  TRACE_EVENT1("gpu", "ContentGLContext::SwapBuffers", "frame", frame_number_);
  frame_number_++;

  // Don't request latest error status from service. Just use the locally cached
  // information from the last flush.
  if (command_buffer_->GetLastState().error != gpu::error::kNoError)
    return false;

  gles2_implementation_->SwapBuffers();

  return true;
}

bool ContentGLContext::Echo(const base::Closure& task) {
  return command_buffer_->Echo(task);
}

ContentGLContext::Error ContentGLContext::GetError() {
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

bool ContentGLContext::IsCommandBufferContextLost() {
  // If the channel shut down unexpectedly, let that supersede the
  // command buffer's state.
  if (channel_->state() == GpuChannelHost::kLost)
    return true;
  gpu::CommandBuffer::State state = command_buffer_->GetLastState();
  return state.error == gpu::error::kLostContext;
}

CommandBufferProxy* ContentGLContext::GetCommandBufferProxy() {
  return command_buffer_;
}

int ContentGLContext::GetGPUProcessID() {
  return channel_->gpu_process_id();
}

int ContentGLContext::GetChannelID() {
  return channel_->client_id();
}

int ContentGLContext::GetContextID() {
  return command_buffer_->GetRouteID();
}

bool ContentGLContext::SetSurfaceVisible(bool visible) {
  return GetCommandBufferProxy()->SetSurfaceVisible(visible);
}

bool ContentGLContext::DiscardBackbuffer() {
  return GetCommandBufferProxy()->DiscardBackbuffer();
}

bool ContentGLContext::EnsureBackbuffer() {
  return GetCommandBufferProxy()->EnsureBackbuffer();
}

void ContentGLContext::SetMemoryAllocationChangedCallback(
    const base::Callback<void(const GpuMemoryAllocationForRenderer&)>&
        callback) {
  GetCommandBufferProxy()->SetMemoryAllocationChangedCallback(callback);
}

gpu::gles2::GLES2Implementation* ContentGLContext::GetImplementation() {
  return gles2_implementation_;
}

ContentGLContext::ContentGLContext(GpuChannelHost* channel)
    : channel_(channel),
      parent_(base::WeakPtr<ContentGLContext>()),
      parent_texture_id_(0),
      command_buffer_(NULL),
      gles2_helper_(NULL),
      transfer_buffer_(NULL),
      gles2_implementation_(NULL),
      last_error_(SUCCESS),
      frame_number_(0) {
  DCHECK(channel);
}

bool ContentGLContext::Initialize(bool onscreen,
                                  int32 surface_id,
                                  const gfx::Size& size,
                                  ContentGLContext* share_group,
                                  const char* allowed_extensions,
                                  const int32* attrib_list,
                                  const GURL& active_url,
                                  gfx::GpuPreference gpu_preference) {
  DCHECK(CalledOnValidThread());
  DCHECK(size.width() >= 0 && size.height() >= 0);
  TRACE_EVENT2("gpu", "ContentGLContext::Initialize",
                   "on_screen", onscreen, "num_pixels", size.GetArea());

  if (channel_->state() != GpuChannelHost::kConnected)
    return false;

  // Ensure the gles2 library is initialized first in a thread safe way.
  g_gles2_initializer.Get();

  bool share_resources = true;
  bool bind_generates_resources = true;
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
      case SHARE_RESOURCES:
        share_resources = !!(*attrib_list++);
        break;
      case BIND_GENERATES_RESOURCES:
        bind_generates_resources = !!(*attrib_list++);
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
    TRACE_EVENT0("gpu",
                 "ContentGLContext::Initialize::CreateViewCommandBuffer");
    command_buffer_ = channel_->CreateViewCommandBuffer(
        surface_id,
        share_group ? share_group->command_buffer_ : NULL,
        allowed_extensions,
        attribs,
        active_url,
        gpu_preference);
  } else {
    command_buffer_ = channel_->CreateOffscreenCommandBuffer(
        size,
        share_group ? share_group->command_buffer_ : NULL,
        allowed_extensions,
        attribs,
        active_url,
        gpu_preference);
  }
  if (!command_buffer_) {
    Destroy();
    return false;
  }

  {
    TRACE_EVENT0("gpu",
                 "ContentGLContext::Initialize::InitializeCommandBuffer");
    // Initiaize the command buffer.
    if (!command_buffer_->Initialize()) {
      Destroy();
      return false;
    }
  }

  command_buffer_->SetChannelErrorCallback(
      base::Bind(&ContentGLContext::OnContextLost, base::Unretained(this)));

  // Create the GLES2 helper, which writes the command buffer protocol.
  gles2_helper_ = new gpu::gles2::GLES2CmdHelper(command_buffer_);
  if (!gles2_helper_->Initialize(kCommandBufferSize)) {
    Destroy();
    return false;
  }

  {
    TRACE_EVENT0("gpu", "ContentGLContext::Initialize::CreateTransferBuffer");
    // Create a transfer buffer used to copy resources between the renderer
    // process and the GPU process.
    transfer_buffer_ = new gpu::TransferBuffer(gles2_helper_);
  }

  // Create the object exposing the OpenGL API.
  gles2_implementation_ = new gpu::gles2::GLES2Implementation(
      gles2_helper_,
      NULL,
      transfer_buffer_,
      share_resources,
      bind_generates_resources);

  if (!gles2_implementation_->Initialize(
      kStartTransferBufferSize,
      kMinTransferBufferSize,
      kMaxTransferBufferSize)) {
    Destroy();
    return false;
  }

  return true;
}

void ContentGLContext::Destroy() {
  TRACE_EVENT0("gpu", "ContentGLContext::Destroy");
  DCHECK(CalledOnValidThread());
  SetParent(NULL);

  if (gles2_implementation_) {
    // First flush the context to ensure that any pending frees of resources
    // are completed. Otherwise, if this context is part of a share group,
    // those resources might leak. Also, any remaining side effects of commands
    // issued on this context might not be visible to other contexts in the
    // share group.
    gles2_implementation_->Flush();

    delete gles2_implementation_;
    gles2_implementation_ = NULL;
  }

  if (transfer_buffer_) {
    delete transfer_buffer_;
    transfer_buffer_ = NULL;
  }

  delete gles2_helper_;
  gles2_helper_ = NULL;

  if (channel_ && command_buffer_) {
    channel_->DestroyCommandBuffer(command_buffer_);
    command_buffer_ = NULL;
  }

  channel_ = NULL;
}

void ContentGLContext::OnContextLost() {
  if (!context_lost_callback_.is_null()) {
    ContentGLContext::ContextLostReason reason = kUnknown;
    if (command_buffer_) {
      reason = ConvertReason(
          command_buffer_->GetLastState().context_lost_reason);
    }
    context_lost_callback_.Run(reason);
  }
}
