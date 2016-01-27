// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/display_list_raster_source.h"

#include <stddef.h>

#include "base/containers/adapters.h"
#include "base/trace_event/trace_event.h"
#include "cc/base/region.h"
#include "cc/debug/debug_colors.h"
#include "cc/playback/discardable_image_map.h"
#include "cc/playback/display_item_list.h"
#include "cc/tiles/image_decode_controller.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"

namespace cc {

namespace {

SkIRect RoundOutRect(const SkRect& rect) {
  SkIRect result;
  rect.roundOut(&result);
  return result;
}

class ImageHijackCanvas : public SkNWayCanvas {
 public:
  ImageHijackCanvas(int width,
                    int height,
                    ImageDecodeController* image_decode_controller)
      : SkNWayCanvas(width, height),
        image_decode_controller_(image_decode_controller) {}

 protected:
  // Ensure that pictures are unpacked by this canvas, instead of being
  // forwarded to the raster canvas.
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }

  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override {
    if (!image->isLazyGenerated()) {
      SkNWayCanvas::onDrawImage(image, x, y, paint);
      return;
    }

    SkMatrix ctm = getTotalMatrix();

    SkSize scale;
    bool is_decomposable = ExtractScale(ctm, &scale);
    ScopedDecodedImageLock scoped_lock(
        image_decode_controller_, image,
        SkRect::MakeIWH(image->width(), image->height()), scale,
        is_decomposable, ctm.hasPerspective(), paint);
    const DecodedDrawImage& decoded_image = scoped_lock.decoded_image();
    DCHECK_EQ(0, static_cast<int>(decoded_image.src_rect_offset().width()));
    DCHECK_EQ(0, static_cast<int>(decoded_image.src_rect_offset().height()));
    const SkPaint* decoded_paint = scoped_lock.decoded_paint();

    bool need_scale = !decoded_image.is_scale_adjustment_identity();
    if (need_scale) {
      SkNWayCanvas::save();
      SkNWayCanvas::scale(1.f / (decoded_image.scale_adjustment().width()),
                          1.f / (decoded_image.scale_adjustment().height()));
    }
    SkNWayCanvas::onDrawImage(decoded_image.image(), x, y, decoded_paint);
    if (need_scale)
      SkNWayCanvas::restore();
  }

  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint constraint) override {
    if (!image->isLazyGenerated()) {
      SkNWayCanvas::onDrawImageRect(image, src, dst, paint, constraint);
      return;
    }

    SkRect src_storage;
    if (!src) {
      src_storage = SkRect::MakeIWH(image->width(), image->height());
      src = &src_storage;
    }
    SkMatrix matrix;
    matrix.setRectToRect(*src, dst, SkMatrix::kFill_ScaleToFit);
    matrix.postConcat(getTotalMatrix());

    SkSize scale;
    bool is_decomposable = ExtractScale(matrix, &scale);
    ScopedDecodedImageLock scoped_lock(image_decode_controller_, image, *src,
                                       scale, is_decomposable,
                                       matrix.hasPerspective(), paint);
    const DecodedDrawImage& decoded_image = scoped_lock.decoded_image();
    const SkPaint* decoded_paint = scoped_lock.decoded_paint();

    SkRect adjusted_src =
        src->makeOffset(decoded_image.src_rect_offset().width(),
                        decoded_image.src_rect_offset().height());
    if (!decoded_image.is_scale_adjustment_identity()) {
      float x_scale = decoded_image.scale_adjustment().width();
      float y_scale = decoded_image.scale_adjustment().height();
      adjusted_src = SkRect::MakeXYWH(
          adjusted_src.x() * x_scale, adjusted_src.y() * y_scale,
          adjusted_src.width() * x_scale, adjusted_src.height() * y_scale);
    }
    SkNWayCanvas::onDrawImageRect(decoded_image.image(), &adjusted_src, dst,
                                  decoded_paint, constraint);
  }

  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override {
    // No cc embedder issues image nine calls.
    NOTREACHED();
  }

 private:
  class ScopedDecodedImageLock {
   public:
    ScopedDecodedImageLock(ImageDecodeController* image_decode_controller,
                           const SkImage* image,
                           const SkRect& src_rect,
                           const SkSize& scale,
                           bool is_decomposable,
                           bool has_perspective,
                           const SkPaint* paint)
        : image_decode_controller_(image_decode_controller),
          paint_(paint),
          draw_image_(image,
                      RoundOutRect(src_rect),
                      scale,
                      paint ? paint->getFilterQuality() : kNone_SkFilterQuality,
                      has_perspective,
                      is_decomposable),
          decoded_draw_image_(
              image_decode_controller_->GetDecodedImageForDraw(draw_image_)) {
      DCHECK(image->isLazyGenerated());
      if (paint) {
        decoded_paint_ = *paint;
        decoded_paint_.setFilterQuality(decoded_draw_image_.filter_quality());
      }
    }

