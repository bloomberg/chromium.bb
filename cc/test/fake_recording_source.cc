// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/fake_recording_source.h"

#include "cc/test/fake_raster_source.h"
#include "cc/test/skia_common.h"

namespace cc {

FakeRecordingSource::FakeRecordingSource()
    : force_unsuitable_for_gpu_rasterization_(false),
      playback_allowed_event_(nullptr) {}

bool FakeRecordingSource::IsSuitableForGpuRasterization() const {
  if (force_unsuitable_for_gpu_rasterization_)
    return false;
  return RecordingSource::IsSuitableForGpuRasterization();
}

scoped_refptr<RasterSource> FakeRecordingSource::CreateRasterSource(
    bool can_use_lcd) const {
  return FakeRasterSource::CreateFromRecordingSourceWithWaitable(
      this, can_use_lcd, playback_allowed_event_);
}

bool FakeRecordingSource::EqualsTo(const FakeRecordingSource& other) {
  // The DisplayItemLists are equal if they are both null or they are both not
  // null and render to the same thing.
  bool display_lists_equal = !display_list_ && !other.display_list_;
  if (display_list_ && other.display_list_) {
    display_lists_equal = AreDisplayListDrawingResultsSame(
        recorded_viewport_, display_list_.get(), other.display_list_.get());
  }

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
         background_color_ == other.background_color_ && display_lists_equal;
}

}  // namespace cc
