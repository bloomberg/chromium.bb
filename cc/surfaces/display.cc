// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/display.h"

#include "base/debug/trace_event.h"
#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/compositor_frame_ack.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/output/software_renderer.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"
#include "cc/surfaces/surface_aggregator.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

Display::Display(DisplayClient* client,
                 SurfaceManager* manager,
                 SharedBitmapManager* bitmap_manager)
    : client_(client), manager_(manager), bitmap_manager_(bitmap_manager) {
  manager_->AddObserver(this);
}

Display::~Display() {
  manager_->RemoveObserver(this);
}

void Display::Resize(SurfaceId id, const gfx::Size& size) {
  current_surface_id_ = id;
  current_surface_size_ = size;
  client_->DisplayDamaged();
}

void Display::InitializeOutputSurface() {
  if (output_surface_)
    return;
  scoped_ptr<OutputSurface> output_surface = client_->CreateOutputSurface();
  if (!output_surface->BindToClient(this))
    return;

  int highp_threshold_min = 0;
  bool use_rgba_4444_texture_format = false;
  size_t id_allocation_chunk_size = 1;
  bool use_distance_field_text = false;
  scoped_ptr<ResourceProvider> resource_provider =
      ResourceProvider::Create(output_surface.get(),
                               bitmap_manager_,
                               highp_threshold_min,
                               use_rgba_4444_texture_format,
                               id_allocation_chunk_size,
                               use_distance_field_text);
  if (!resource_provider)
    return;

  if (output_surface->context_provider()) {
    TextureMailboxDeleter* texture_mailbox_deleter = NULL;
    scoped_ptr<GLRenderer> renderer =
        GLRenderer::Create(this,
                           &settings_,
                           output_surface.get(),
                           resource_provider.get(),
                           texture_mailbox_deleter,
                           highp_threshold_min);
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  } else {
    scoped_ptr<SoftwareRenderer> renderer = SoftwareRenderer::Create(
        this, &settings_, output_surface.get(), resource_provider.get());
    if (!renderer)
      return;
    renderer_ = renderer.Pass();
  }

  output_surface_ = output_surface.Pass();
  resource_provider_ = resource_provider.Pass();
  aggregator_.reset(new SurfaceAggregator(manager_, resource_provider_.get()));
}

bool Display::Draw() {
  if (current_surface_id_.is_null())
    return false;

  InitializeOutputSurface();
  if (!output_surface_)
    return false;

  contained_surfaces_.clear();

  scoped_ptr<CompositorFrame> frame =
      aggregator_->Aggregate(current_surface_id_, &contained_surfaces_);
  if (!frame)
    return false;

  TRACE_EVENT0("cc", "Display::Draw");
  DelegatedFrameData* frame_data = frame->delegated_frame_data.get();

  // Only reshape when we know we are going to draw. Otherwise, the reshape
  // can leave the window at the wrong size if we never draw and the proper
  // viewport size is never set.
  output_surface_->Reshape(current_surface_size_, 1.f);
  float device_scale_factor = 1.0f;
  gfx::Rect device_viewport_rect = gfx::Rect(current_surface_size_);
  gfx::Rect device_clip_rect = device_viewport_rect;
  bool disable_picture_quad_image_filtering = false;

  renderer_->DecideRenderPassAllocationsForFrame(frame_data->render_pass_list);
  renderer_->DrawFrame(&frame_data->render_pass_list,
                       device_scale_factor,
                       device_viewport_rect,
                       device_clip_rect,
                       disable_picture_quad_image_filtering);
  CompositorFrameMetadata metadata;
  renderer_->SwapBuffers(metadata);
  return true;
}

void Display::OnSurfaceDamaged(SurfaceId surface) {
  if (contained_surfaces_.find(surface) != contained_surfaces_.end())
    client_->DisplayDamaged();
}

SurfaceId Display::CurrentSurfaceId() {
  return current_surface_id_;
}

}  // namespace cc