    ~ScopedDecodedImageLock() {
      image_decode_controller_->DrawWithImageFinished(draw_image_,
                                                      decoded_draw_image_);
    }

    const DecodedDrawImage& decoded_image() const {
      return decoded_draw_image_;
    }
    const SkPaint* decoded_paint() const {
      return paint_ ? &decoded_paint_ : nullptr;
    }

   private:
    ImageDecodeController* image_decode_controller_;
    const SkPaint* paint_;
    DrawImage draw_image_;
    DecodedDrawImage decoded_draw_image_;
    SkPaint decoded_paint_;
  };

  ImageDecodeController* image_decode_controller_;
};

}  // namespace

scoped_refptr<DisplayListRasterSource>
DisplayListRasterSource::CreateFromDisplayListRecordingSource(
    const DisplayListRecordingSource* other,
    bool can_use_lcd_text) {
  return make_scoped_refptr(
      new DisplayListRasterSource(other, can_use_lcd_text));
}

DisplayListRasterSource::DisplayListRasterSource(
    const DisplayListRecordingSource* other,
    bool can_use_lcd_text)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(false),
      image_decode_controller_(nullptr) {}

DisplayListRasterSource::DisplayListRasterSource(
    const DisplayListRasterSource* other,
    bool can_use_lcd_text)
    : display_list_(other->display_list_),
      painter_reported_memory_usage_(other->painter_reported_memory_usage_),
      background_color_(other->background_color_),
      requires_clear_(other->requires_clear_),
      can_use_lcd_text_(can_use_lcd_text),
      is_solid_color_(other->is_solid_color_),
      solid_color_(other->solid_color_),
      recorded_viewport_(other->recorded_viewport_),
      size_(other->size_),
      clear_canvas_with_debug_color_(other->clear_canvas_with_debug_color_),
      slow_down_raster_scale_factor_for_debug_(
          other->slow_down_raster_scale_factor_for_debug_),
      should_attempt_to_use_distance_field_text_(
          other->should_attempt_to_use_distance_field_text_),
      image_decode_controller_(other->image_decode_controller_) {}

DisplayListRasterSource::~DisplayListRasterSource() {
}

void DisplayListRasterSource::PlaybackToSharedCanvas(
    SkCanvas* raster_canvas,
    const gfx::Rect& canvas_rect,
    float contents_scale) const {
  // TODO(vmpstr): This can be improved by plumbing whether the tile itself has
  // discardable images. This way we would only pay for the hijack canvas if the
  // tile actually needed it.
  if (display_list_->MayHaveDiscardableImages()) {
    const SkImageInfo& info = raster_canvas->imageInfo();
    ImageHijackCanvas canvas(info.width(), info.height(),
                             image_decode_controller_);
    canvas.addCanvas(raster_canvas);

    RasterCommon(&canvas, nullptr, canvas_rect, canvas_rect, contents_scale);
  } else {
    RasterCommon(raster_canvas, nullptr, canvas_rect, canvas_rect,
                 contents_scale);
  }
}

void DisplayListRasterSource::RasterForAnalysis(skia::AnalysisCanvas* canvas,
                                                const gfx::Rect& canvas_rect,
                                                float contents_scale) const {
  RasterCommon(canvas, canvas, canvas_rect, canvas_rect, contents_scale);
}

void DisplayListRasterSource::PlaybackToCanvas(
    SkCanvas* raster_canvas,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    float contents_scale) const {
  PrepareForPlaybackToCanvas(raster_canvas, canvas_bitmap_rect,
                             canvas_playback_rect, contents_scale);

  SkImageInfo info = raster_canvas->imageInfo();
  ImageHijackCanvas canvas(info.width(), info.height(),
                           image_decode_controller_);
  canvas.addCanvas(raster_canvas);
  RasterCommon(&canvas, NULL, canvas_bitmap_rect, canvas_playback_rect,
               contents_scale);
}

