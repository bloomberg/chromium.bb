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
#include "gpu/command_buffer/service/texture_manager.h"

using gpu::gles2::ContextGroup;
using gpu::gles2::TextureManager;
typedef TextureManager::TextureInfo TextureInfo;

namespace content {

TextureImageTransportSurface::Texture::Texture()
    : client_id(0),
      sent_to_client(false) {
}

TextureImageTransportSurface::Texture::~Texture() {
}

TextureImageTransportSurface::TextureImageTransportSurface(
    GpuChannelManager* manager,
    GpuCommandBufferStub* stub,
    const gfx::GLSurfaceHandle& handle)
      : fbo_id_(0),
        front_(0),
        stub_destroyed_(false),
        backbuffer_suggested_allocation_(true),
        frontbuffer_suggested_allocation_(true),
        frontbuffer_is_protected_(true),
        protection_state_id_(0),
        handle_(handle),
        parent_stub_(NULL),
        is_swap_buffers_pending_(false),
        did_unschedule_(false),
        did_flip_(false) {
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
  GpuChannelManager* manager = helper_->manager();
  GpuChannel* parent_channel = manager->LookupChannel(handle_.parent_client_id);
  if (!parent_channel)
    return false;

  parent_stub_ = parent_channel->LookupCommandBuffer(handle_.parent_context_id);
  if (!parent_stub_)
    return false;

  parent_stub_->AddDestructionObserver(this);
  TextureManager* texture_manager =
      parent_stub_->decoder()->GetContextGroup()->texture_manager();
  DCHECK(texture_manager);

  for (int i = 0; i < 2; ++i) {
    Texture& texture = textures_[i];
    texture.client_id = handle_.parent_texture_id[i];
    texture.info = texture_manager->GetTextureInfo(texture.client_id);
    if (!texture.info)
      return false;

    if (!texture.info->target())
      texture_manager->SetInfoTarget(texture.info, GL_TEXTURE_2D);
    texture_manager->SetParameter(
        texture.info, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    texture_manager->SetParameter(
        texture.info, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    texture_manager->SetParameter(
        texture.info, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    texture_manager->SetParameter(
        texture.info, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  }

  surface_ = manager->GetDefaultOffscreenSurface();
  if (!surface_.get())
    return false;

  if (!helper_->Initialize())
    return false;

  const CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kUIPrioritizeInGpuProcess))
    helper_->SetPreemptByCounter(parent_channel->MessagesPendingCount());

  return true;
}

void TextureImageTransportSurface::Destroy() {
  if (parent_stub_) {
    parent_stub_->decoder()->MakeCurrent();
    ReleaseParentStub();
  }

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

  if (!fbo_id_) {
    glGenFramebuffersEXT(1, &fbo_id_);
    glBindFramebufferEXT(GL_FRAMEBUFFER, fbo_id_);
    CreateBackTexture(gfx::Size(1, 1));

#ifndef NDEBUG
    GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
      DLOG(ERROR) << "Framebuffer incomplete.";
      glDeleteFramebuffersEXT(1, &fbo_id_);
      fbo_id_ = 0;
      return false;
    }
#endif
    DCHECK(helper_->stub());
    helper_->stub()->AddDestructionObserver(this);
  }

  return true;
}

unsigned int TextureImageTransportSurface::GetBackingFrameBufferObject() {
  return fbo_id_;
}

void TextureImageTransportSurface::SetBackbufferAllocation(bool allocation) {
  DCHECK(!is_swap_buffers_pending_);
  if (backbuffer_suggested_allocation_ == allocation)
     return;
  backbuffer_suggested_allocation_ = allocation;

  if (!helper_->MakeCurrent())
    return;

  if (backbuffer_suggested_allocation_) {
    DCHECK(!textures_[back()].info->service_id() ||
           !textures_[back()].sent_to_client);
    CreateBackTexture(textures_[back()].size);
  } else {
    ReleaseTexture(back());
  }
}

void TextureImageTransportSurface::SetFrontbufferAllocation(bool allocation) {
  if (frontbuffer_suggested_allocation_ == allocation)
    return;
  frontbuffer_suggested_allocation_ = allocation;
  AdjustFrontBufferAllocation();
}

void TextureImageTransportSurface::AdjustFrontBufferAllocation() {
  if (!helper_->MakeCurrent())
    return;

  if (!frontbuffer_suggested_allocation_ && !frontbuffer_is_protected_ &&
      textures_[front()].info->service_id()) {
    ReleaseTexture(front());
    if (textures_[front()].sent_to_client) {
      GpuHostMsg_AcceleratedSurfaceRelease_Params params;
      params.identifier = textures_[front()].client_id;
      helper_->SendAcceleratedSurfaceRelease(params);
      textures_[front()].sent_to_client = false;
    }
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
  CreateBackTexture(size);
}

void TextureImageTransportSurface::OnWillDestroyStub(
    GpuCommandBufferStub* stub) {
  if (stub == parent_stub_) {
    ReleaseParentStub();
    helper_->SetPreemptByCounter(NULL);
  } else {
    DCHECK(stub == helper_->stub());
    stub->RemoveDestructionObserver(this);

    // We are losing the stub owning us, this is our last chance to clean up the
    // resources we allocated in the stub's context.
    if (fbo_id_) {
      glDeleteFramebuffersEXT(1, &fbo_id_);
      CHECK_GL_ERROR();
      fbo_id_ = 0;
    }

    stub_destroyed_ = true;
  }
}

bool TextureImageTransportSurface::SwapBuffers() {
  DCHECK(backbuffer_suggested_allocation_);
  if (!frontbuffer_suggested_allocation_ || !frontbuffer_is_protected_)
    return true;
  if (!parent_stub_) {
    LOG(ERROR) << "SwapBuffers failed because no parent stub.";
    return false;
  }

  glFlush();
  front_ = back();
  previous_damage_rect_ = gfx::Rect(textures_[front()].size);

  DCHECK(textures_[front()].client_id != 0);

  GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
  params.surface_handle = textures_[front()].client_id;
  params.size = textures_[front()].size;
  params.protection_state_id = protection_state_id_;
  params.skip_ack = false;
  helper_->SendAcceleratedSurfaceBuffersSwapped(params);

  DCHECK(!is_swap_buffers_pending_);
  is_swap_buffers_pending_ = true;
  return true;
}

bool TextureImageTransportSurface::PostSubBuffer(
    int x, int y, int width, int height) {
  DCHECK(backbuffer_suggested_allocation_);
  DCHECK(textures_[back()].info->service_id());
  if (!frontbuffer_suggested_allocation_ || !frontbuffer_is_protected_)
    return true;
  // If we are recreating the frontbuffer with this swap, make sure we are
  // drawing a full frame.
  DCHECK(textures_[front()].info->service_id() ||
         (!x && !y && gfx::Size(width, height) == textures_[back()].size));
  if (!parent_stub_) {
    LOG(ERROR) << "PostSubBuffer failed because no parent stub.";
    return false;
  }

  const gfx::Rect new_damage_rect(x, y, width, height);

  // An empty damage rect is a successful no-op.
  if (new_damage_rect.IsEmpty())
    return true;

  int back_texture_service_id = textures_[back()].info->service_id();
  int front_texture_service_id = textures_[front()].info->service_id();

  gfx::Size expected_size = textures_[back()].size;
  bool surfaces_same_size = textures_[front()].size == expected_size;

  if (surfaces_same_size) {
    std::vector<gfx::Rect> regions_to_copy;
    GetRegionsToCopy(previous_damage_rect_, new_damage_rect, &regions_to_copy);

    ScopedFrameBufferBinder fbo_binder(fbo_id_);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        front_texture_service_id,
        0);
    ScopedTextureBinder texture_binder(back_texture_service_id);

    for (size_t i = 0; i < regions_to_copy.size(); ++i) {
      const gfx::Rect& region_to_copy = regions_to_copy[i];
      if (!region_to_copy.IsEmpty()) {
        glCopyTexSubImage2D(GL_TEXTURE_2D, 0, region_to_copy.x(),
            region_to_copy.y(), region_to_copy.x(), region_to_copy.y(),
            region_to_copy.width(), region_to_copy.height());
      }
    }
  } else if (!surfaces_same_size && did_flip_) {
    DCHECK(new_damage_rect == gfx::Rect(expected_size));
  }

  glFlush();
  front_ = back();
  previous_damage_rect_ = new_damage_rect;

  DCHECK(textures_[front()].client_id);

  GpuHostMsg_AcceleratedSurfacePostSubBuffer_Params params;
  params.surface_handle = textures_[front()].client_id;
  params.surface_size = textures_[front()].size;
  params.x = x;
  params.y = y;
  params.width = width;
  params.height = height;
  params.protection_state_id = protection_state_id_;
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
  gfx::Size size = textures_[back()].size;

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

void TextureImageTransportSurface::OnSetFrontSurfaceIsProtected(
    bool is_protected, uint32 protection_state_id) {
  protection_state_id_ = protection_state_id;
  if (frontbuffer_is_protected_ == is_protected)
    return;
  frontbuffer_is_protected_ = is_protected;
  AdjustFrontBufferAllocation();

  // If surface is set to protected, and we haven't actually released it yet,
  // we can set the ui surface handle now just by sending a swap message.
  if (is_protected && textures_[front()].info->service_id() &&
      textures_[front()].sent_to_client) {
    GpuHostMsg_AcceleratedSurfaceBuffersSwapped_Params params;
    params.surface_handle = textures_[front()].client_id;
    params.size = textures_[front()].size;
    params.protection_state_id = protection_state_id_;
    params.skip_ack = true;
    helper_->SendAcceleratedSurfaceBuffersSwapped(params);
  }
}

void TextureImageTransportSurface::OnBufferPresented(bool presented,
                                                     uint32 sync_point) {
  if (sync_point == 0) {
    BufferPresentedImpl(presented);
  } else {
    helper_->manager()->sync_point_manager()->AddSyncPointCallback(
        sync_point,
        base::Bind(&TextureImageTransportSurface::BufferPresentedImpl,
                   this->AsWeakPtr(),
                   presented));
  }
}

void TextureImageTransportSurface::BufferPresentedImpl(bool presented) {
  DCHECK(is_swap_buffers_pending_);
  is_swap_buffers_pending_ = false;

  if (presented) {
    // If we had not flipped, the two frame damage tracking is inconsistent.
    // So conservatively take the whole frame.
    if (!did_flip_)
      previous_damage_rect_ = gfx::Rect(textures_[front()].size);
  } else {
    front_ = back();
    previous_damage_rect_ = gfx::Rect(0, 0, 0, 0);
  }

  did_flip_ = presented;

  // We're relying on the fact that the parent context is
  // finished with it's context when it inserts the sync point that
  // triggers this callback.
  if (helper_->MakeCurrent()) {
    if (textures_[front()].size != textures_[back()].size ||
        !textures_[back()].info->service_id() ||
        !textures_[back()].sent_to_client) {
      // We may get an ACK from a stale swap just to reschedule.  In that case,
      // we may not have a backbuffer suggestion and should not recreate one.
      if (backbuffer_suggested_allocation_)
        CreateBackTexture(textures_[front()].size);
    } else {
      AttachBackTextureToFBO();
    }
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

void TextureImageTransportSurface::ReleaseTexture(int id) {
  if (!parent_stub_)
    return;
  Texture& texture = textures_[id];
  TextureInfo* info = texture.info;
  DCHECK(info);

  GLuint service_id = info->service_id();
  if (!service_id)
    return;
  info->SetServiceId(0);

  {
    ScopedFrameBufferBinder fbo_binder(fbo_id_);
    glDeleteTextures(1, &service_id);
  }
  glFlush();
  CHECK_GL_ERROR();
}

void TextureImageTransportSurface::CreateBackTexture(const gfx::Size& size) {
  if (!parent_stub_)
    return;
  Texture& texture = textures_[back()];
  TextureInfo* info = texture.info;
  DCHECK(info);

  GLuint service_id = info->service_id();

  if (service_id && texture.size == size && texture.sent_to_client)
    return;

  if (!service_id) {
    glGenTextures(1, &service_id);
    info->SetServiceId(service_id);
  }

  if (size != texture.size) {
    texture.size = size;
    TextureManager* texture_manager =
        parent_stub_->decoder()->GetContextGroup()->texture_manager();
    texture_manager->SetLevelInfo(
        info,
        GL_TEXTURE_2D,
        0,
        GL_RGBA,
        size.width(),
        size.height(),
        1,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        true);
  }

  {
    ScopedTextureBinder texture_binder(service_id);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
        size.width(), size.height(), 0,
        GL_RGBA, GL_UNSIGNED_BYTE, NULL);
    CHECK_GL_ERROR();
  }

  AttachBackTextureToFBO();

  GpuHostMsg_AcceleratedSurfaceNew_Params params;
  params.width = size.width();
  params.height = size.height();
  params.surface_handle = texture.client_id;
  helper_->SendAcceleratedSurfaceNew(params);
  texture.sent_to_client = true;
}

void TextureImageTransportSurface::AttachBackTextureToFBO() {
  if (!parent_stub_)
    return;
  TextureInfo* info = textures_[back()].info;
  DCHECK(info);

  ScopedFrameBufferBinder fbo_binder(fbo_id_);
  glFramebufferTexture2DEXT(GL_FRAMEBUFFER,
      GL_COLOR_ATTACHMENT0,
      GL_TEXTURE_2D,
      info->service_id(),
      0);
  glFlush();
  CHECK_GL_ERROR();

#ifndef NDEBUG
  GLenum status = glCheckFramebufferStatusEXT(GL_FRAMEBUFFER);
  if (status != GL_FRAMEBUFFER_COMPLETE) {
    DLOG(ERROR) << "Framebuffer incomplete.";
  }
#endif
}

void TextureImageTransportSurface::ReleaseParentStub() {
  DCHECK(parent_stub_);
  parent_stub_->RemoveDestructionObserver(this);
  for (int i = 0; i < 2; ++i) {
    Texture& texture = textures_[i];
    texture.info = NULL;
    if (!texture.sent_to_client)
      continue;
    GpuHostMsg_AcceleratedSurfaceRelease_Params params;
    params.identifier = texture.client_id;
    helper_->SendAcceleratedSurfaceRelease(params);
  }
  parent_stub_ = NULL;
}

}  // namespace content
