// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_
#define CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_

#include "base/memory/scoped_vector.h"
#include "base/threading/non_thread_safe.h"
#include "cc/output/software_output_device.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/surface/transport_dib.h"

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
  TransportDIB* CreateDIB();

  int front_buffer_;
  int last_buffer_;
  int num_free_buffers_;
  ScopedVector<TransportDIB> dibs_;
  ScopedVector<TransportDIB> awaiting_ack_;
  SkBitmap bitmap_;
  uint32 sequence_num_;
};

}  // namespace content

#endif  // CONTENT_RENDERER_GPU_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_H_
