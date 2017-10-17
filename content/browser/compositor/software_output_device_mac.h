// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_
#define CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_

#include <IOSurface/IOSurface.h>
#include <list>

#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "components/viz/service/display/software_output_device.h"
#include "content/common/content_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/vsync_provider.h"

class SkCanvas;

namespace content {

class CONTENT_EXPORT SoftwareOutputDeviceMac : public viz::SoftwareOutputDevice,
                                               public gfx::VSyncProvider {
 public:
  explicit SoftwareOutputDeviceMac(gfx::AcceleratedWidget widget);
  ~SoftwareOutputDeviceMac() override;

  // viz::SoftwareOutputDevice implementation.
  void Resize(const gfx::Size& pixel_size, float scale_factor) override;
  SkCanvas* BeginPaint(const gfx::Rect& damage_rect) override;
  void EndPaint() override;
  void DiscardBackbuffer() override;
  void EnsureBackbuffer() override;
  gfx::VSyncProvider* GetVSyncProvider() override;

  // gfx::VSyncProvider implementation.
  void GetVSyncParameters(
      const gfx::VSyncProvider::UpdateVSyncCallback& callback) override;

  // Testing methods.
  SkRegion LastCopyRegionForTesting() const {
    return last_copy_region_for_testing_;
  }
  IOSurfaceRef CurrentPaintIOSurfaceForTesting() const {
    return current_paint_buffer_->io_surface.get();
  }
  size_t BufferQueueSizeForTesting() const { return buffer_queue_.size(); }

 private:
  struct Buffer {
    Buffer();
    ~Buffer();
    base::ScopedCFTypeRef<IOSurfaceRef> io_surface;
    // The damage of all BeginPaints since this buffer was the back buffer.
    SkRegion accumulated_damage;
  };

  // Copy the pixels from the previous buffer to the new buffer, and union
  // |new_damage_rect| into all |buffer_queue_|'s accumulated damages.
  void UpdateAndCopyBufferDamage(Buffer* previous_paint_buffer,
                                 const SkRegion& new_damage_rect);

  gfx::AcceleratedWidget widget_ = gfx::kNullAcceleratedWidget;
  gfx::Size pixel_size_;
  float scale_factor_ = 1;

  // The queue of buffers. The back is the most recently painted buffer
  // (sometimes equal to |current_paint_buffer_|), and the front is the
  // least recently painted buffer.
  std::list<std::unique_ptr<Buffer>> buffer_queue_;

  // A pointer to the last element of |buffer_queue_| during paint. It is only
  // valid between BeginPaint and EndPaint.
  Buffer* current_paint_buffer_ = nullptr;

  // The SkCanvas wraps the mapped |current_paint_buffer_|'s IOSurface. It is
  // valid only between BeginPaint and EndPaint.
  std::unique_ptr<SkCanvas> current_paint_canvas_;

  gfx::VSyncProvider::UpdateVSyncCallback update_vsync_callback_;

  SkRegion last_copy_region_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(SoftwareOutputDeviceMac);
};

}  // namespace content

#endif  // CONTENT_BROWSER_COMPOSITOR_SOFTWARE_OUTPUT_DEVICE_MAC_H_
