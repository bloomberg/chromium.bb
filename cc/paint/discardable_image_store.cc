// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/discardable_image_store.h"

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "cc/paint/display_item_list.h"
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

}  // namespace

class DiscardableImageStore::PaintTrackingCanvas : public SkNoDrawCanvas {
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

 protected:
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

 private:
  std::vector<SkPaint> saved_paints_;
};

DiscardableImageStore::DiscardableImageStore(
    int width,
    int height,
    std::vector<std::pair<DrawImage, gfx::Rect>>* image_set,
    base::flat_map<PaintImage::Id, gfx::Rect>* image_id_to_rect)
    : canvas_(base::MakeUnique<PaintTrackingCanvas>(width, height)),
      image_set_(image_set),
      image_id_to_rect_(image_id_to_rect) {}

DiscardableImageStore::~DiscardableImageStore() = default;

SkNoDrawCanvas* DiscardableImageStore::GetNoDrawCanvas() {
  return canvas_.get();
}

void DiscardableImageStore::GatherDiscardableImages(
    const PaintOpBuffer* buffer) {
  if (!buffer->HasDiscardableImages())
    return;

  SkMatrix original = canvas_->getTotalMatrix();
  canvas_->save();
  // TODO(khushalsagar): Optimize out save/restore blocks if there are no images
  // in the draw ops between them.
  for (auto* op : PaintOpBuffer::Iterator(buffer)) {
    if (op->IsDrawOp()) {
      switch (op->GetType()) {
        case PaintOpType::DrawArc: {
          auto* arc_op = static_cast<DrawArcOp*>(op);
          AddImageFromFlags(arc_op->oval, arc_op->flags);
        } break;
        case PaintOpType::DrawCircle: {
          auto* circle_op = static_cast<DrawCircleOp*>(op);
          SkRect rect =
              SkRect::MakeXYWH(circle_op->cx - circle_op->radius,
                               circle_op->cy - circle_op->radius,
                               2 * circle_op->radius, 2 * circle_op->radius);
          AddImageFromFlags(rect, circle_op->flags);
        } break;
        case PaintOpType::DrawImage: {
          auto* image_op = static_cast<DrawImageOp*>(op);
          const SkImage* sk_image = image_op->image.sk_image().get();
          AddImage(image_op->image,
                   SkRect::MakeIWH(sk_image->width(), sk_image->height()),
                   SkRect::MakeXYWH(image_op->left, image_op->top,
                                    sk_image->width(), sk_image->height()),
                   nullptr, image_op->flags);
        } break;
        case PaintOpType::DrawImageRect: {
          auto* image_rect_op = static_cast<DrawImageRectOp*>(op);
          SkMatrix matrix;
          matrix.setRectToRect(image_rect_op->src, image_rect_op->dst,
                               SkMatrix::kFill_ScaleToFit);
          AddImage(image_rect_op->image, image_rect_op->src, image_rect_op->dst,
                   &matrix, image_rect_op->flags);
        } break;
        case PaintOpType::DrawIRect: {
          auto* rect_op = static_cast<DrawIRectOp*>(op);
          AddImageFromFlags(SkRect::Make(rect_op->rect), rect_op->flags);
        } break;
        case PaintOpType::DrawOval: {
          auto* oval_op = static_cast<DrawOvalOp*>(op);
          AddImageFromFlags(oval_op->oval, oval_op->flags);
        } break;
        case PaintOpType::DrawPath: {
          auto* path_op = static_cast<DrawPathOp*>(op);
          AddImageFromFlags(path_op->path.getBounds(), path_op->flags);
        } break;
        case PaintOpType::DrawRecord: {
          auto* record_op = static_cast<DrawRecordOp*>(op);
          GatherDiscardableImages(record_op->record.get());
        } break;
        case PaintOpType::DrawRect: {
          auto* rect_op = static_cast<DrawRectOp*>(op);
          AddImageFromFlags(rect_op->rect, rect_op->flags);
        } break;
        case PaintOpType::DrawRRect: {
          auto* rect_op = static_cast<DrawRRectOp*>(op);
          AddImageFromFlags(rect_op->rrect.rect(), rect_op->flags);
        } break;
        // TODO(khushalsagar): Check if we should be querying images from any of
        // the following ops.
        case PaintOpType::DrawPosText:
        case PaintOpType::DrawLine:
        case PaintOpType::DrawDRRect:
        case PaintOpType::DrawText:
        case PaintOpType::DrawTextBlob:
        case PaintOpType::DrawColor:
          break;
        default:
          NOTREACHED();
      }
    } else {
      op->Raster(canvas_.get(), original);
    }
  }
  canvas_->restore();
}

// Currently this function only handles extracting images from SkImageShaders
// embedded in SkPaints. Other embedded image cases, such as SkPictures,
// are not yet handled.
void DiscardableImageStore::AddImageFromFlags(const SkRect& rect,
                                              const PaintFlags& flags) {
  SkShader* shader = flags.getSkShader();
  if (shader) {
    SkMatrix matrix;
    SkShader::TileMode xy[2];
    SkImage* image = shader->isAImage(&matrix, xy);
    if (image) {
      // We currently use the wrong id for images that come from shaders. We
      // don't know what the stable id is, but since the completion and
      // animation states are both unknown, this value doesn't matter as it
      // won't be used in checker imaging anyway. Keep this value the same to
      // avoid id churn.
      // TODO(vmpstr): Remove this when we can add paint images into shaders
      // directly.
      PaintImage paint_image(PaintImage::kUnknownStableId, sk_ref_sp(image),
                             PaintImage::AnimationType::UNKNOWN,
                             PaintImage::CompletionState::UNKNOWN);
      // TODO(ericrk): Handle cases where we only need a sub-rect from the
      // image. crbug.com/671821
      AddImage(std::move(paint_image), SkRect::MakeFromIRect(image->bounds()),
               rect, &matrix, flags);
    }
  }
}

void DiscardableImageStore::AddImage(PaintImage paint_image,
                                     const SkRect& src_rect,
                                     const SkRect& rect,
                                     const SkMatrix* local_matrix,
                                     const PaintFlags& flags) {
  if (!paint_image.sk_image()->isLazyGenerated())
    return;

  const SkRect& clip_rect = SkRect::Make(canvas_->getDeviceClipBounds());
  const SkMatrix& ctm = canvas_->getTotalMatrix();

  SkRect paint_rect = MapRect(ctm, rect);
  bool computed_paint_bounds =
      canvas_->ComputePaintBounds(paint_rect, ToSkPaint(&flags), &paint_rect);
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

  // The true target color space will be assigned when it is known, in
  // GetDiscardableImagesInRect.
  gfx::ColorSpace target_color_space;

  SkMatrix matrix = ctm;
  if (local_matrix)
    matrix.postConcat(*local_matrix);

  (*image_id_to_rect_)[paint_image.stable_id()].Union(image_rect);
  image_set_->push_back(
      std::make_pair(DrawImage(std::move(paint_image), src_irect,
                               filter_quality, matrix, target_color_space),
                     image_rect));
}

}  // namespace cc