void DisplayListRasterSource::PrepareForPlaybackToCanvas(
    SkCanvas* canvas,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    float contents_scale) const {
  // TODO(hendrikw): See if we can split this up into separate functions.
  bool partial_update = canvas_bitmap_rect != canvas_playback_rect;

  if (!partial_update)
    canvas->discard();
  if (clear_canvas_with_debug_color_) {
    // Any non-painted areas in the content bounds will be left in this color.
    if (!partial_update) {
      canvas->clear(DebugColors::NonPaintedFillColor());
    } else {
      canvas->save();
      canvas->clipRect(gfx::RectToSkRect(
          canvas_playback_rect - canvas_bitmap_rect.OffsetFromOrigin()));
      canvas->drawColor(DebugColors::NonPaintedFillColor());
      canvas->restore();
    }
  }

  // If this raster source has opaque contents, it is guaranteeing that it will
  // draw an opaque rect the size of the layer.  If it is not, then we must
  // clear this canvas ourselves.
  if (requires_clear_) {
    TRACE_EVENT_INSTANT0("cc", "SkCanvas::clear", TRACE_EVENT_SCOPE_THREAD);
    // Clearing is about ~4x faster than drawing a rect even if the content
    // isn't covering a majority of the canvas.
    if (!partial_update) {
      canvas->clear(SK_ColorTRANSPARENT);
    } else {
      canvas->save();
      canvas->clipRect(gfx::RectToSkRect(
          canvas_playback_rect - canvas_bitmap_rect.OffsetFromOrigin()));
      canvas->drawColor(SK_ColorTRANSPARENT, SkXfermode::kClear_Mode);
      canvas->restore();
    }
  } else {
    // Even if completely covered, for rasterizations that touch the edge of the
    // layer, we also need to raster the background color underneath the last
    // texel (since the recording won't cover it) and outside the last texel
    // (due to linear filtering when using this texture).
    gfx::Rect content_rect =
        gfx::ScaleToEnclosingRect(gfx::Rect(size_), contents_scale);

    // The final texel of content may only be partially covered by a
    // rasterization; this rect represents the content rect that is fully
    // covered by content.
    gfx::Rect deflated_content_rect = content_rect;
    deflated_content_rect.Inset(0, 0, 1, 1);
    deflated_content_rect.Intersect(canvas_playback_rect);
    if (!deflated_content_rect.Contains(canvas_playback_rect)) {
      if (clear_canvas_with_debug_color_) {
        // Any non-painted areas outside of the content bounds are left in
        // this color.  If this is seen then it means that cc neglected to
        // rerasterize a tile that used to intersect with the content rect
        // after the content bounds grew.
        canvas->save();
        canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
        canvas->clipRect(gfx::RectToSkRect(content_rect),
                         SkRegion::kDifference_Op);
        canvas->drawColor(DebugColors::MissingResizeInvalidations(),
                          SkXfermode::kSrc_Mode);
        canvas->restore();
      }

      // Drawing at most 2 x 2 x (canvas width + canvas height) texels is 2-3X
      // faster than clearing, so special case this.
      canvas->save();
      canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
      gfx::Rect inflated_content_rect = content_rect;
      // Only clear edges that will be inside the canvas_playback_rect, else we
      // clear things that are still valid from a previous raster.
      inflated_content_rect.Inset(0, 0, -1, -1);
      inflated_content_rect.Intersect(canvas_playback_rect);
      canvas->clipRect(gfx::RectToSkRect(inflated_content_rect),
                       SkRegion::kReplace_Op);
      canvas->clipRect(gfx::RectToSkRect(deflated_content_rect),
                       SkRegion::kDifference_Op);
      canvas->drawColor(background_color_, SkXfermode::kSrc_Mode);
      canvas->restore();
    }
  }
}

void DisplayListRasterSource::RasterCommon(
    SkCanvas* canvas,
    SkPicture::AbortCallback* callback,
    const gfx::Rect& canvas_bitmap_rect,
    const gfx::Rect& canvas_playback_rect,
    float contents_scale) const {
  canvas->translate(-canvas_bitmap_rect.x(), -canvas_bitmap_rect.y());
  gfx::Rect content_rect =
      gfx::ScaleToEnclosingRect(gfx::Rect(size_), contents_scale);
  content_rect.Intersect(canvas_playback_rect);

  canvas->clipRect(gfx::RectToSkRect(content_rect), SkRegion::kIntersect_Op);

  DCHECK(display_list_.get());
  gfx::Rect canvas_target_playback_rect =
      canvas_playback_rect - canvas_bitmap_rect.OffsetFromOrigin();
  int repeat_count = std::max(1, slow_down_raster_scale_factor_for_debug_);
  for (int i = 0; i < repeat_count; ++i) {
    display_list_->Raster(canvas, callback, canvas_target_playback_rect,
                          contents_scale);
  }
}

