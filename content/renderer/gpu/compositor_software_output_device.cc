// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/gpu/compositor_software_output_device.h"

#include "base/logging.h"
#include "cc/output/software_frame_data.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkDevice.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "ui/gfx/skia_util.h"

namespace content {

namespace {

class CompareById {
 public:
  CompareById(const TransportDIB::Id& id)
      : id_(id) {
  }

  bool operator()(const TransportDIB* dib) const {
    return dib->id() == id_;
  }

 private:
  TransportDIB::Id id_;
};

}  // namespace

CompositorSoftwareOutputDevice::CompositorSoftwareOutputDevice()
    : front_buffer_(0),
      last_buffer_(-1),
      num_free_buffers_(0),
      sequence_num_(0) {
  DetachFromThread();
}

CompositorSoftwareOutputDevice::~CompositorSoftwareOutputDevice() {
  DCHECK(CalledOnValidThread());
}

TransportDIB* CompositorSoftwareOutputDevice::CreateDIB() {
  const size_t size = 4 * viewport_size_.GetArea();
  TransportDIB* dib = TransportDIB::Create(size, sequence_num_++);
  CHECK(dib);
  bool success = dib->Map();
  CHECK(success);
  return dib;
}

void CompositorSoftwareOutputDevice::Resize(gfx::Size viewport_size) {
  DCHECK(CalledOnValidThread());

  // Reset last_buffer_ so that we don't copy over old damage.
  last_buffer_ = -1;

  if (viewport_size_ == viewport_size)
    return;
  viewport_size_ = viewport_size;

  // Keep non-acked dibs open.
  for (size_t i = 0; i < dibs_.size() - num_free_buffers_; ++i) {
    size_t index = (front_buffer_ + num_free_buffers_ + i) % dibs_.size();
    awaiting_ack_.push_back(dibs_[index]);
    dibs_[index] = NULL;
  }

  dibs_.clear();
  front_buffer_ = 0;
  num_free_buffers_ = 0;
}

SkCanvas* CompositorSoftwareOutputDevice::BeginPaint(gfx::Rect damage_rect) {
  DCHECK(CalledOnValidThread());

  if (num_free_buffers_ == 0) {
    dibs_.insert(dibs_.begin() + front_buffer_, CreateDIB());
    num_free_buffers_++;
  }

  TransportDIB* front_dib = dibs_[front_buffer_];
  DCHECK(front_dib);
  DCHECK(front_dib->memory());

  // Set up a canvas for the front_dib.
  bitmap_.setConfig(SkBitmap::kARGB_8888_Config,
                    viewport_size_.width(),
                    viewport_size_.height());
  bitmap_.setPixels(front_dib->memory());
  device_ = skia::AdoptRef(new SkDevice(bitmap_));
  canvas_ = skia::AdoptRef(new SkCanvas(device_.get()));

  // Copy damage_rect_ from last_buffer_ to front_buffer_.
  if (last_buffer_ != -1 && !damage_rect.Contains(damage_rect_)) {
    TransportDIB* last_dib = dibs_[last_buffer_];
    SkBitmap back_bitmap;
    back_bitmap.setConfig(SkBitmap::kARGB_8888_Config,
                          viewport_size_.width(),
                          viewport_size_.height());
    back_bitmap.setPixels(last_dib->memory());

    SkRect last_damage = gfx::RectToSkRect(damage_rect_);
    canvas_->drawBitmapRectToRect(back_bitmap, &last_damage, last_damage, NULL);
  }
  damage_rect_ = damage_rect;

  return canvas_.get();
}

void CompositorSoftwareOutputDevice::EndPaint(
    cc::SoftwareFrameData* frame_data) {
  DCHECK(CalledOnValidThread());

  if (frame_data) {
    frame_data->size = viewport_size_;
    frame_data->damage_rect = damage_rect_;
    frame_data->dib_id = dibs_[front_buffer_]->id();
  }

  last_buffer_ = front_buffer_;
  front_buffer_ = (front_buffer_ + 1) % dibs_.size();
  --num_free_buffers_;
  DCHECK_GE(num_free_buffers_, 0);
}

void CompositorSoftwareOutputDevice::ReclaimDIB(const TransportDIB::Id& id) {
  DCHECK(CalledOnValidThread());

  // The reclaimed handle might not be among the currently
  // active dibs if we got a resize event in the mean time.
  ScopedVector<TransportDIB>::iterator it =
      std::find_if(dibs_.begin(), dibs_.end(), CompareById(id));
  if (it != dibs_.end()) {
    ++num_free_buffers_;
  } else {
    it = std::find_if(awaiting_ack_.begin(),
                      awaiting_ack_.end(),
                      CompareById(id));
    awaiting_ack_.erase(it);
  }

  DCHECK_LE(static_cast<size_t>(num_free_buffers_), dibs_.size());
}

}  // namespace content
