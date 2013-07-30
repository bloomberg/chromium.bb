// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/reflector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/browser/aura/browser_compositor_output_surface.h"
#include "content/common/gpu/client/gl_helper.h"
#include "ui/compositor/layer.h"

namespace content {

ReflectorImpl::ReflectorImpl(
    ui::Compositor* mirrored_compositor,
    ui::Layer* mirroring_layer,
    IDMap<BrowserCompositorOutputSurface>* output_surface_map,
    int surface_id)
    : texture_id_(0),
      texture_size_(mirrored_compositor->size()),
      output_surface_map_(output_surface_map),
      mirrored_compositor_(mirrored_compositor),
      mirroring_compositor_(mirroring_layer->GetCompositor()),
      mirroring_layer_(mirroring_layer),
      impl_message_loop_(ui::Compositor::GetCompositorMessageLoop()),
      main_message_loop_(base::MessageLoopProxy::current()),
      surface_id_(surface_id) {
  CreateSharedTexture();
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::InitOnImplThread, this));
}

void ReflectorImpl::InitOnImplThread() {
  AttachToOutputSurface(output_surface_map_->Lookup(surface_id_));
  gl_helper_->CopyTextureFullImage(texture_id_, texture_size_);
  // The shared texture doesn't have the data, so invokes full redraw
  // now.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::FullRedrawContentOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

void ReflectorImpl::Shutdown() {
  mirroring_compositor_ = NULL;
  mirroring_layer_ = NULL;
  shared_texture_ = NULL;
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::ShutdownOnImplThread, this));
}

void ReflectorImpl::ShutdownOnImplThread() {
  BrowserCompositorOutputSurface* output_surface =
      output_surface_map_->Lookup(surface_id_);
  output_surface->SetReflector(NULL);
  gl_helper_.reset();
  // The instance must be deleted on main thread.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::DeleteOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

// This must be called on ImplThread, or before the surface is passed to
// ImplThread.
void ReflectorImpl::AttachToOutputSurface(
    BrowserCompositorOutputSurface* output_surface) {
  gl_helper_.reset(new GLHelper(output_surface->context3d()));
  output_surface->SetReflector(this);
}

void ReflectorImpl::OnMirroringCompositorResized() {
  mirroring_compositor_->ScheduleFullRedraw();
}

void ReflectorImpl::OnLostResources() {
  shared_texture_ = NULL;
  mirroring_layer_->SetExternalTexture(NULL);
}

void ReflectorImpl::OnReshape(gfx::Size size) {
  if (texture_size_ == size)
    return;
  texture_size_ = size;
  DCHECK(texture_id_);
  gl_helper_->ResizeTexture(texture_id_, size);
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::UpdateTextureSizeOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_));
}

void ReflectorImpl::OnSwapBuffers() {
  DCHECK(texture_id_);
  gl_helper_->CopyTextureFullImage(texture_id_, texture_size_);
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::FullRedrawOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_));
}

void ReflectorImpl::OnPostSubBuffer(gfx::Rect rect) {
  DCHECK(texture_id_);
  gl_helper_->CopyTextureSubImage(texture_id_, rect);
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::UpdateSubBufferOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_,
                 rect));
}

void ReflectorImpl::CreateSharedTexture() {
  texture_id_ =
      ImageTransportFactory::GetInstance()->GetGLHelper()->CreateTexture();
  shared_texture_ =
      ImageTransportFactory::GetInstance()->CreateOwnedTexture(
          texture_size_, 1.0f, texture_id_);
  mirroring_layer_->SetExternalTexture(shared_texture_.get());
}

ReflectorImpl::~ReflectorImpl() {
  // Make sure the reflector is deleted on main thread.
  DCHECK_EQ(main_message_loop_.get(),
            base::MessageLoopProxy::current().get());
}

void ReflectorImpl::UpdateTextureSizeOnMainThread(gfx::Size size) {
  if (!mirroring_layer_)
    return;
  mirroring_layer_->SetBounds(gfx::Rect(size));
}

void ReflectorImpl::FullRedrawOnMainThread(gfx::Size size) {
  if (!mirroring_compositor_)
    return;
  UpdateTextureSizeOnMainThread(size);
  mirroring_compositor_->ScheduleFullRedraw();
}

void ReflectorImpl::UpdateSubBufferOnMainThread(gfx::Size size,
                                                gfx::Rect rect) {
  if (!mirroring_compositor_)
    return;
  UpdateTextureSizeOnMainThread(size);
  // Flip the coordinates to compositor's one.
  int y = size.height() - rect.y() - rect.height();
  gfx::Rect new_rect(rect.x(), y, rect.width(), rect.height());
  mirroring_layer_->SchedulePaint(new_rect);
}

void ReflectorImpl::FullRedrawContentOnMainThread() {
  mirrored_compositor_->ScheduleFullRedraw();
}

}  // namespace content
