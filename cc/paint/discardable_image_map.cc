// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_op_buffer.h"
#include "third_party/skia/include/utils/SkNoDrawCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

class PaintTrackingCanvas final : public SkNoDrawCanvas {
 public:
  PaintTrackingCanvas(int width, int height) : SkNoDrawCanvas(width, height) {}
  ~PaintTrackingCanvas() override = default;

  bool ComputePaintBounds(const SkRect& rect,
                          const SkPaint* current_paint,
                          SkRect* paint_bounds) {
    *paint_bounds = rect;
    if (current_paint) {
      if (!current_paint->canComputeFastBounds())
        return false;
      *paint_bounds =
          current_paint->computeFastBounds(*paint_bounds, paint_bounds);
    }

    for (const auto& paint : base::Reversed(saved_paints_)) {
      if (!paint.canComputeFastBounds())
        return false;
      *paint_bounds = paint.computeFastBounds(*paint_bounds, paint_bounds);
    }

    return true;
  }

 private:
  // SkNoDrawCanvas overrides.
  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
    saved_paints_.push_back(rec.fPaint ? *rec.fPaint : SkPaint());
    return SkNoDrawCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    saved_paints_.push_back(SkPaint());
    return SkNoDrawCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(saved_paints_.size(), 0u);
    saved_paints_.pop_back();
    SkNoDrawCanvas::willRestore();
  }

  std::vector<SkPaint> saved_paints_;
};

class DiscardableImageGenerator {
 public:
  DiscardableImageGenerator(int width, int height) : canvas_(width, height) {}
  ~DiscardableImageGenerator() = default;

  void GatherDiscardableImages(const PaintOpBuffer* buffer) {
    if (!buffer->HasDiscardableImages())
      return;

    PlaybackParams params(nullptr, canvas_.getTotalMatrix());
    canvas_.save();
    // TODO(khushalsagar): Optimize out save/restore blocks if there are no
    // images in the draw ops between them.
    for (auto* op : PaintOpBuffer::Iterator(buffer)) {
      if (op->IsDrawOp()) {
        SkRect op_rect;
        if (op->IsPaintOpWithFlags() && PaintOp::GetBounds(op, &op_rect)) {
          AddImageFromFlags(op_rect,
                            static_cast<const PaintOpWithFlags*>(op)->flags);
        }

        PaintOpType op_type = static_cast<PaintOpType>(op->type);
        if (op_type == PaintOpType::DrawImage) {
          auto* image_op = static_cast<DrawImageOp*>(op);
          auto* sk_image = image_op->image.GetSkImage().get();
          AddImage(image_op->image,
                   SkRect::MakeIWH(sk_image->width(), sk_image->height()),
                   SkRect::MakeXYWH(image_op->left, image_op->top,
                                    sk_image->width(), sk_image->height()),
                   nullptr, image_op->flags);
        } else if (op_type == PaintOpType::DrawImageRect) {
          auto* image_rect_op = static_cast<DrawImageRectOp*>(op);
          SkMatrix matrix;
          matrix.setRectToRect(image_rect_op->src, image_rect_op->dst,
                               SkMatrix::kFill_ScaleToFit);
          AddImage(image_rect_op->image, image_rect_op->src, image_rect_op->dst,
                   &matrix, image_rect_op->flags);
        } else if (op_type == PaintOpType::DrawRecord) {
          GatherDiscardableImages(
              static_cast<const DrawRecordOp*>(op)->record.get());
        }
      } else {
        op->Raster(&canvas_, params);
      }
    }
    canvas_.restore();
  }

  std::vector<std::pair<DrawImage, gfx::Rect>> TakeImages() {
    return std::move(image_set_);
  }
  base::flat_map<PaintImage::Id, gfx::Rect> TakeImageIdToRectMap() {
    return std::move(image_id_to_rect_);
  }
  std::vector<DiscardableImageMap::AnimatedImageMetadata>
  TakeAnimatedImagesMetadata() {
    return std::move(animated_images_metadata_);
  }

  void RecordColorHistograms() const {
    if (color_stats_total_image_count_ > 0) {
      int srgb_image_percent = (100 * color_stats_srgb_image_count_) /
                               color_stats_total_image_count_;
      UMA_HISTOGRAM_PERCENTAGE("Renderer4.ImagesPercentSRGB",
                               srgb_image_percent);
    }

    base::CheckedNumeric<int> srgb_pixel_percent =
        100 * color_stats_srgb_pixel_count_ / color_stats_total_pixel_count_;
    if (srgb_pixel_percent.IsValid()) {
      UMA_HISTOGRAM_PERCENTAGE("Renderer4.ImagePixelsPercentSRGB",
                               srgb_pixel_percent.ValueOrDie());
    }
  }

  bool all_images_are_srgb() const {
    return color_stats_srgb_image_count_ == color_stats_total_image_count_;
  }

 private:
  void AddImageFromFlags(const SkRect& rect, const PaintFlags& flags) {
    if (!flags.HasShader() ||
        flags.getShader()->shader_type() != PaintShader::Type::kImage)
      return;

    const PaintImage& paint_image = flags.getShader()->paint_image();
    SkMatrix local_matrix = flags.getShader()->GetLocalMatrix();
    AddImage(paint_image,
             SkRect::MakeWH(paint_image.width(), paint_image.height()), rect,
             &local_matrix, flags);
  }

