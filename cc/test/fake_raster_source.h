// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_RASTER_SOURCE_H_
#define CC_TEST_FAKE_RASTER_SOURCE_H_

#include "base/memory/ref_counted.h"
#include "cc/playback/raster_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

class SkCanvas;

namespace cc {

class RecordingSource;

class FakeRasterSource : public RasterSource {
 public:
  static scoped_refptr<FakeRasterSource> CreateInfiniteFilled();
  static scoped_refptr<FakeRasterSource> CreateFilled(const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFilledLCD(const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFilledSolidColor(
      const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreatePartiallyFilled(
      const gfx::Size& size,
      const gfx::Rect& recorded_viewport);
  static scoped_refptr<FakeRasterSource> CreateEmpty(const gfx::Size& size);
  static scoped_refptr<FakeRasterSource> CreateFromRecordingSource(
      const RecordingSource* recording_source,
      bool can_use_lcd);
  static scoped_refptr<FakeRasterSource> CreateFromRecordingSourceWithWaitable(
      const RecordingSource* recording_source,
      bool can_use_lcd,
      base::WaitableEvent* playback_allowed_event);

  void PlaybackToCanvas(SkCanvas* canvas,
                        const gfx::Rect& canvas_bitmap_rect,
                        const gfx::Rect& canvas_playback_rect,
                        float contents_scale,
                        const PlaybackSettings& settings) const override;

 protected:
  FakeRasterSource(const RecordingSource* recording_source, bool can_use_lcd);
  FakeRasterSource(const RecordingSource* recording_source,
                   bool can_use_lcd,
                   base::WaitableEvent* playback_allowed_event);
  ~FakeRasterSource() override;

 private:
  base::WaitableEvent* playback_allowed_event_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_RASTER_SOURCE_H_
