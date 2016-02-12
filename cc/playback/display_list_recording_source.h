// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_LIST_RECORDING_SOURCE_H_
#define CC_PLAYBACK_DISPLAY_LIST_RECORDING_SOURCE_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"

namespace cc {

namespace proto {
class DisplayListRecordingSource;
}  // namespace proto

class ContentLayerClient;
class DisplayItemList;
class DisplayListRasterSource;
class ImageSerializationProcessor;
class Region;

class CC_EXPORT DisplayListRecordingSource {
 public:
  // TODO(schenney) Remove RECORD_WITH_SK_NULL_CANVAS when we no longer
  // support a non-Slimming Paint path.
  enum RecordingMode {
    RECORD_NORMALLY,
    RECORD_WITH_SK_NULL_CANVAS,
    RECORD_WITH_PAINTING_DISABLED,
    RECORD_WITH_CACHING_DISABLED,
    RECORD_WITH_CONSTRUCTION_DISABLED,
    RECORD_WITH_SUBSEQUENCE_CACHING_DISABLED,
    RECORDING_MODE_COUNT,  // Must be the last entry.
  };

  DisplayListRecordingSource();
  virtual ~DisplayListRecordingSource();

  void ToProtobuf(
      proto::DisplayListRecordingSource* proto,
      ImageSerializationProcessor* image_serialization_processor) const;
  void FromProtobuf(const proto::DisplayListRecordingSource& proto,
                    ImageSerializationProcessor* image_serialization_processor);

  bool UpdateAndExpandInvalidation(ContentLayerClient* painter,
                                   Region* invalidation,
                                   const gfx::Size& layer_size,
                                   const gfx::Rect& visible_layer_rect,
                                   int frame_number,
                                   RecordingMode recording_mode);
  gfx::Size GetSize() const;
  void SetEmptyBounds();
  void SetSlowdownRasterScaleFactor(int factor);
  void SetGenerateDiscardableImagesMetadata(bool generate_metadata);
  void SetBackgroundColor(SkColor background_color);
  void SetRequiresClear(bool requires_clear);

  // These functions are virtual for testing.
  virtual scoped_refptr<DisplayListRasterSource> CreateRasterSource(
      bool can_use_lcd_text) const;
  virtual bool IsSuitableForGpuRasterization() const;

  gfx::Rect recorded_viewport() const { return recorded_viewport_; }

 protected:
  void Clear();

  gfx::Rect recorded_viewport_;
  gfx::Size size_;
  int slow_down_raster_scale_factor_for_debug_;
  bool generate_discardable_images_metadata_;
  bool requires_clear_;
  bool is_solid_color_;
  bool clear_canvas_with_debug_color_;
  SkColor solid_color_;
  SkColor background_color_;

  scoped_refptr<DisplayItemList> display_list_;
  size_t painter_reported_memory_usage_;

 private:
  void UpdateInvalidationForNewViewport(const gfx::Rect& old_recorded_viewport,
                                        const gfx::Rect& new_recorded_viewport,
                                        Region* invalidation);
  void FinishDisplayItemListUpdate();

  friend class DisplayListRasterSource;

  void DetermineIfSolidColor();

  DISALLOW_COPY_AND_ASSIGN(DisplayListRecordingSource);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_LIST_RECORDING_SOURCE_H_
