// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/texture_image_transport_surface.h"

#include <string>
#include <vector>

#include "base/command_line.h"
#include "content/common/gpu/gl_scoped_binders.h"
#include "content/common/gpu/gpu_channel.h"
#include "content/common/gpu/gpu_channel_manager.h"
#include "content/common/gpu/gpu_messages.h"
#include "content/common/gpu/sync_point_manager.h"
#include "content/public/common/content_switches.h"
#include "gpu/command_buffer/service/context_group.h"
#include "gpu/command_buffer/service/gpu_scheduler.h"

using gpu::gles2::ContextGroup;
using gpu::gles2::MailboxManager;
using gpu::gles2::MailboxName;
using gpu::gles2::TextureDefinition;
using gpu::gles2::TextureManager;

namespace content {

TextureImageTransportSurface::TextureImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle)
      : fbo_id_(0),
        backbuffer_(CreateTextureDefinition(gfx::Size(), 0)),
        stub_destroyed_(false),
        backbuffer_suggested_allocation_(true),
        frontbuffer_suggested_allocation_(true),
        handle_(handle),
        is_swap_buffers_pending_(false),
        did_unschedule_(false) {
  helper_.reset(new ImageTransportHelper(this,
                                         manager,
                                         stub,
                                         gfx::kNullPluginWindow));
}

TextureImageTransportSurface::~TextureImageTransportSurface() {
  DCHECK(stub_destroyed_);
  Destroy();
}

bool TextureImageTransportSurface::Initialize() {
  mailbox_manager_ =
      helper_->stub()->decoder()->GetContextGroup()->mailbox_manager();

  GpuChannelManager* manager = helper_->manager();
  surface_ = manager->GetDefaultOffscreenSurface();
  if (!surface_.get())
    return false;

  if (!helper_->Initialize())
    return false;

  GpuChannel* parent_channel = manager->LookupChannel(handle_.parent_client_id);
  if (parent_channel) {
    const CommandLine* command_line = CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess))
      helper_->SetPreemptByFlag(parent_channel->GetPreemptionFlag());
  }

  return true;
}

void TextureImageTransportSurface::Destroy() {
  if (surface_.get())
    surface_ = NULL;

  helper_->Destroy();
}

bool TextureImageTransportSurface::DeferDraws() {
  // The command buffer hit a draw/clear command that could clobber the
  // texture in use by the UI compositor. If a Swap is pending, abort
  // processing of the command by returning true and unschedule until the Swap
  // Ack arrives.
  DCHECK(!did_unschedule_);
  if (is_swap_buffers_pending_) {
    did_unschedule_ = true;
    helper_->SetScheduled(false);
    return true;
  }
  return false;
}

bool TextureImageTransportSurface::Resize(const gfx::Size&) {
  return true;
}

bool TextureImageTransportSurface::IsOffscreen() {
  return true;
}

bool TextureImageTransportSurface::OnMakeCurrent(gfx::GLContext* context) {
  if (stub_destroyed_) {
    // Early-exit so that we don't recreate the fbo. We still want to return
    // true, so that the context is made current and the GLES2DecoderImpl can
    // release its own resources.
    return true;
  }

  context_ = context;

  if (!fbo_id_) {
    glGenFramebuffersEXT(1, &fbo_id_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
    current_size_ = gfx::Size(1, 1);
    helper_->stub()->AddDestructionObserver(this);
  }

  // We could be receiving non-deferred GL commands, that is anything that does
  // not need a framebuffer.
  if (!backbuffer_->service_id() && !is_swap_buffers_pending_ &&
      backbuffer_suggested_allocation_) {
    CreateBackTexture();
  }
  return true;
}

unsigned int TextureImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

bool TextureImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  DCHECK(!is_swap_buffers_pending_);
  if (backbuffer_suggested_allocation_ == allocation)
     return true;
  backbuffer_suggested_allocation_ = allocation;

  if (backbuffer_suggested_allocation_) {
    DCHECK(!backbuffer_->service_id());
    CreateBackTexture();
  } else {
    ReleaseBackTexture();
  }

  return true;
}

void TextureImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;

  if (!frontbuffer_suggested_allocation_) {
    GpuHostMsg_AcceleratedSurfaceRelease_Params params;
    helper_->SendAcceleratedSurfaceRelease(params);
  }
}

void* TextureImageTransportSurface::GetShareHandle() {
  return GetHandle();
}

void* TextureImageTransportSurface::GetDisplay() {
  return surface_.get() ? surface_->GetDisplay() : NULL;
}

void* TextureImageTransportSurface::GetConfig() {
  return surface_.get() ? surface_->GetConfig() : NULL;
}

void TextureImageTransportSurface::OnResize(gfx::Size size) {
  current_size_ = size;
  CreateBackTexture();
}

