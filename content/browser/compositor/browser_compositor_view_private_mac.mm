// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/compositor/browser_compositor_view_private_mac.h"

#include "base/debug/trace_event.h"
#include "content/browser/compositor/gpu_process_transport_factory.h"
#include "content/browser/renderer_host/compositing_iosurface_context_mac.h"
#include "content/browser/renderer_host/compositing_iosurface_mac.h"
#include "content/browser/renderer_host/software_layer_mac.h"
#include "content/public/browser/context_factory.h"
#include "ui/base/cocoa/animation_utils.h"
#include "ui/gl/scoped_cgl.h"

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewMacInternal

namespace content {

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

  // Set the Cocoa view to be hosting the un-flipped background layer (hosting
  // a flipped layer results in unpredictable behavior).
  cocoa_view_.reset([[BrowserCompositorViewCocoa alloc] initWithClient:this]);

  // Create a compositor to draw the contents of |cocoa_view_|.
  compositor_.reset(new ui::Compositor(
      cocoa_view_, content::GetContextFactory()));
}

BrowserCompositorViewMacInternal::~BrowserCompositorViewMacInternal() {
  DCHECK(!client_);
  [cocoa_view_ resetClient];
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

  compositor_->SetScaleAndSize(1.0, gfx::Size(0, 0));
  compositor_->SetRootLayer(NULL);
  client_ = NULL;
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
  CGRect new_layer_bounds = CGRectMake(
    0,
    0,
    [accelerated_layer_ iosurface]->dip_io_surface_size().width(),
    [accelerated_layer_ iosurface]->dip_io_surface_size().height());
  bool bounds_changed = !CGRectEqualToRect(
      new_layer_bounds, [accelerated_layer_ bounds]);
  [accelerated_layer_ setBounds:new_layer_bounds];
  if (bounds_changed) {
    [accelerated_layer_ setNeedsDisplay];
    [accelerated_layer_ displayIfNeeded];
  }

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
  [software_layer_ setContentsToData:pixels
                        withRowBytes:row_bytes
                       withPixelSize:gfx::Size(info.fWidth, info.fHeight)
                     withScaleFactor:scale_factor];

  // If there was an accelerated layer, remove it.
  {
    [accelerated_layer_ resetClient];
    [accelerated_layer_ removeFromSuperlayer];
    accelerated_layer_.reset();
  }
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

}  // namespace content

////////////////////////////////////////////////////////////////////////////////
// BrowserCompositorViewCocoa

@implementation BrowserCompositorViewCocoa

- (id)initWithClient:(content::BrowserCompositorViewCocoaClient*)client {
  if (self = [super init]) {
    client_ = client;
  }
  return self;
}

- (void)dealloc {
  DCHECK(!client_);
  [super dealloc];
}

- (void)resetClient {
  client_ = NULL;
}

- (void)gotAcceleratedIOSurfaceFrame:(IOSurfaceID)surface_handle
                 withOutputSurfaceID:(int)surface_id
                     withLatencyInfo:(std::vector<ui::LatencyInfo>)latency_info
                       withPixelSize:(gfx::Size)pixel_size
                     withScaleFactor:(float)scale_factor {
  if (!client_)
    return;
  client_->GotAcceleratedIOSurfaceFrame(
      surface_handle, surface_id, latency_info, pixel_size, scale_factor);
}

- (void)gotSoftwareFrame:(cc::SoftwareFrameData*)frame_data
         withScaleFactor:(float)scale_factor
              withCanvas:(SkCanvas*)canvas {
  if (!client_)
    return;
  client_->GotSoftwareFrame(frame_data, scale_factor, canvas);
}

@end  // BrowserCompositorViewCocoa

