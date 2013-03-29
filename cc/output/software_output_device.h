// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_SOFTWARE_OUTPUT_DEVICE_H_
#define CC_OUTPUT_SOFTWARE_OUTPUT_DEVICE_H_

#include "base/basictypes.h"
#include "cc/base/cc_export.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"
#include "ui/gfx/vector2d.h"
#include "ui/surface/transport_dib.h"

class SkBitmap;
class SkDevice;
class SkCanvas;

namespace cc {

class SoftwareFrameData;

// This is a "tear-off" class providing software drawing support to
// OutputSurface, such as to a platform-provided window framebuffer.
class CC_EXPORT SoftwareOutputDevice {
 public:
  SoftwareOutputDevice();
  virtual ~SoftwareOutputDevice();

  // SoftwareOutputDevice implementation.
  virtual void Resize(gfx::Size size);

  virtual SkCanvas* BeginPaint(gfx::Rect damage_rect);
  virtual void EndPaint(SoftwareFrameData* frame_data);

  virtual void CopyToBitmap(gfx::Rect rect, SkBitmap* output);
  virtual void Scroll(gfx::Vector2d delta,
                      gfx::Rect clip_rect);

  // TODO(skaslev) Remove this after UberCompositor lands.
  virtual void ReclaimDIB(const TransportDIB::Id& id);

 protected:
  gfx::Size viewport_size_;
  gfx::Rect damage_rect_;
  skia::RefPtr<SkDevice> device_;
  skia::RefPtr<SkCanvas> canvas_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDevice);
};

}  // namespace cc

#endif  // CC_OUTPUT_SOFTWARE_OUTPUT_DEVICE_H_