  void AddImage(PaintImage paint_image,
                const SkRect& src_rect,
                const SkRect& rect,
                const SkMatrix* local_matrix,
                const PaintFlags& flags) {
    if (!paint_image.IsLazyGenerated())
      return;

    const SkRect& clip_rect = SkRect::Make(canvas_.getDeviceClipBounds());
    const SkMatrix& ctm = canvas_.getTotalMatrix();

    SkRect paint_rect = MapRect(ctm, rect);
    SkPaint paint = flags.ToSkPaint();
    bool computed_paint_bounds =
        canvas_.ComputePaintBounds(paint_rect, &paint, &paint_rect);
    if (!computed_paint_bounds) {
      // TODO(vmpstr): UMA this case.
      paint_rect = clip_rect;
    }

    // Clamp the image rect by the current clip rect.
    if (!paint_rect.intersect(clip_rect))
      return;

    SkFilterQuality filter_quality = flags.getFilterQuality();

    SkIRect src_irect;
    src_rect.roundOut(&src_irect);
    gfx::Rect image_rect = gfx::ToEnclosingRect(gfx::SkRectToRectF(paint_rect));

    // During raster, we use the device clip bounds on the canvas, which outsets
    // the actual clip by 1 due to the possibility of antialiasing. Account for
    // this here by outsetting the image rect by 1. Note that this only affects
    // queries into the rtree, which will now return images that only touch the
    // bounds of the query rect.
    //
    // Note that it's not sufficient for us to inset the device clip bounds at
    // raster time, since we might be sending a larger-than-one-item display
    // item to skia, which means that skia will internally determine whether to
    // raster the picture (using device clip bounds that are outset).
    image_rect.Inset(-1, -1);

    // Make a note if any image was originally specified in a non-sRGB color
    // space.
    SkColorSpace* source_color_space = paint_image.color_space();
    color_stats_total_pixel_count_ += image_rect.size().GetCheckedArea();
    color_stats_total_image_count_++;
    if (!source_color_space || source_color_space->isSRGB()) {
      color_stats_srgb_pixel_count_ += image_rect.size().GetCheckedArea();
      color_stats_srgb_image_count_++;
    }

    SkMatrix matrix = ctm;
    if (local_matrix)
      matrix.postConcat(*local_matrix);

    image_id_to_rect_[paint_image.stable_id()].Union(image_rect);

    if (paint_image.ShouldAnimate()) {
      animated_images_metadata_.emplace_back(
          paint_image.stable_id(), paint_image.completion_state(),
          paint_image.GetFrameMetadata(), paint_image.repetition_count(),
          paint_image.reset_animation_sequence_id());
    }

    image_set_.emplace_back(
        DrawImage(std::move(paint_image), src_irect, filter_quality, matrix),
        image_rect);
  }

  // This canvas is used only for tracking transform/clip/filter state from the
  // non-drawing ops.
  PaintTrackingCanvas canvas_;
  std::vector<std::pair<DrawImage, gfx::Rect>> image_set_;
  base::flat_map<PaintImage::Id, gfx::Rect> image_id_to_rect_;
  std::vector<DiscardableImageMap::AnimatedImageMetadata>
      animated_images_metadata_;

  // Statistics about the number of images and pixels that will require color
  // conversion if the target color space is not sRGB.
  int color_stats_srgb_image_count_ = 0;
  int color_stats_total_image_count_ = 0;
  base::CheckedNumeric<int64_t> color_stats_srgb_pixel_count_ = 0;
  base::CheckedNumeric<int64_t> color_stats_total_pixel_count_ = 0;
};

}  // namespace

DiscardableImageMap::DiscardableImageMap() = default;
DiscardableImageMap::~DiscardableImageMap() = default;

void DiscardableImageMap::Generate(const PaintOpBuffer* paint_op_buffer,
                                   const gfx::Rect& bounds) {
  TRACE_EVENT0("cc", "DiscardableImageMap::Generate");

  if (!paint_op_buffer->HasDiscardableImages())
    return;

  DiscardableImageGenerator generator(bounds.right(), bounds.bottom());
  generator.GatherDiscardableImages(paint_op_buffer);
  generator.RecordColorHistograms();
  image_id_to_rect_ = generator.TakeImageIdToRectMap();
  animated_images_metadata_ = generator.TakeAnimatedImagesMetadata();
  all_images_are_srgb_ = generator.all_images_are_srgb();
  auto images = generator.TakeImages();
  images_rtree_.Build(
      images,
      [](const std::vector<std::pair<DrawImage, gfx::Rect>>& items,
         size_t index) { return items[index].second; },
      [](const std::vector<std::pair<DrawImage, gfx::Rect>>& items,
         size_t index) { return items[index].first; });
}

void DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    std::vector<const DrawImage*>* images) const {
  *images = images_rtree_.SearchRefs(rect);
}

gfx::Rect DiscardableImageMap::GetRectForImage(PaintImage::Id image_id) const {
  const auto& it = image_id_to_rect_.find(image_id);
  return it == image_id_to_rect_.end() ? gfx::Rect() : it->second;
}

void DiscardableImageMap::Reset() {
  image_id_to_rect_.clear();
  image_id_to_rect_.shrink_to_fit();
  images_rtree_.Reset();
}

DiscardableImageMap::AnimatedImageMetadata::AnimatedImageMetadata(
    PaintImage::Id paint_image_id,
    PaintImage::CompletionState completion_state,
    std::vector<FrameMetadata> frames,
    int repetition_count,
    PaintImage::AnimationSequenceId reset_animation_sequence_id)
    : paint_image_id(paint_image_id),
      completion_state(completion_state),
      frames(std::move(frames)),
      repetition_count(repetition_count),
      reset_animation_sequence_id(reset_animation_sequence_id) {}

DiscardableImageMap::AnimatedImageMetadata::~AnimatedImageMetadata() = default;

DiscardableImageMap::AnimatedImageMetadata::AnimatedImageMetadata(
    const AnimatedImageMetadata& other) = default;

}  // namespace cc
