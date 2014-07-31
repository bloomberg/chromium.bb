// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_private_mac.h"

#include <map>

#include "base/debug/trace_event.h"
#include "base/lazy_instance.h"
#include "base/message_loop/message_loop.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "content/browser/renderer_host/dip_util.h"
#include "content/browser/renderer_host/render_widget_resize_helper.h"
#include "content/browser/renderer_host/software_layer_mac.h"
#include "content/public/browser/context_factory.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gl/scoped_cgl.h"

namespace content {
namespace {

typedef std::map<gfx::AcceleratedWidget,BrowserCompositorViewMacInternal*>
    WidgetToInternalsMap;
base::LazyInstance<WidgetToInternalsMap> g_widget_to_internals_map;

}

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewMacInternal

BrowserCompositorViewMacInternal::BrowserCompositorViewMacInternal()
    : client_(NULL),
      accelerated_layer_output_surface_id_(0) {
  // Disable the fade-in animation as the layers are added.
  ScopedCAActionDisabler disabler;

  // Add a flipped transparent layer as a child, so that we don't need to
  // fiddle with the position of sub-layers -- they will always be at the
  // origin.
  flipped_layer_.reset([[CALayer alloc] init]);
  [flipped_layer_ setGeometryFlipped:YES];
  [flipped_layer_ setAnchorPoint:CGPointMake(0, 0)];
  [flipped_layer_
      setAutoresizingMask:kCALayerWidthSizable|kCALayerHeightSizable];

  // Use a sequence number as the accelerated widget handle that we can use
  // to look up the internals structure.
  static uintptr_t last_sequence_number = 0;
  last_sequence_number += 1;
  native_widget_ = reinterpret_cast<gfx::AcceleratedWidget>(
      last_sequence_number);
  g_widget_to_internals_map.Pointer()->insert(
      std::make_pair(native_widget_, this));

  // Create a compositor to draw the contents of this view.
  compositor_.reset(new ui::Compositor(
      native_widget_,
      content::GetContextFactory(),
      RenderWidgetResizeHelper::Get()->task_runner()));
}

BrowserCompositorViewMacInternal::~BrowserCompositorViewMacInternal() {
  DCHECK(!client_);
  g_widget_to_internals_map.Pointer()->erase(native_widget_);
}

void BrowserCompositorViewMacInternal::SetClient(
    BrowserCompositorViewMacClient* client) {
  // Disable the fade-in animation as the view is added.
  ScopedCAActionDisabler disabler;

  DCHECK(client && !client_);
  client_ = client;
  compositor_->SetRootLayer(client_->BrowserCompositorRootLayer());

  CALayer* background_layer = [client_->BrowserCompositorSuperview() layer];
  DCHECK(background_layer);
  [flipped_layer_ setBounds:[background_layer bounds]];
  [background_layer addSublayer:flipped_layer_];
}

void BrowserCompositorViewMacInternal::ResetClient() {
  if (!client_)
    return;

  // Disable the fade-out animation as the view is removed.
  ScopedCAActionDisabler disabler;

  [flipped_layer_ removeFromSuperlayer];

  [accelerated_layer_ removeFromSuperlayer];
  [accelerated_layer_ resetClient];
  accelerated_layer_.reset();
  accelerated_layer_output_surface_id_ = 0;

  [software_layer_ removeFromSuperlayer];
  software_layer_.reset();

  last_swap_size_dip_ = gfx::Size();

  compositor_->SetScaleAndSize(1.0, gfx::Size(0, 0));
  compositor_->SetRootLayer(NULL);
  client_ = NULL;
}

bool BrowserCompositorViewMacInternal::HasFrameOfSize(
    const gfx::Size& dip_size) const {
  return last_swap_size_dip_ == dip_size;
}

void BrowserCompositorViewMacInternal::BeginPumpingFrames() {
  [accelerated_layer_ beginPumpingFrames];
}

void BrowserCompositorViewMacInternal::EndPumpingFrames() {
  [accelerated_layer_ endPumpingFrames];
}

void BrowserCompositorViewMacInternal::GotAcceleratedIOSurfaceFrame(
    IOSurfaceID io_surface_id,
    int output_surface_id,
    const std::vector<ui::LatencyInfo>& latency_info,
    gfx::Size pixel_size,
    float scale_factor) {
  DCHECK(!accelerated_layer_output_surface_id_);
  accelerated_layer_output_surface_id_ = output_surface_id;
  accelerated_latency_info_.insert(accelerated_latency_info_.end(),
                                   latency_info.begin(), latency_info.end());

  // If there is no client and therefore no superview to draw into, early-out.
  if (!client_) {
    AcceleratedLayerDidDrawFrame(true);
    return;
  }

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  // If there is already an accelerated layer, but it has the wrong scale
  // factor or it was poisoned, remove the old layer and replace it.
  base::scoped_nsobject<CompositingIOSurfaceLayer> old_accelerated_layer;
  if (accelerated_layer_ && (
          [accelerated_layer_ context]->HasBeenPoisoned() ||
          [accelerated_layer_ iosurface]->scale_factor() != scale_factor)) {
    old_accelerated_layer = accelerated_layer_;
    accelerated_layer_.reset();
  }

  // If there is not a layer for accelerated frames, create one.
  if (!accelerated_layer_) {
    scoped_refptr<content::CompositingIOSurfaceMac> iosurface =
        content::CompositingIOSurfaceMac::Create();
    accelerated_layer_.reset([[CompositingIOSurfaceLayer alloc]
        initWithIOSurface:iosurface
          withScaleFactor:scale_factor
               withClient:this]);
    [flipped_layer_ addSublayer:accelerated_layer_];
  }

  // Open the provided IOSurface.
  {
    bool result = true;
    gfx::ScopedCGLSetCurrentContext scoped_set_current_context(
        [accelerated_layer_ context]->cgl_context());
    result = [accelerated_layer_ iosurface]->SetIOSurfaceWithContextCurrent(
        [accelerated_layer_ context], io_surface_id, pixel_size, scale_factor);
    if (!result)
      LOG(ERROR) << "Failed SetIOSurface on CompositingIOSurfaceMac";
  }
  [accelerated_layer_ gotNewFrame];

  // Set the bounds of the accelerated layer to match the size of the frame.
  // If the bounds changed, force the content to be displayed immediately.
  last_swap_size_dip_ = [accelerated_layer_ iosurface]->dip_io_surface_size();
  CGRect new_layer_bounds = CGRectMake(
      0, 0, last_swap_size_dip_.width(), last_swap_size_dip_.height());
  bool bounds_changed = !CGRectEqualToRect(
      new_layer_bounds, [accelerated_layer_ bounds]);
  [accelerated_layer_ setBounds:new_layer_bounds];
  if (bounds_changed)
    [accelerated_layer_ setNeedsDisplayAndDisplayAndAck];

  // If there was a software layer or an old accelerated layer, remove it.
  // Disable the fade-out animation as the layer is removed.
  {
    [software_layer_ removeFromSuperlayer];
    software_layer_.reset();
    [old_accelerated_layer resetClient];
    [old_accelerated_layer removeFromSuperlayer];
    old_accelerated_layer.reset();
  }
}

void BrowserCompositorViewMacInternal::GotSoftwareFrame(
    cc::SoftwareFrameData* frame_data,
    float scale_factor,
    SkCanvas* canvas) {
  if (!frame_data || !canvas || !client_)
    return;

  // Disable the fade-in or fade-out effect if we create or remove layers.
  ScopedCAActionDisabler disabler;

  // If there is not a layer for software frames, create one.
  if (!software_layer_) {
    software_layer_.reset([[SoftwareLayer alloc] init]);
    [flipped_layer_ addSublayer:software_layer_];
  }

  // Set the software layer to draw the provided canvas.
  SkImageInfo info;
  size_t row_bytes;
  const void* pixels = canvas->peekPixels(&info, &row_bytes);
  gfx::Size pixel_size(info.fWidth, info.fHeight);
  [software_layer_ setContentsToData:pixels
                        withRowBytes:row_bytes
                       withPixelSize:pixel_size
                     withScaleFactor:scale_factor];
  last_swap_size_dip_ = ConvertSizeToDIP(scale_factor, pixel_size);

  // If there was an accelerated layer, remove it.
  {
    [accelerated_layer_ resetClient];
    [accelerated_layer_ removeFromSuperlayer];
    accelerated_layer_.reset();
  }
}

bool BrowserCompositorViewMacInternal::AcceleratedLayerShouldAckImmediately()
    const {
  // If there is no client then the accelerated layer is not in the hierarchy
  // and will never draw.
  if (!client_)
    return true;
  return client_->BrowserCompositorViewShouldAckImmediately();
}

void BrowserCompositorViewMacInternal::AcceleratedLayerDidDrawFrame(
    bool succeeded) {
  if (accelerated_layer_output_surface_id_) {
    content::ImageTransportFactory::GetInstance()->OnSurfaceDisplayed(
        accelerated_layer_output_surface_id_);
    accelerated_layer_output_surface_id_ = 0;
  }

  if (client_)
    client_->BrowserCompositorViewFrameSwapped(accelerated_latency_info_);

  accelerated_latency_info_.clear();

  if (!succeeded) {
    if (accelerated_layer_)
      [accelerated_layer_ context]->PoisonContextAndSharegroup();
    compositor_->ScheduleFullRedraw();
  }
}

// static
BrowserCompositorViewMacInternal* BrowserCompositorViewMacInternal::
    FromAcceleratedWidget(gfx::AcceleratedWidget widget) {
  WidgetToInternalsMap::const_iterator found =
      g_widget_to_internals_map.Pointer()->find(widget);
  // This can end up being accessed after the underlying widget has been
  // destroyed, but while the ui::Compositor is still being destroyed.
  // Return NULL in these cases.
  if (found == g_widget_to_internals_map.Pointer()->end())
    return NULL;
  return found->second;
}


}  // namespace content

