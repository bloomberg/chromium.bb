// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/discardable_image_map.h"

#include <stddef.h>

#include <algorithm>
#include <limits>

#include "base/containers/adapters.h"
#include "base/memory/ptr_util.h"
#include "cc/base/math_util.h"
#include "cc/playback/display_item_list.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

// Returns a rect clamped to |max_size|. Note that |paint_rect| should intersect
// or be contained by a rect defined by (0, 0) and |max_size|.
gfx::Rect SafeClampPaintRectToSize(const SkRect& paint_rect,
                                   const gfx::Size& max_size) {
  // bounds_rect.x() + bounds_rect.width() (aka bounds_rect.right()) might
  // overflow integer bounds, so do custom intersect, since gfx::Rect::Intersect
  // uses bounds_rect.right().
  gfx::RectF bounds_rect = gfx::SkRectToRectF(paint_rect);
  float x_offset_if_negative = bounds_rect.x() < 0.f ? bounds_rect.x() : 0.f;
  float y_offset_if_negative = bounds_rect.y() < 0.f ? bounds_rect.y() : 0.f;
  bounds_rect.set_x(std::max(0.f, bounds_rect.x()));
  bounds_rect.set_y(std::max(0.f, bounds_rect.y()));

  // Verify that the rects intersect or that bound_rect is contained by
  // max_size.
  DCHECK_GE(bounds_rect.width(), -x_offset_if_negative);
  DCHECK_GE(bounds_rect.height(), -y_offset_if_negative);
  DCHECK_GE(max_size.width(), bounds_rect.x());
  DCHECK_GE(max_size.height(), bounds_rect.y());

  bounds_rect.set_width(std::min(bounds_rect.width() + x_offset_if_negative,
                                 max_size.width() - bounds_rect.x()));
  bounds_rect.set_height(std::min(bounds_rect.height() + y_offset_if_negative,
                                  max_size.height() - bounds_rect.y()));
  return gfx::ToEnclosingRect(bounds_rect);
}

namespace {

// We're using an NWay canvas with no added canvases, so in effect
// non-overridden functions are no-ops.
class DiscardableImagesMetadataCanvas : public SkNWayCanvas {
 public:
  DiscardableImagesMetadataCanvas(
      int width,
      int height,
      std::vector<std::pair<DrawImage, gfx::Rect>>* image_set,
      std::unordered_map<ImageId, gfx::Rect>* image_id_to_rect)
      : SkNWayCanvas(width, height),
        image_set_(image_set),
        image_id_to_rect_(image_id_to_rect),
        canvas_bounds_(SkRect::MakeIWH(width, height)),
        canvas_size_(width, height) {}

 protected:
  // we need to "undo" the behavior of SkNWayCanvas, which will try to forward
  // it.
  void onDrawPicture(const SkPicture* picture,
                     const SkMatrix* matrix,
                     const SkPaint* paint) override {
    SkCanvas::onDrawPicture(picture, matrix, paint);
  }

  void onDrawImage(const SkImage* image,
                   SkScalar x,
                   SkScalar y,
                   const SkPaint* paint) override {
    const SkMatrix& ctm = getTotalMatrix();
    AddImage(
        sk_ref_sp(image), SkRect::MakeIWH(image->width(), image->height()),
        MapRect(ctm, SkRect::MakeXYWH(x, y, image->width(), image->height())),
        ctm, paint);
  }

  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint) override {
    const SkMatrix& ctm = getTotalMatrix();
    SkRect src_storage;
    if (!src) {
      src_storage = SkRect::MakeIWH(image->width(), image->height());
      src = &src_storage;
    }
    SkMatrix matrix;
    matrix.setRectToRect(*src, dst, SkMatrix::kFill_ScaleToFit);
    matrix.preConcat(ctm);
    AddImage(sk_ref_sp(image), *src, MapRect(ctm, dst), matrix, paint);
  }

  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override {
    // No cc embedder issues image nine calls.
    NOTREACHED();
  }

  void onDrawRect(const SkRect& r, const SkPaint& paint) override {
    AddPaintImage(r, paint);
  }

  void onDrawPath(const SkPath& path, const SkPaint& paint) override {
    AddPaintImage(path.getBounds(), paint);
  }

  void onDrawOval(const SkRect& r, const SkPaint& paint) override {
    AddPaintImage(r, paint);
  }

  void onDrawArc(const SkRect& r,
                 SkScalar start_angle,
                 SkScalar sweep_angle,
                 bool use_center,
                 const SkPaint& paint) override {
    AddPaintImage(r, paint);
  }

