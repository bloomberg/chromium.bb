// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/aura/software_output_device_ozone.h"
#include "third_party/skia/include/core/SkBitmapDevice.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "ui/compositor/compositor.h"
#include "ui/gfx/ozone/surface_factory_ozone.h"
#include "ui/gfx/skia_util.h"

namespace content {

SoftwareOutputDeviceOzone::SoftwareOutputDeviceOzone(
    ui::Compositor* compositor) : compositor_(compositor) {
  if (gfx::SurfaceFactoryOzone::GetInstance()->InitializeHardware() !=
      gfx::SurfaceFactoryOzone::INITIALIZED)
    LOG(FATAL) << "Failed to initialize hardware in OZONE";
}

SoftwareOutputDeviceOzone::~SoftwareOutputDeviceOzone() {
}

void SoftwareOutputDeviceOzone::Resize(gfx::Size viewport_size) {
  if (viewport_size_ == viewport_size)
    return;

  viewport_size_ = viewport_size;
  gfx::Rect bounds(viewport_size_);

  gfx::SurfaceFactoryOzone* factory = gfx::SurfaceFactoryOzone::GetInstance();
  factory->AttemptToResizeAcceleratedWidget(compositor_->widget(),
                                            bounds);
  gfx::AcceleratedWidget realized_widget = factory->RealizeAcceleratedWidget(
      compositor_->widget());

  if (realized_widget == gfx::kNullAcceleratedWidget)
    LOG(FATAL) << "Failed to get a realized AcceleratedWidget";

  canvas_ = skia::SharePtr(factory->GetCanvasForWidget(realized_widget));
  device_ = skia::SharePtr(canvas_->getDevice());
}

SkCanvas* SoftwareOutputDeviceOzone::BeginPaint(gfx::Rect damage_rect) {
  DCHECK(gfx::Rect(viewport_size_).Contains(damage_rect));

  canvas_->clipRect(gfx::RectToSkRect(damage_rect), SkRegion::kReplace_Op);
  // Save the current state so we can restore once we're done drawing. This is
  // saved after the clip since we want to keep the clip information after we're
  // done drawing such that OZONE knows what was updated.
  canvas_->save();

  return SoftwareOutputDevice::BeginPaint(damage_rect);
}

void SoftwareOutputDeviceOzone::EndPaint(cc::SoftwareFrameData* frame_data) {
  SoftwareOutputDevice::EndPaint(frame_data);

  canvas_->restore();

  if (damage_rect_.IsEmpty())
    return;

  bool scheduled = gfx::SurfaceFactoryOzone::GetInstance()->SchedulePageFlip(
      compositor_->widget());
  DCHECK(scheduled) << "Failed to schedule pageflip";
}

}  // namespace content