void TextureImageTransportSurface::OnWillDestroyStub(
    GpuCommandBufferStub* stub) {
  DCHECK(stub == helper_->stub());
  stub->RemoveDestructionObserver(this);

  GpuHostMsg_AcceleratedSurfaceRelease_Params params;
  helper_->SendAcceleratedSurfaceRelease(params);

  ReleaseBackTexture();

  // We are losing the stub owning us, this is our last chance to clean up the
  // resources we allocated in the stub's context.
  if (fbo_id_) {
    glDeleteFramebuffersEXT(1, &fbo_id_);
    CHECK_GL_ERROR();
    fbo_id_ = 0;
  }

  stub_destroyed_ = true;
}

bool TextureImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;

  if (!backbuffer_->service_id()) {
    LOG(ERROR) << "Swap without valid backing.";
    return true;
  }

  DCHECK(backbuffer_size() == current_size_);
  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.size = backbuffer_size();
  params.mailbox_name.assign(
      reinterpret_cast<const char*>(&mailbox_name_), sizeof(mailbox_name_));

  glFlush();
  ProduceTexture();

  // Do not allow destruction while we are still waiting for a swap ACK,
  // so we do not leak a texture in the mailbox.
  AddRef();

  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool TextureImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_)
    return true;
  const gfx::Rect new_damage_rect(x, y, width, height);
  DCHECK(gfx::Rect(gfx::Point(), current_size_).Contains(new_damage_rect));

  // An empty damage rect is a successful no-op.
  if (new_damage_rect.IsEmpty())
    return true;

  if (!backbuffer_->service_id()) {
    LOG(ERROR) << "Swap without valid backing.";
    return true;
  }

  DCHECK(current_size_ == backbuffer_size());
  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_size = backbuffer_size();
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  params.mailbox_name.assign(
      reinterpret_cast<const char*>(&mailbox_name_), sizeof(mailbox_name_));

  glFlush();
  ProduceTexture();

  // Do not allow destruction while we are still waiting for a swap ACK,
  // so we do not leak a texture in the mailbox.
  AddRef();

  helper_->SendAcceleratedSurfacePostSubBuffer(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

std::string TextureImageTransportSurface::GetExtensions() {
  std::string extensions = gfx::GLSurface::GetExtensions();
  extensions += extensions.empty() ? "" : " ";
  extensions += "GL_CHROMIUM_front_buffer_cached ";
  extensions += "GL_CHROMIUM_post_sub_buffer";
  return extensions;
}

gfx::Size TextureImageTransportSurface::GetSize() {
  gfx::Size size = current_size_;

  // OSMesa expects a non-zero size.
  return gfx::Size(size.width() == 0 ? 1 : size.width(),
                   size.height() == 0 ? 1 : size.height());
}

void* TextureImageTransportSurface::GetHandle() {
  return surface_.get() ? surface_->GetHandle() : NULL;
}

unsigned TextureImageTransportSurface::GetFormat() {
  return surface_.get() ? surface_->GetFormat() : 0;
}

void TextureImageTransportSurface::OnBufferPresented(
    const AcceleratedSurfaceMsg_BufferPresented_Params& params) {
  if (params.sync_point == 0) {
    BufferPresentedImpl(params.mailbox_name);
  } else {
    helper_->manager()->sync_point_manager()->AddSyncPointCallback(
        params.sync_point,
        base::Bind(&TextureImageTransportSurface::BufferPresentedImpl,
                   this,
                   params.mailbox_name));
  }

  // Careful, we might get deleted now if we were only waiting for
  // a final swap ACK.
  Release();
}

void TextureImageTransportSurface::BufferPresentedImpl(
    const std::string& mailbox_name) {
  DCHECK(!backbuffer_->service_id());
  if (!mailbox_name.empty()) {
    DCHECK(mailbox_name.length() == GL_MAILBOX_SIZE_CHROMIUM);
    mailbox_name.copy(reinterpret_cast<char *>(&mailbox_name_),
                      sizeof(MailboxName));
    ConsumeTexture();
  }

  if (stub_destroyed_ && backbuffer_->service_id()) {
    // TODO(sievers): Remove this after changes to the mailbox to take ownership
    // of the service ids.
    DCHECK(context_.get() && surface_.get());
    uint32 service_id = backbuffer_->ReleaseServiceId();
    if (context_->MakeCurrent(surface_.get()))
      glDeleteTextures(1, &service_id);

    return;
  }

  DCHECK(is_swap_buffers_pending_);
  is_swap_buffers_pending_ = false;

  // We should not have allowed the backbuffer to be discarded while the ack
  // was pending.
  DCHECK(backbuffer_suggested_allocation_);

  // We're relying on the fact that the parent context is
  // finished with it's context when it inserts the sync point that
  // triggers this callback.
  if (helper_->MakeCurrent()) {
    if (backbuffer_size() != current_size_ || !backbuffer_->service_id())
      CreateBackTexture();
    else
      AttachBackTextureToFBO();
  }

  // Even if MakeCurrent fails, schedule anyway, to trigger the lost context
  // logic.
  if (did_unschedule_) {
    did_unschedule_ = false;
    helper_->SetScheduled(true);
  }
}

void TextureImageTransportSurface::OnResizeViewACK() {
  NOTREACHED();
}

void TextureImageTransportSurface::ReleaseBackTexture() {
  if (!backbuffer_->service_id())
    return;

  uint32 service_id = backbuffer_->ReleaseServiceId();
  glDeleteTextures(1, &service_id);
  backbuffer_.reset(CreateTextureDefinition(gfx::Size(), 0));
  mailbox_name_ = MailboxName();
  glFlush();
  CHECK_GL_ERROR();
}

void TextureImageTransportSurface::CreateBackTexture() {
  // If |is_swap_buffers_pending| we are waiting for our backbuffer
  // in the mailbox, so we shouldn't be reallocating it now.
  DCHECK(!is_swap_buffers_pending_);

  if (backbuffer_->service_id() && backbuffer_size() == current_size_)
    return;

  uint32 service_id = backbuffer_->ReleaseServiceId();

  VLOG(1) << "Allocating new backbuffer texture";

  // On Qualcomm we couldn't resize an FBO texture past a certain
  // size, after we allocated it as 1x1. So here we simply delete
  // the previous texture on resize, to insure we don't 'run out of
  // memory'.
  if (service_id &&
      helper_->stub()
             ->decoder()
             ->GetContextGroup()
             ->feature_info()
             ->workarounds()
             .delete_instead_of_resize_fbo) {
    glDeleteTextures(1, &service_id);
    service_id = 0;
    mailbox_name_ = MailboxName();
  }

  if (!service_id) {
    MailboxName new_mailbox_name;
    MailboxName& name = mailbox_name_;
    // This slot should be uninitialized.
    DCHECK(!memcmp(&name, &new_mailbox_name, sizeof(MailboxName)));
    mailbox_manager_->GenerateMailboxName(&new_mailbox_name);
    name = new_mailbox_name;
    glGenTextures(1, &service_id);
  }

  backbuffer_.reset(
      CreateTextureDefinition(current_size_, service_id));

  {
    ScopedTextureBinder texture_binder(service_id);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        current_size_.width(), current_size_.height(), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_GL_ERROR();
  }

  AttachBackTextureToFBO();
}

void TextureImageTransportSurface::AttachBackTextureToFBO() {
  DCHECK(backbuffer_->service_id());
  ScopedFrameBufferBinder fbo_binder(fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      backbuffer_->service_id(),
      0);
  CHECK_GL_ERROR();

#ifndef NDEBUG
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(FATAL) << "Framebuffer incomplete: " << status;
  }
#endif
}

