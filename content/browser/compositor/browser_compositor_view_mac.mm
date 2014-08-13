// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_mac.h"

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "content/browser/gpu/gpu_process_host_ui_shim.h"
#include "content/browser/compositor/browser_compositor_view_private_mac.h"
#include "content/common/gpu/gpu_messages.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewMac

namespace content {
namespace {

// The number of placeholder objects allocated. If this reaches zero, then
// the BrowserCompositorViewMacInternal being held on to for recycling,
// |g_recyclable_internal_view|, will be freed.
uint32 g_placeholder_count = 0;

// A spare BrowserCompositorViewMacInternal kept around for recycling.
base::LazyInstance<scoped_ptr<BrowserCompositorViewMacInternal>>
  g_recyclable_internal_view;

}  // namespace

BrowserCompositorViewMac::BrowserCompositorViewMac(
    BrowserCompositorViewMacClient* client) : client_(client) {
  // Try to use the recyclable BrowserCompositorViewMacInternal if there is one,
  // otherwise allocate a new one.
  // TODO(ccameron): If there exists a frame in flight (swap has been called
  // by the compositor, but the frame has not arrived from the GPU process
  // yet), then that frame may inappropriately flash in the new view.
  internal_view_ = g_recyclable_internal_view.Get().Pass();
  if (!internal_view_)
    internal_view_.reset(new BrowserCompositorViewMacInternal);
  internal_view_->SetClient(client_);
}

BrowserCompositorViewMac::~BrowserCompositorViewMac() {
  // Make this BrowserCompositorViewMacInternal recyclable for future instances.
  internal_view_->ResetClient();
  g_recyclable_internal_view.Get() = internal_view_.Pass();

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorViewMacInternal that we just populated.
  if (!g_placeholder_count)
    g_recyclable_internal_view.Get().reset();
}

ui::Compositor* BrowserCompositorViewMac::GetCompositor() const {
  DCHECK(internal_view_);
  return internal_view_->compositor();
}

bool BrowserCompositorViewMac::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  if (internal_view_)
    return internal_view_->HasFrameOfSize(dip_size);
  return false;
}

void BrowserCompositorViewMac::BeginPumpingFrames() {
  if (internal_view_)
    internal_view_->BeginPumpingFrames();
}

void BrowserCompositorViewMac::EndPumpingFrames() {
  if (internal_view_)
    internal_view_->EndPumpingFrames();
}

// static
void BrowserCompositorViewMac::GotAcceleratedFrame(
    gfx::AcceleratedWidget widget,
    uint64 surface_handle, int surface_id,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size, float scale_factor,
    int gpu_host_id, int gpu_route_id) {
  BrowserCompositorViewMacInternal* internal_view =
      BrowserCompositorViewMacInternal::FromAcceleratedWidget(widget);
  int renderer_id = 0;
  if (internal_view) {
    internal_view->GotAcceleratedFrame(
        surface_handle, surface_id, latency_info, pixel_size, scale_factor);
    renderer_id = internal_view->GetRendererID();
  }

  // Acknowledge the swap, now that it has been processed.
  AcceleratedSurfaceMsg_BufferPresented_Params ack_params;
  ack_params.sync_point = 0;
  ack_params.renderer_id = renderer_id;
  GpuProcessHostUIShim* ui_shim = GpuProcessHostUIShim::FromID(gpu_host_id);
  if (ui_shim) {
    ui_shim->Send(new AcceleratedSurfaceMsg_BufferPresented(
        gpu_route_id, ack_params));
  }
}

// static
void BrowserCompositorViewMac::GotSoftwareFrame(
    gfx::AcceleratedWidget widget,
    cc::SoftwareFrameData* frame_data, float scale_factor, SkCanvas* canvas) {
  BrowserCompositorViewMacInternal* internal_view =
      BrowserCompositorViewMacInternal::FromAcceleratedWidget(widget);
  if (internal_view)
    internal_view->GotSoftwareFrame(frame_data, scale_factor, canvas);
}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewPlaceholderMac

BrowserCompositorViewPlaceholderMac::BrowserCompositorViewPlaceholderMac() {
  g_placeholder_count += 1;
}

BrowserCompositorViewPlaceholderMac::~BrowserCompositorViewPlaceholderMac() {
  DCHECK_GT(g_placeholder_count, 0u);
  g_placeholder_count -= 1;

  // If there are no placeholders allocated, destroy the recyclable
  // BrowserCompositorViewMacInternal.
  if (!g_placeholder_count)
    g_recyclable_internal_view.Get().reset();
}

}  // namespace content
