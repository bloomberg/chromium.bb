// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SURFACES_DISPLAY_H_
#define CC_SURFACES_DISPLAY_H_

#include "cc/surfaces/display.h"

#include "base/message_loop/message_loop.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/direct_renderer.h"
#include "cc/output/gl_renderer.h"
#include "cc/surfaces/display_client.h"
#include "cc/surfaces/surface.h"
#include "ui/gfx/frame_time.h"

namespace cc {

Display::Display(DisplayClient* client, SurfaceManager* manager)
    : scheduled_draw_(false),
      client_(client),
      manager_(manager),
      aggregator_(manager) {
}

Display::~Display() {
}

void Display::Resize(const gfx::Size& size) {
  current_surface_.reset(new Surface(manager_, this, size));
}

bool Display::Draw() {
  if (!current_surface_)
    return false;

  // TODO(jamesr): Use the surface aggregator instead.
  scoped_ptr<DelegatedFrameData> frame_data(new DelegatedFrameData);
  CompositorFrame* current_frame = current_surface_->GetEligibleFrame();
  frame_data->resource_list =
      current_frame->delegated_frame_data->resource_list;
  RenderPass::CopyAll(current_frame->delegated_frame_data->render_pass_list,
                      &frame_data->render_pass_list);

  if (!layer_tree_host_) {
    // TODO(jbauman): Switch to use ResourceProvider and GLRenderer directly,
    // as using LayerTreeHost from here is a layering violation.
    LayerTreeSettings settings;
    layer_tree_host_ =
        LayerTreeHost::CreateSingleThreaded(this, this, NULL, settings);
    resource_collection_ = new DelegatedFrameResourceCollection;
    resource_collection_->SetClient(this);
    layer_tree_host_->SetLayerTreeHostClientReady();
  }
  if (!delegated_frame_provider_ ||
      delegated_frame_provider_->frame_size() !=
          frame_data->render_pass_list.back()->output_rect.size()) {
    delegated_frame_provider_ =
        new DelegatedFrameProvider(resource_collection_, frame_data.Pass());
    delegated_layer_ =
        DelegatedRendererLayer::Create(delegated_frame_provider_);

    layer_tree_host_->SetRootLayer(delegated_layer_);
    delegated_layer_->SetDisplaySize(current_surface_->size());
    delegated_layer_->SetBounds(current_surface_->size());
    delegated_layer_->SetContentsOpaque(true);
    delegated_layer_->SetIsDrawable(true);
  } else {
    delegated_frame_provider_->SetFrameData(frame_data.Pass());
  }
  layer_tree_host_->SetViewportSize(current_surface_->size());

  return true;
}

scoped_ptr<OutputSurface> Display::CreateOutputSurface(bool fallback) {
  return client_->CreateOutputSurface();
}

void Display::ScheduleComposite() {
  if (scheduled_draw_)
    return;

  scheduled_draw_ = true;

  base::MessageLoop::current()->PostTask(
      FROM_HERE, base::Bind(&Display::DoComposite, base::Unretained(this)));
}

void Display::ScheduleAnimation() {
  ScheduleComposite();
}

void Display::DoComposite() {
  scheduled_draw_ = false;
  layer_tree_host_->Composite(gfx::FrameTime::Now());
}

int Display::CurrentSurfaceID() {
  return current_surface_ ? current_surface_->surface_id() : 0;
}

void Display::ReturnResources(const ReturnedResourceArray& resources) {
  // We never generate any resources, so we should never have any returned.
  DCHECK(resources.empty());
}

}  // namespace cc

#endif  // CC_SURFACES_DISPLAY_H_
