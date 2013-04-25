// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_

#include "base/memory/scoped_vector.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/software_output_device.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace content {

// This class can be created only on the main thread, but then becomes pinned
// to a fixed thread when BindToClient is called.
class CompositorSoftwareOutputDevice
    : NON_EXPORTED_BASE(public cc::SoftwareOutputDevice),
      NON_EXPORTED_BASE(public base::NonThreadSafe) {
public:
  CompositorSoftwareOutputDevice();
  virtual ~CompositorSoftwareOutputDevice();

  virtual void Resize(gfx::Size size) OVERRIDE;

  virtual SkCanvas* BeginPaint(gfx::Rect damage_rect) OVERRIDE;
  virtual void EndPaint(cc::SoftwareFrameData* frame_data) OVERRIDE;

  virtual void ReclaimDIB(const TransportDIB::Id& id) OVERRIDE;

private:
  class DIB {
   public:
    explicit DIB(size_t size);
    ~DIB();

    TransportDIB* dib() const {
      return dib_;
    }

   private:
    TransportDIB* dib_;

    DISALLOW_COPY_AND_ASSIGN(DIB);
  };

  class CompareById {
   public:
    CompareById(const TransportDIB::Id& id) : id_(id) {}

    bool operator()(const DIB* dib) const {
      return dib->dib() && dib->dib()->id() == id_;
    }

   private:
    TransportDIB::Id id_;
  };

  DIB* CreateDIB();

  int front_buffer_;
  int num_free_buffers_;
  ScopedVector<DIB> dibs_;
  ScopedVector<DIB> awaiting_ack_;
  SkBitmap bitmap_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_
