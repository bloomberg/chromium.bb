// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_software_output_device.h"

#include "base/logging.h"
#include "cc/output/software_frame_data.h"
#include "content/renderer/render_process.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/skia_util.h"
#include "ui/surface/transport_dib.h"

namespace content {

CompositorSoftwareOutputDevice::DIB::DIB(size_t size) {
  RenderProcess* render_process = RenderProcess::current();
  dib_ = render_process->CreateTransportDIB(size);
  CHECK(dib_);
  bool success = dib_->Map();
  CHECK(success);
}

CompositorSoftwareOutputDevice::DIB::~DIB() {
  RenderProcess* render_process = RenderProcess::current();
  render_process->FreeTransportDIB(dib_);
}

CompositorSoftwareOutputDevice::CompositorSoftwareOutputDevice()
    : front_buffer_(-1),
      num_free_buffers_(0) {
  DetachFromThread();
}

CompositorSoftwareOutputDevice::~CompositorSoftwareOutputDevice() {
  DCHECK(CalledOnValidThread());
}

CompositorSoftwareOutputDevice::DIB*
CompositorSoftwareOutputDevice::CreateDIB() {
  const size_t size =  4 * viewport_size_.GetArea();
  return new DIB(size);
}

void CompositorSoftwareOutputDevice::Resize(gfx::Size viewport_size) {
  DCHECK(CalledOnValidThread());

  if (viewport_size_ == viewport_size)
    return;

  // Keep non-ACKed dibs open.
  int first_non_free = front_buffer_ + num_free_buffers_ + 1;
  int num_non_free = dibs_.size() - num_free_buffers_;
  for (int i = 0; i < num_non_free; ++i) {
    int index = (first_non_free + i) % dibs_.size();
    awaiting_ack_.push_back(dibs_[index]);
    dibs_[index] = NULL;
  }

  dibs_.clear();
  front_buffer_ = -1;
  num_free_buffers_ = 0;
  viewport_size_ = viewport_size;
}

SkCanvas* CompositorSoftwareOutputDevice::BeginPaint(gfx::Rect damage_rect) {
  DCHECK(CalledOnValidThread());

  gfx::Rect last_damage_rect = damage_rect_;
  damage_rect_ = damage_rect;

  int last_buffer = front_buffer_;
  if (num_free_buffers_ == 0) {
    dibs_.insert(dibs_.begin() + (front_buffer_ + 1), CreateDIB());
    last_damage_rect = gfx::Rect(viewport_size_);
  } else {
    --num_free_buffers_;
  }
  front_buffer_ = (front_buffer_ + 1) % dibs_.size();

  TransportDIB* front_dib = dibs_[front_buffer_]->dib();
  DCHECK(front_dib);
  DCHECK(front_dib->memory());

  // Set up a canvas for the current front buffer.
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
                    viewport_size_.width(),
                    viewport_size_.height());
  bitmap_.setPixels(front_dib->memory());
  device_ = skia::AdoptRef(new SkDevice(bitmap_));
  canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));

  // Copy over previous damage.
  if (last_buffer != -1) {
    TransportDIB* last_dib = dibs_[last_buffer]->dib();
    SkBitmap back_bitmap;
    back_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                          viewport_size_.width(),
                          viewport_size_.height());
    back_bitmap.setPixels(last_dib->memory());

    SkRegion region(RectToSkIRect(last_damage_rect));
    region.op(RectToSkIRect(damage_rect), SkRegion::kDifference_Op);
    for (SkRegion::Iterator it(region); !it.done(); it.next()) {
      const SkIRect& src_rect = it.rect();
      SkRect dst_rect = SkRect::Make(src_rect);
      canvas_->drawBitmapRect(back_bitmap, &src_rect, dst_rect, NULL);
    }
  }

  return canvas_.get();
}

void CompositorSoftwareOutputDevice::EndPaint(
    cc::SoftwareFrameData* frame_data) {
  DCHECK(CalledOnValidThread());

  if (frame_data) {
    frame_data->size = viewport_size_;
    frame_data->damage_rect = damage_rect_;
    frame_data->dib_id = dibs_[front_buffer_]->dib()->id();
  }
}

void CompositorSoftwareOutputDevice::ReclaimDIB(const TransportDIB::Id& id) {
  DCHECK(CalledOnValidThread());

  if (!TransportDIB::is_valid_id(id))
    return;

  // The reclaimed dib id might not be among the currently
  // active dibs if we got a resize event in the mean time.
  ScopedVector<DIB>::iterator it =
      std::find_if(dibs_.begin(), dibs_.end(), CompareById(id));
  if (it != dibs_.end()) {
    ++num_free_buffers_;
    DCHECK_LE(static_cast<size_t>(num_free_buffers_), dibs_.size());
    return;
  } else {
    it = std::find_if(awaiting_ack_.begin(), awaiting_ack_.end(),
                      CompareById(id));
    DCHECK(it != awaiting_ack_.end());
    awaiting_ack_.erase(it);
  }
}

}  // namespace content
