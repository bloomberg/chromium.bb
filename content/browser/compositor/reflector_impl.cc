// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
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
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::InitOnImplThread, this));
}

void ReflectorImpl::InitOnImplThread() {
  // Ignore if the reflector was shutdown before
  // initialized, or it's already initialized.
  if (!output_surface_map_ || gl_helper_.get())
    return;

  BrowserCompositorOutputSurface* source_surface =
      output_surface_map_->Lookup(surface_id_);
  // Skip if the source surface isn't ready yet. This will be
  // initiailze when the source surface becomes ready.
  if (!source_surface)
    return;

  AttachToOutputSurfaceOnImplThread(source_surface);
  // The shared texture doesn't have the data, so invokes full redraw
  // now.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::FullRedrawContentOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

void ReflectorImpl::OnSourceSurfaceReady(int surface_id) {
  DCHECK_EQ(surface_id_, surface_id);
  InitOnImplThread();
}

void ReflectorImpl::Shutdown() {
  mirroring_compositor_ = NULL;
  mirroring_layer_ = NULL;
  {
    base::AutoLock lock(texture_lock_);
    texture_id_ = 0;
  }
  shared_texture_ = NULL;
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::ShutdownOnImplThread, this));
}

void ReflectorImpl::ShutdownOnImplThread() {
  BrowserCompositorOutputSurface* output_surface =
      output_surface_map_->Lookup(surface_id_);
  if (output_surface)
    output_surface->SetReflector(NULL);
  output_surface_map_ = NULL;
  gl_helper_.reset();
  // The instance must be deleted on main thread.
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::DeleteOnMainThread,
                 scoped_refptr<ReflectorImpl>(this)));
}

void ReflectorImpl::ReattachToOutputSurfaceFromMainThread(
    BrowserCompositorOutputSurface* output_surface) {
  impl_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::AttachToOutputSurfaceOnImplThread,
                 this, output_surface));
}

void ReflectorImpl::OnMirroringCompositorResized() {
  mirroring_compositor_->ScheduleFullRedraw();
}

void ReflectorImpl::OnLostResources() {
  mirroring_layer_->SetShowPaintedContent();
}

void ReflectorImpl::OnReshape(gfx::Size size) {
  if (texture_size_ == size)
    return;
  texture_size_ = size;
  {
    base::AutoLock lock(texture_lock_);
    if (texture_id_) {
      gl_helper_->ResizeTexture(texture_id_, size);
      gl_helper_->Flush();
    }
  }
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::UpdateTextureSizeOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_));
}

void ReflectorImpl::OnSwapBuffers() {
  {
    base::AutoLock lock(texture_lock_);
    if (texture_id_) {
      gl_helper_->CopyTextureFullImage(texture_id_, texture_size_);
      gl_helper_->Flush();
    }
  }
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::FullRedrawOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_));
}

void ReflectorImpl::OnPostSubBuffer(gfx::Rect rect) {
  {
    base::AutoLock lock(texture_lock_);
    if (texture_id_) {
      gl_helper_->CopyTextureSubImage(texture_id_, rect);
      gl_helper_->Flush();
    }
  }
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::UpdateSubBufferOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_,
                 rect));
}

void ReflectorImpl::CreateSharedTextureOnMainThread(gfx::Size size) {
  {
    base::AutoLock lock(texture_lock_);
    texture_id_ =
        ImageTransportFactory::GetInstance()->GetGLHelper()->CreateTexture();
    shared_texture_ =
        ImageTransportFactory::GetInstance()->CreateOwnedTexture(
            size, 1.0f, texture_id_);
    ImageTransportFactory::GetInstance()->GetGLHelper()->Flush();
  }
  mirroring_layer_->SetExternalTexture(shared_texture_.get());
  FullRedrawOnMainThread(size);
}

ReflectorImpl::~ReflectorImpl() {
  // Make sure the reflector is deleted on main thread.
  DCHECK_EQ(main_message_loop_.get(),
            base::MessageLoopProxy::current().get());
}

void ReflectorImpl::AttachToOutputSurfaceOnImplThread(
    BrowserCompositorOutputSurface* output_surface) {
  output_surface->context_provider()->BindToCurrentThread();
  gl_helper_.reset(
      new GLHelper(output_surface->context_provider()->ContextGL(),
                   output_surface->context_provider()->ContextSupport()));
  output_surface->SetReflector(this);
  main_message_loop_->PostTask(
      FROM_HERE,
      base::Bind(&ReflectorImpl::CreateSharedTextureOnMainThread,
                 this->AsWeakPtr(),
                 texture_size_));
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
