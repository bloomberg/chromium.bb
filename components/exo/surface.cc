// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface.h"

#include "base/callback_helpers.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "cc/resources/single_release_callback.h"
#include "components/exo/buffer.h"
#include "components/exo/surface_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/buffer_format_util.h"
#include "ui/gfx/gpu_memory_buffer.h"

namespace exo {

////////////////////////////////////////////////////////////////////////////////
// Surface, public:

Surface::Surface() : compositor_(nullptr), delegate_(nullptr) {
  SetLayer(new ui::Layer(ui::LAYER_SOLID_COLOR));
  set_owned_by_client();
}

Surface::~Surface() {
  if (delegate_)
    delegate_->OnSurfaceDestroying();

  layer()->SetShowSolidColorContent();

  if (compositor_)
    compositor_->RemoveObserver(this);

  // Call pending frame callbacks with a null frame time to indicate that they
  // have been cancelled.
  frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 frame_callbacks_);
  for (const auto& frame_callback : active_frame_callbacks_)
    frame_callback.Run(base::TimeTicks());
}

void Surface::Attach(Buffer* buffer) {
  TRACE_EVENT1("exo", "Surface::Attach", "buffer", buffer->AsTracedValue());

  pending_buffer_ = buffer ? buffer->AsWeakPtr() : base::WeakPtr<Buffer>();
  PreferredSizeChanged();
}

void Surface::Damage(const gfx::Rect& damage) {
  TRACE_EVENT1("exo", "Surface::Damage", "damage", damage.ToString());

  pending_damage_.Union(damage);
}

void Surface::RequestFrameCallback(const FrameCallback& callback) {
  TRACE_EVENT0("exo", "Surface::RequestFrameCallback");

  pending_frame_callbacks_.push_back(callback);
}

void Surface::SetOpaqueRegion(const SkRegion& region) {
  TRACE_EVENT1("exo", "Surface::SetOpaqueRegion", "region",
               gfx::SkIRectToRect(region.getBounds()).ToString());

  pending_opaque_region_ = region;
}

void Surface::Commit() {
  TRACE_EVENT0("exo", "Surface::Commit");

  if (delegate_)
    delegate_->OnSurfaceCommit();

  cc::TextureMailbox texture_mailbox;
  scoped_ptr<cc::SingleReleaseCallback> texture_mailbox_release_callback;
  if (pending_buffer_) {
    texture_mailbox_release_callback =
        pending_buffer_->AcquireTextureMailbox(&texture_mailbox);
    pending_buffer_.reset();
  } else {
    // Show solid color content if there is no pending buffer.
    layer()->SetShowSolidColorContent();
  }

  if (texture_mailbox_release_callback) {
    // Update layer with the new contents.
    layer()->SetTextureMailbox(texture_mailbox,
                               texture_mailbox_release_callback.Pass(),
                               texture_mailbox.size_in_pixels());
    layer()->SetTextureFlipped(false);
    layer()->SetBounds(gfx::Rect(layer()->bounds().origin(),
                                 texture_mailbox.size_in_pixels()));
    layer()->SetFillsBoundsOpaquely(pending_opaque_region_.contains(
        gfx::RectToSkIRect(gfx::Rect(texture_mailbox.size_in_pixels()))));
  }

  // Schedule redraw of the damage region.
  layer()->SchedulePaint(pending_damage_);
  pending_damage_ = gfx::Rect();

  ui::Compositor* compositor = layer()->GetCompositor();
  if (compositor && !pending_frame_callbacks_.empty()) {
    // Start observing the compositor for frame callbacks.
    if (!compositor_) {
      compositor->AddObserver(this);
      compositor_ = compositor;
    }

    // Move pending frame callbacks to the end of |frame_callbacks_|.
    frame_callbacks_.splice(frame_callbacks_.end(), pending_frame_callbacks_);
  }
}

void Surface::SetSurfaceDelegate(SurfaceDelegate* delegate) {
  DCHECK(!delegate_ || !delegate);
  delegate_ = delegate;
}

scoped_refptr<base::trace_event::TracedValue> Surface::AsTracedValue() const {
  scoped_refptr<base::trace_event::TracedValue> value =
      new base::trace_event::TracedValue;
  value->SetString("name", layer()->name());
  return value;
}

////////////////////////////////////////////////////////////////////////////////
// views::Views overrides:

gfx::Size Surface::GetPreferredSize() const {
  return pending_buffer_ ? pending_buffer_->GetSize() : layer()->size();
}

////////////////////////////////////////////////////////////////////////////////
// ui::CompositorObserver overrides:

void Surface::OnCompositingDidCommit(ui::Compositor* compositor) {
  // Move frame callbacks to the end of |active_frame_callbacks_|.
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 frame_callbacks_);
}

void Surface::OnCompositingStarted(ui::Compositor* compositor,
                                   base::TimeTicks start_time) {
  // Run all frame callbacks associated with the compositor's active tree.
  while (!active_frame_callbacks_.empty()) {
    active_frame_callbacks_.front().Run(start_time);
    active_frame_callbacks_.pop_front();
  }
}

void Surface::OnCompositingShuttingDown(ui::Compositor* compositor) {
  compositor->RemoveObserver(this);
  compositor_ = nullptr;
}

}  // namespace exo
