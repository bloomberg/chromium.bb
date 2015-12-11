// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_display_list_recording_source.h"

#include "cc/test/fake_display_list_raster_source.h"
#include "cc/test/skia_common.h"

namespace cc {

FakeDisplayListRecordingSource::FakeDisplayListRecordingSource()
    : force_unsuitable_for_gpu_rasterization_(false),
      playback_allowed_event_(nullptr) {}

bool FakeDisplayListRecordingSource::IsSuitableForGpuRasterization() const {
  if (force_unsuitable_for_gpu_rasterization_)
    return false;
  return DisplayListRecordingSource::IsSuitableForGpuRasterization();
}

scoped_refptr<DisplayListRasterSource>
FakeDisplayListRecordingSource::CreateRasterSource(bool can_use_lcd) const {
  return FakeDisplayListRasterSource::CreateFromRecordingSourceWithWaitable(
      this, can_use_lcd, playback_allowed_event_);
}

bool FakeDisplayListRecordingSource::EqualsTo(
    const FakeDisplayListRecordingSource& other) {
  return recorded_viewport_ == other.recorded_viewport_ &&
         size_ == other.size_ &&
         slow_down_raster_scale_factor_for_debug_ ==
             other.slow_down_raster_scale_factor_for_debug_ &&
         generate_discardable_images_metadata_ ==
             other.generate_discardable_images_metadata_ &&
         requires_clear_ == other.requires_clear_ &&
         is_solid_color_ == other.is_solid_color_ &&
         clear_canvas_with_debug_color_ ==
             other.clear_canvas_with_debug_color_ &&
         solid_color_ == other.solid_color_ &&
         background_color_ == other.background_color_ &&
         AreDisplayListDrawingResultsSame(recorded_viewport_, display_list_,
                                          other.display_list_);
}

}  // namespace cc
