// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PLAYBACK_DISPLAY_LIST_RASTER_SOURCE_H_
#define CC_PLAYBACK_DISPLAY_LIST_RASTER_SOURCE_H_

#include <stddef.h>

#include <vector>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/playback/display_list_recording_source.h"
#include "skia/ext/analysis_canvas.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace cc {
class DisplayItemList;
class DrawImage;
class ImageDecodeController;

class CC_EXPORT DisplayListRasterSource
    : public base::RefCountedThreadSafe<DisplayListRasterSource> {
 public:
  static scoped_refptr<DisplayListRasterSource>
  CreateFromDisplayListRecordingSource(const DisplayListRecordingSource* other,
                                       bool can_use_lcd_text);

  // Raster a subrect of this RasterSource into the given canvas. It is
  // assumed that contents_scale has already been applied to this canvas.
  // Writes the total number of pixels rasterized and the time spent
  // rasterizing to the stats if the respective pointer is not nullptr.
  // It is assumed that the canvas passed here will only be rasterized by
  // this raster source via this call.
  //
  // Virtual for testing.
  //
  // Note that this should only be called after the image decode controller has
  // been set, which happens during commit.
  virtual void PlaybackToCanvas(SkCanvas* canvas,
                                const gfx::Rect& canvas_bitmap_rect,
                                const gfx::Rect& canvas_playback_rect,
                                float contents_scale) const;

  // Similar to above, except that the canvas passed here can (or was already)
  // rasterized into by another raster source. That is, it is not safe to clear
  // the canvas or discard its underlying memory.
  void PlaybackToSharedCanvas(SkCanvas* canvas,
                              const gfx::Rect& canvas_rect,
                              float contents_scale) const;

  // Returns whether the given rect at given scale is of solid color in
  // this raster source, as well as the solid color value.
  bool PerformSolidColorAnalysis(const gfx::Rect& content_rect,
                                 float contents_scale,
                                 SkColor* color) const;

  // Returns true iff the whole raster source is of solid color.
  bool IsSolidColor() const;

  // Returns the color of the raster source if it is solid color. The results
  // are unspecified if IsSolidColor returns false.
  SkColor GetSolidColor() const;

  // Returns the size of this raster source.
  gfx::Size GetSize() const;

  // Populate the given list with all images that may overlap the given
  // rect in layer space. The returned draw images' matrices are modified as if
  // they were being using during raster at scale |raster_scale|.
  void GetDiscardableImagesInRect(const gfx::Rect& layer_rect,
                                  float raster_scale,
                                  std::vector<DrawImage>* images) const;

  bool HasDiscardableImageInRect(const gfx::Rect& layer_rect) const;

  // Return true iff this raster source can raster the given rect in layer
  // space.
  bool CoversRect(const gfx::Rect& layer_rect) const;

  // Returns true if this raster source has anything to rasterize.
  virtual bool HasRecordings() const;

  // Valid rectangle in which everything is recorded and can be rastered from.
  virtual gfx::Rect RecordedViewport() const;

  // Informs the raster source that it should attempt to use distance field text
  // during rasterization.
  virtual void SetShouldAttemptToUseDistanceFieldText();

  // Return true iff this raster source would benefit from using distance
  // field text.
  virtual bool ShouldAttemptToUseDistanceFieldText() const;

  // Tracing functionality.
  virtual void DidBeginTracing();
  virtual void AsValueInto(base::trace_event::TracedValue* array) const;
  virtual skia::RefPtr<SkPicture> GetFlattenedPicture();
  virtual size_t GetPictureMemoryUsage() const;

  // Return true if LCD anti-aliasing may be used when rastering text.
  virtual bool CanUseLCDText() const;

  scoped_refptr<DisplayListRasterSource> CreateCloneWithoutLCDText() const;

  // Image decode controller should be set once. Its lifetime has to exceed that
  // of the raster source, since the raster source will access it during raster.
  void SetImageDecodeController(ImageDecodeController* image_decode_controller);

 protected:
  friend class base::RefCountedThreadSafe<DisplayListRasterSource>;

  DisplayListRasterSource(const DisplayListRecordingSource* other,
                          bool can_use_lcd_text);
  DisplayListRasterSource(const DisplayListRasterSource* other,
                          bool can_use_lcd_text);
  virtual ~DisplayListRasterSource();

  // These members are const as this raster source may be in use on another
  // thread and so should not be touched after construction.
  const scoped_refptr<DisplayItemList> display_list_;
  const size_t painter_reported_memory_usage_;
  const SkColor background_color_;
  const bool requires_clear_;
  const bool can_use_lcd_text_;
  const bool is_solid_color_;
  const SkColor solid_color_;
  const gfx::Rect recorded_viewport_;
  const gfx::Size size_;
  const bool clear_canvas_with_debug_color_;
  const int slow_down_raster_scale_factor_for_debug_;
  // TODO(enne/vmiura): this has a read/write race between raster and compositor
  // threads with multi-threaded Ganesh.  Make this const or remove it.
  bool should_attempt_to_use_distance_field_text_;

  // In practice, this is only set once before raster begins, so it's ok with
  // respect to threading.
  ImageDecodeController* image_decode_controller_;

 private:
  // Called when analyzing a tile. We can use AnalysisCanvas as
  // SkPicture::AbortCallback, which allows us to early out from analysis.
  void RasterForAnalysis(skia::AnalysisCanvas* canvas,
                         const gfx::Rect& canvas_rect,
                         float contents_scale) const;

  void RasterCommon(SkCanvas* canvas,
                    SkPicture::AbortCallback* callback,
                    const gfx::Rect& canvas_bitmap_rect,
                    const gfx::Rect& canvas_playback_rect,
                    float contents_scale) const;

  void PrepareForPlaybackToCanvas(SkCanvas* canvas,
                                  const gfx::Rect& canvas_bitmap_rect,
                                  const gfx::Rect& canvas_playback_rect,
                                  float contents_scale) const;

  DISALLOW_COPY_AND_ASSIGN(DisplayListRasterSource);
};

}  // namespace cc

#endif  // CC_PLAYBACK_DISPLAY_LIST_RASTER_SOURCE_H_
