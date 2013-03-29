// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/software_output_device.h"

#include "base/logging.h"
#include "cc/output/software_frame_data.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"

namespace cc {

SoftwareOutputDevice::SoftwareOutputDevice() {}

SoftwareOutputDevice::~SoftwareOutputDevice() {}

void SoftwareOutputDevice::Resize(gfx::Size viewport_size) {
  if (viewport_size_ == viewport_size)
    return;

  viewport_size_ = viewport_size;
  device_ = skia::AdoptRef(new SkDevice(SkBitmap::kARGB_8888_Config,
      viewport_size.width(), viewport_size.height(), true));
  canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));
}

SkCanvas* SoftwareOutputDevice::BeginPaint(gfx::Rect damage_rect) {
  DCHECK(device_);
  damage_rect_ = damage_rect;
  return canvas_.get();
}

void SoftwareOutputDevice::EndPaint(SoftwareFrameData* frame_data) {
  DCHECK(device_);
  if (frame_data) {
    frame_data->size = viewport_size_;
    frame_data->damage_rect = damage_rect_;
    frame_data->dib_id = TransportDIB::Id();
  }
}

void SoftwareOutputDevice::CopyToBitmap(
    gfx::Rect rect, SkBitmap* output) {
  DCHECK(device_);
  SkIRect invert_rect = SkIRect::MakeXYWH(
      rect.x(), viewport_size_.height() - rect.bottom(),
      rect.width(), rect.height());
  const SkBitmap& bitmap = device_->accessBitmap(false);
  bitmap.extractSubset(output, invert_rect);
}

void SoftwareOutputDevice::Scroll(
    gfx::Vector2d delta, gfx::Rect clip_rect) {
  NOTIMPLEMENTED();
}

void SoftwareOutputDevice::ReclaimDIB(const TransportDIB::Id& id) {
  NOTIMPLEMENTED();
}

}  // namespace cc