TextureDefinition* TextureImageTransportSurface::CreateTextureDefinition(
    gfx::Size size, int service_id) {
  TextureDefinition::LevelInfo info(
      GL_TEXTURE_2D, GL_RGBA, size.width(), size.height(), 1,
      0, GL_RGBA, GL_UNSIGNED_BYTE, true);

  TextureDefinition::LevelInfos level_infos;
  level_infos.resize(1);
  level_infos[0].resize(1);
  level_infos[0][0] = info;
  return new TextureDefinition(
      GL_TEXTURE_2D,
      service_id,
      GL_LINEAR,
      GL_LINEAR,
      GL_CLAMP_TO_EDGE,
      GL_CLAMP_TO_EDGE,
      GL_NONE,
      true,
      level_infos);
}

void TextureImageTransportSurface::ConsumeTexture() {
  DCHECK(!backbuffer_->service_id());

  backbuffer_.reset(mailbox_manager_->ConsumeTexture(
      GL_TEXTURE_2D, mailbox_name_));
  if (!backbuffer_) {
    mailbox_name_ = MailboxName();
    backbuffer_.reset(CreateTextureDefinition(gfx::Size(), 0));
  }
}

void TextureImageTransportSurface::ProduceTexture() {
  DCHECK(backbuffer_->service_id());
  DCHECK(!backbuffer_size().IsEmpty());

  // Pass NULL as |owner| here to avoid errors from glConsumeTextureCHROMIUM()
  // when the renderer context group goes away before the RWHV handles a pending
  // ACK. We avoid leaking a texture in the mailbox by waiting for the final ACK
  // at which point we consume the correct texture back.
  bool success = mailbox_manager_->ProduceTexture(
      GL_TEXTURE_2D,
      mailbox_name_,
      backbuffer_.release(),
      NULL);
  DCHECK(success);
  mailbox_name_ = MailboxName();
  backbuffer_.reset(CreateTextureDefinition(gfx::Size(), 0));
}

}  // namespace content