  void onDrawRRect(const SkRRect& rr, const SkPaint& paint) override {
    AddPaintImage(rr.rect(), paint);
  }

  SaveLayerStrategy getSaveLayerStrategy(const SaveLayerRec& rec) override {
    saved_paints_.push_back(*rec.fPaint);
    return SkNWayCanvas::getSaveLayerStrategy(rec);
  }

  void willSave() override {
    saved_paints_.push_back(SkPaint());
    return SkNWayCanvas::willSave();
  }

  void willRestore() override {
    DCHECK_GT(saved_paints_.size(), 0u);
    saved_paints_.pop_back();
    SkNWayCanvas::willRestore();
  }

 private:
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

  void AddImage(sk_sp<const SkImage> image,
                const SkRect& src_rect,
                const SkRect& rect,
                const SkMatrix& matrix,
                const SkPaint* paint) {
    if (!image->isLazyGenerated())
      return;

    SkRect paint_rect;
    bool computed_paint_bounds = ComputePaintBounds(rect, paint, &paint_rect);
    if (!computed_paint_bounds) {
      // TODO(vmpstr): UMA this case.
      paint_rect = canvas_bounds_;
    }

    if (!paint_rect.intersects(canvas_bounds_))
      return;

    SkFilterQuality filter_quality = kNone_SkFilterQuality;
    if (paint) {
      filter_quality = paint->getFilterQuality();
    }

    SkIRect src_irect;
    src_rect.roundOut(&src_irect);
    gfx::Rect image_rect = SafeClampPaintRectToSize(paint_rect, canvas_size_);

    (*image_id_to_rect_)[image->uniqueID()].Union(image_rect);
    image_set_->push_back(std::make_pair(
        DrawImage(std::move(image), src_irect, filter_quality, matrix),
        image_rect));
  }

  // Currently this function only handles extracting images from SkImageShaders
  // embedded in SkPaints. Other embedded image cases, such as SkPictures,
  // are not yet handled.
  void AddPaintImage(const SkRect& rect, const SkPaint& paint) {
    SkShader* shader = paint.getShader();
    if (shader) {
      SkMatrix matrix;
      SkShader::TileMode xy[2];
      SkImage* image = shader->isAImage(&matrix, xy);
      if (image) {
        const SkMatrix& ctm = getTotalMatrix();
        matrix.postConcat(ctm);
        // TODO(ericrk): Handle cases where we only need a sub-rect from the
        // image. crbug.com/671821
        AddImage(sk_ref_sp(image), SkRect::MakeFromIRect(image->bounds()),
                 MapRect(ctm, rect), matrix, &paint);
      }
    }
  }

  std::vector<std::pair<DrawImage, gfx::Rect>>* image_set_;
  std::unordered_map<ImageId, gfx::Rect>* image_id_to_rect_;
  const SkRect canvas_bounds_;
  const gfx::Size canvas_size_;
  std::vector<SkPaint> saved_paints_;
};

}  // namespace

DiscardableImageMap::DiscardableImageMap() {}

DiscardableImageMap::~DiscardableImageMap() {}

std::unique_ptr<SkCanvas> DiscardableImageMap::BeginGeneratingMetadata(
    const gfx::Size& bounds) {
  DCHECK(all_images_.empty());
  return base::MakeUnique<DiscardableImagesMetadataCanvas>(
      bounds.width(), bounds.height(), &all_images_, &image_id_to_rect_);
}

void DiscardableImageMap::EndGeneratingMetadata() {
  images_rtree_.Build(all_images_,
                      [](const std::pair<DrawImage, gfx::Rect>& image) {
                        return image.second;
                      });
}

void DiscardableImageMap::GetDiscardableImagesInRect(
    const gfx::Rect& rect,
    float contents_scale,
    std::vector<DrawImage>* images) const {
  std::vector<size_t> indices;
  images_rtree_.Search(rect, &indices);
  for (size_t index : indices)
    images->push_back(all_images_[index].first.ApplyScale(contents_scale));
}

gfx::Rect DiscardableImageMap::GetRectForImage(ImageId image_id) const {
  const auto& it = image_id_to_rect_.find(image_id);
  return it == image_id_to_rect_.end() ? gfx::Rect() : it->second;
}

DiscardableImageMap::ScopedMetadataGenerator::ScopedMetadataGenerator(
    DiscardableImageMap* image_map,
    const gfx::Size& bounds)
    : image_map_(image_map),
      metadata_canvas_(image_map->BeginGeneratingMetadata(bounds)) {}

DiscardableImageMap::ScopedMetadataGenerator::~ScopedMetadataGenerator() {
  image_map_->EndGeneratingMetadata();
}

}  // namespace cc
