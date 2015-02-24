// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/reflector_impl.h"

#include "base/bind.h"
#include "base/location.h"
#include "content/browser/compositor/browser_compositor_output_surface.h"
#include "content/browser/compositor/owned_mailbox.h"
#include "content/common/gpu/client/gl_helper.h"
#include "ui/compositor/layer.h"

namespace content {

ReflectorImpl::ReflectorImpl(ui::Compositor* mirrored_compositor,
                             ui::Layer* mirroring_layer)
    : mirrored_compositor_(mirrored_compositor),
      mirroring_layer_(mirroring_layer),
      mirrored_compositor_gl_helper_texture_id_(0),
      needs_set_mailbox_(false),
      flip_texture_(false),
      output_surface_(nullptr) {
}

ReflectorImpl::~ReflectorImpl() {
}

void ReflectorImpl::Shutdown() {
  if (output_surface_)
    DetachFromOutputSurface();
  // Prevent the ReflectorImpl from picking up a new output surface.
  mirroring_layer_ = nullptr;
}

void ReflectorImpl::DetachFromOutputSurface() {
  DCHECK(output_surface_);
  output_surface_->SetReflector(nullptr);
  DCHECK(mailbox_.get());
  mailbox_ = nullptr;
  output_surface_ = nullptr;
  mirrored_compositor_gl_helper_->DeleteTexture(
      mirrored_compositor_gl_helper_texture_id_);
  mirrored_compositor_gl_helper_texture_id_ = 0;
  mirrored_compositor_gl_helper_ = nullptr;
  mirroring_layer_->SetShowSolidColorContent();
}

void ReflectorImpl::OnSourceSurfaceReady(
    BrowserCompositorOutputSurface* output_surface) {
  if (!mirroring_layer_)
    return;  // Was already Shutdown().
  if (output_surface == output_surface_)
    return;  // Is already attached.
  if (output_surface_)
    DetachFromOutputSurface();

  // Use the GLHelper from the ImageTransportFactory for our OwnedMailbox so we
  // don't have to manage the lifetime of the GLHelper relative to the lifetime
  // of the mailbox.
  GLHelper* shared_helper = ImageTransportFactory::GetInstance()->GetGLHelper();
  mailbox_ = new OwnedMailbox(shared_helper);
  needs_set_mailbox_ = true;

  // Create a GLHelper attached to the mirrored compositor's output surface for
  // copying the output of the mirrored compositor.
  mirrored_compositor_gl_helper_.reset(
      new GLHelper(output_surface->context_provider()->ContextGL(),
                   output_surface->context_provider()->ContextSupport()));
  // Create a texture id in the name space of the new GLHelper to update the
  // mailbox being held by the |mirroring_layer_|.
  mirrored_compositor_gl_helper_texture_id_ =
      mirrored_compositor_gl_helper_->ConsumeMailboxToTexture(
          mailbox_->mailbox(), mailbox_->sync_point());

  flip_texture_ = !output_surface->capabilities().flipped_output_surface;

  // The texture doesn't have the data. Request full redraw on mirrored
  // compositor so that the full content will be copied to mirroring compositor.
  // This full redraw should land us in OnSourceSwapBuffers() to resize the
  // texture appropriately.
  mirrored_compositor_->ScheduleFullRedraw();

  output_surface_ = output_surface;
  output_surface_->SetReflector(this);
}

void ReflectorImpl::OnMirroringCompositorResized() {
  mirroring_layer_->SchedulePaint(mirroring_layer_->bounds());
}

void ReflectorImpl::OnSourceSwapBuffers() {
  if (!mirroring_layer_)
    return;
  // Should be attached to the source output surface already.
  DCHECK(mailbox_.get());

  gfx::Size size = output_surface_->SurfaceSize();
  mirrored_compositor_gl_helper_->CopyTextureFullImage(
      mirrored_compositor_gl_helper_texture_id_, size);
  // Insert a barrier to make the copy show up in the mirroring compositor's
  // mailbox. Since the the compositor contexts and the ImageTransportFactory's
  // GLHelper are all on the same GPU channel, this is sufficient instead of
  // plumbing through a sync point.
  mirrored_compositor_gl_helper_->InsertOrderingBarrier();

  // Request full redraw on mirroring compositor.
  UpdateTexture(size, mirroring_layer_->bounds());
}

void ReflectorImpl::OnSourcePostSubBuffer(const gfx::Rect& rect) {
  if (!mirroring_layer_)
    return;
  // Should be attached to the source output surface already.
  DCHECK(mailbox_.get());

  gfx::Size size = output_surface_->SurfaceSize();
  mirrored_compositor_gl_helper_->CopyTextureSubImage(
      mirrored_compositor_gl_helper_texture_id_, rect);
  // Insert a barrier to make the copy show up in the mirroring compositor's
  // mailbox. Since the the compositor contexts and the ImageTransportFactory's
  // GLHelper are all on the same GPU channel, this is sufficient instead of
  // plumbing through a sync point.
  mirrored_compositor_gl_helper_->InsertOrderingBarrier();

  int y = rect.y();
  // Flip the coordinates to compositor's one.
  if (flip_texture_)
    y = size.height() - rect.y() - rect.height();
  gfx::Rect mirroring_rect(rect.x(), y, rect.width(), rect.height());

  // Request redraw of the dirty portion in mirroring compositor.
  UpdateTexture(size, mirroring_rect);
}

static void ReleaseMailbox(scoped_refptr<OwnedMailbox> mailbox,
                           unsigned int sync_point,
                           bool is_lost) {
  mailbox->UpdateSyncPoint(sync_point);
}

void ReflectorImpl::UpdateTexture(const gfx::Size& source_size,
                                  const gfx::Rect& redraw_rect) {
  if (needs_set_mailbox_) {
    mirroring_layer_->SetTextureMailbox(
        cc::TextureMailbox(mailbox_->holder()),
        cc::SingleReleaseCallback::Create(base::Bind(ReleaseMailbox, mailbox_)),
        source_size);
    needs_set_mailbox_ = false;
  } else {
    mirroring_layer_->SetTextureSize(source_size);
  }
  mirroring_layer_->SetBounds(gfx::Rect(source_size));
  mirroring_layer_->SetTextureFlipped(flip_texture_);
  mirroring_layer_->SchedulePaint(redraw_rect);
}

}  // namespace content
