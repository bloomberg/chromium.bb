// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display_embedder/software_output_device_ozone.h"

#include <memory>

#include "ui/gfx/skia_util.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/ozone/public/ozone_platform.h"
#include "ui/ozone/public/surface_factory_ozone.h"
#include "ui/ozone/public/surface_ozone_canvas.h"

namespace viz {

// static
std::unique_ptr<SoftwareOutputDeviceOzone> SoftwareOutputDeviceOzone::Create(
    gfx::AcceleratedWidget widget) {
  std::unique_ptr<SoftwareOutputDeviceOzone> result(
      new SoftwareOutputDeviceOzone(widget));
  if (!result->surface_ozone_)
    return nullptr;
  return result;
}

SoftwareOutputDeviceOzone::SoftwareOutputDeviceOzone(
    gfx::AcceleratedWidget widget) {
  ui::SurfaceFactoryOzone* factory =
      ui::OzonePlatform::GetInstance()->GetSurfaceFactoryOzone();

  surface_ozone_ = factory->CreateCanvasForWidget(widget);

  if (!surface_ozone_) {
    LOG(ERROR) << "Failed to initialize canvas";
    return;
  }

  vsync_provider_ = surface_ozone_->CreateVSyncProvider();
}

SoftwareOutputDeviceOzone::~SoftwareOutputDeviceOzone() {}

void SoftwareOutputDeviceOzone::Resize(const gfx::Size& viewport_pixel_size,
                                       float scale_factor) {
  if (viewport_pixel_size_ == viewport_pixel_size)
    return;

  viewport_pixel_size_ = viewport_pixel_size;

  surface_ozone_->ResizeCanvas(viewport_pixel_size_);
}

SkCanvas* SoftwareOutputDeviceOzone::BeginPaint(const gfx::Rect& damage_rect) {
  DCHECK(gfx::Rect(viewport_pixel_size_).Contains(damage_rect));

  // Get canvas for next frame.
  surface_ = surface_ozone_->GetSurface();

  return SoftwareOutputDevice::BeginPaint(damage_rect);
}

void SoftwareOutputDeviceOzone::EndPaint() {
  SoftwareOutputDevice::EndPaint();

  surface_ozone_->PresentCanvas(damage_rect_);
}

}  // namespace viz