skia::RefPtr<SkPicture> DisplayListRasterSource::GetFlattenedPicture() {
  TRACE_EVENT0("cc", "DisplayListRasterSource::GetFlattenedPicture");

  gfx::Rect display_list_rect(size_);
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(display_list_rect.width(),
                                             display_list_rect.height());
  if (!display_list_rect.IsEmpty()) {
    PrepareForPlaybackToCanvas(canvas, display_list_rect, display_list_rect,
                               1.f);
    RasterCommon(canvas, nullptr, display_list_rect, display_list_rect, 1.f);
  }
  skia::RefPtr<SkPicture> picture =
      skia::AdoptRef(recorder.endRecordingAsPicture());

  return picture;
}

size_t DisplayListRasterSource::GetPictureMemoryUsage() const {
  if (!display_list_)
    return 0;
  return display_list_->ApproximateMemoryUsage() +
         painter_reported_memory_usage_;
}

bool DisplayListRasterSource::PerformSolidColorAnalysis(
    const gfx::Rect& content_rect,
    float contents_scale,
    SkColor* color) const {
  TRACE_EVENT0("cc", "DisplayListRasterSource::PerformSolidColorAnalysis");

  gfx::Rect layer_rect =
      gfx::ScaleToEnclosingRect(content_rect, 1.0f / contents_scale);

  layer_rect.Intersect(gfx::Rect(size_));
  skia::AnalysisCanvas canvas(layer_rect.width(), layer_rect.height());
  RasterForAnalysis(&canvas, layer_rect, 1.0f);
  return canvas.GetColorIfSolid(color);
}

void DisplayListRasterSource::GetDiscardableImagesInRect(
    const gfx::Rect& layer_rect,
    float raster_scale,
    std::vector<DrawImage>* images) const {
  DCHECK_EQ(0u, images->size());
  display_list_->GetDiscardableImagesInRect(layer_rect, raster_scale, images);
}

bool DisplayListRasterSource::HasDiscardableImageInRect(
    const gfx::Rect& layer_rect) const {
  return display_list_ && display_list_->HasDiscardableImageInRect(layer_rect);
}

bool DisplayListRasterSource::CoversRect(const gfx::Rect& layer_rect) const {
  if (size_.IsEmpty())
    return false;
  gfx::Rect bounded_rect = layer_rect;
  bounded_rect.Intersect(gfx::Rect(size_));
  return recorded_viewport_.Contains(bounded_rect);
}

gfx::Size DisplayListRasterSource::GetSize() const {
  return size_;
}

bool DisplayListRasterSource::IsSolidColor() const {
  return is_solid_color_;
}

SkColor DisplayListRasterSource::GetSolidColor() const {
  DCHECK(IsSolidColor());
  return solid_color_;
}

bool DisplayListRasterSource::HasRecordings() const {
  return !!display_list_.get();
}

gfx::Rect DisplayListRasterSource::RecordedViewport() const {
  return recorded_viewport_;
}

void DisplayListRasterSource::SetShouldAttemptToUseDistanceFieldText() {
  should_attempt_to_use_distance_field_text_ = true;
}

bool DisplayListRasterSource::ShouldAttemptToUseDistanceFieldText() const {
  return should_attempt_to_use_distance_field_text_;
}

void DisplayListRasterSource::AsValueInto(
    base::trace_event::TracedValue* array) const {
  if (display_list_.get())
    TracedValue::AppendIDRef(display_list_.get(), array);
}

void DisplayListRasterSource::DidBeginTracing() {
  if (display_list_.get())
    display_list_->EmitTraceSnapshot();
}

bool DisplayListRasterSource::CanUseLCDText() const {
  return can_use_lcd_text_;
}

scoped_refptr<DisplayListRasterSource>
DisplayListRasterSource::CreateCloneWithoutLCDText() const {
  bool can_use_lcd_text = false;
  return scoped_refptr<DisplayListRasterSource>(
      new DisplayListRasterSource(this, can_use_lcd_text));
}

void DisplayListRasterSource::SetImageDecodeController(
    ImageDecodeController* image_decode_controller) {
  DCHECK(image_decode_controller);
  // Note that although this function should only be called once, tests tend to
  // call it several times using the same controller.
  DCHECK(!image_decode_controller_ ||
         image_decode_controller_ == image_decode_controller);
  image_decode_controller_ = image_decode_controller;
}

}  // namespace cc
