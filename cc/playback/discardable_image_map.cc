// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/playback/discardable_image_map.h"

#include <algorithm>
#include <limits>

#include "cc/base/math_util.h"
#include "cc/playback/display_item_list.h"
#include "third_party/skia/include/utils/SkNWayCanvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

SkRect MapRect(const SkMatrix& matrix, const SkRect& src) {
  SkRect dst;
  matrix.mapRect(&dst, src);
  return dst;
}

// We're using an NWay canvas with no added canvases, so in effect
// non-overridden functions are no-ops.
class GatherDiscardableImageCanvas : public SkNWayCanvas {
 public:
  GatherDiscardableImageCanvas(int width,
                               int height,
                               std::vector<PositionImage>* image_set)
      : SkNWayCanvas(width, height),
        image_set_(image_set),
        canvas_bounds_(SkRect::MakeIWH(width, height)) {}

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
    const SkMatrix& ctm = this->getTotalMatrix();
    AddImage(image, MapRect(ctm, SkRect::MakeXYWH(x, y, image->width(),
                                                  image->height())),
             ctm, paint);
  }

  void onDrawImageRect(const SkImage* image,
                       const SkRect* src,
                       const SkRect& dst,
                       const SkPaint* paint,
                       SrcRectConstraint) override {
    const SkMatrix& ctm = this->getTotalMatrix();
    SkRect src_storage;
    if (!src) {
      src_storage = SkRect::MakeIWH(image->width(), image->height());
      src = &src_storage;
    }
    SkMatrix matrix;
    matrix.setRectToRect(*src, dst, SkMatrix::kFill_ScaleToFit);
    matrix.postConcat(ctm);
    AddImage(image, MapRect(ctm, dst), matrix, paint);
  }

  void onDrawImageNine(const SkImage* image,
                       const SkIRect& center,
                       const SkRect& dst,
                       const SkPaint* paint) override {
    AddImage(image, dst, this->getTotalMatrix(), paint);
  }

 private:
  void AddImage(const SkImage* image,
                const SkRect& rect,
                const SkMatrix& matrix,
                const SkPaint* paint) {
    if (rect.intersects(canvas_bounds_) && image->isLazyGenerated()) {
      SkFilterQuality filter_quality = kNone_SkFilterQuality;
      if (paint) {
        filter_quality = paint->getFilterQuality();
      }
      image_set_->push_back(PositionImage(image, rect, matrix, filter_quality));
    }
  }

  std::vector<PositionImage>* image_set_;
  const SkRect canvas_bounds_;
};

}  // namespace

DiscardableImageMap::DiscardableImageMap(const gfx::Size& cell_size)
    : cell_size_(cell_size) {
  DCHECK(!cell_size.IsEmpty());
}

DiscardableImageMap::~DiscardableImageMap() {}

void DiscardableImageMap::GenerateDiscardableImagesMetadata(
    SkPicture* picture,
    const gfx::Rect& layer_rect) {
  DCHECK(picture);

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = 0;
  int max_y = 0;

  std::vector<PositionImage> images;
  {
    SkIRect picture_bounds = picture->cullRect().roundOut();
    GatherDiscardableImageCanvas canvas(picture_bounds.right(),
                                        picture_bounds.bottom(), &images);
    canvas.drawPicture(picture);
  }

  for (auto& position_image : images) {
    // The image rect is in space relative to the picture, but it can extend far
    // beyond the picture itself (since it represents the rect of actual image
    // contained within the picture, not clipped to picture bounds). We only
    // care about image queries that intersect the picture, so insert only into
    // the intersection of the two rects.
    gfx::Rect rect_clipped_to_picture = gfx::IntersectRects(
        gfx::ToEnclosingRect(gfx::SkRectToRectF(position_image.image_rect)),
        gfx::Rect(layer_rect.size()));

    gfx::Point min(MathUtil::UncheckedRoundDown(rect_clipped_to_picture.x(),
                                                cell_size_.width()),
                   MathUtil::UncheckedRoundDown(rect_clipped_to_picture.y(),
                                                cell_size_.height()));
    gfx::Point max(MathUtil::UncheckedRoundDown(rect_clipped_to_picture.right(),
                                                cell_size_.width()),
                   MathUtil::UncheckedRoundDown(
                       rect_clipped_to_picture.bottom(), cell_size_.height()));

    // We recorded the picture as if it was at (0, 0) by translating by layer
    // rect origin. Add the rect origin back here. It really doesn't make much
    // of a difference, since the query for pixel refs doesn't use this
    // information. However, since picture pile / display list also returns this
    // information, it would be nice to express it relative to the layer, not
    // relative to the particular implementation of the raster source.
    position_image.image_rect.offset(layer_rect.x(), layer_rect.y());

    for (int y = min.y(); y <= max.y(); y += cell_size_.height()) {
      for (int x = min.x(); x <= max.x(); x += cell_size_.width()) {
        ImageMapKey key(x, y);
        data_map_[key].push_back(position_image);
      }
    }

    min_x = std::min(min_x, min.x());
    min_y = std::min(min_y, min.y());
    max_x = std::max(max_x, max.x());
    max_y = std::max(max_y, max.y());
  }

  min_pixel_cell_ = gfx::Point(min_x, min_y);
  max_pixel_cell_ = gfx::Point(max_x, max_y);
}

base::LazyInstance<DiscardableImageMap::Images>
    DiscardableImageMap::Iterator::empty_images_;

DiscardableImageMap::Iterator::Iterator()
    : target_image_map_(NULL),
      current_images_(empty_images_.Pointer()),
      current_index_(0),
      min_point_(-1, -1),
      max_point_(-1, -1),
      current_x_(0),
      current_y_(0) {}

DiscardableImageMap::Iterator::Iterator(const gfx::Rect& rect,
                                        const DisplayItemList* display_list)
    : target_image_map_(display_list->images_.get()),
      current_images_(empty_images_.Pointer()),
      current_index_(0) {
  map_layer_rect_ = display_list->layer_rect_;
  PointToFirstImage(rect);
}

DiscardableImageMap::Iterator::~Iterator() {}

DiscardableImageMap::Iterator& DiscardableImageMap::Iterator::operator++() {
  ++current_index_;
  // If we're not at the end of the list, then we have the next item.
  if (current_index_ < current_images_->size())
    return *this;

  DCHECK(current_y_ <= max_point_.y());
  while (true) {
    gfx::Size cell_size = target_image_map_->cell_size_;

    // Advance the current grid cell.
    current_x_ += cell_size.width();
    if (current_x_ > max_point_.x()) {
      current_y_ += cell_size.height();
      current_x_ = min_point_.x();
      if (current_y_ > max_point_.y()) {
        current_images_ = empty_images_.Pointer();
        current_index_ = 0;
        break;
      }
    }

    // If there are no pixel refs at this grid cell, keep incrementing.
    ImageMapKey key(current_x_, current_y_);
    ImageMap::const_iterator iter = target_image_map_->data_map_.find(key);
    if (iter == target_image_map_->data_map_.end())
      continue;

    // We found a non-empty list: store it and get the first pixel ref.
    current_images_ = &iter->second;
    current_index_ = 0;
    break;
  }
  return *this;
}

void DiscardableImageMap::Iterator::PointToFirstImage(const gfx::Rect& rect) {
  gfx::Rect query_rect(rect);
  // Early out if the query rect doesn't intersect this picture.
  if (!query_rect.Intersects(map_layer_rect_) || !target_image_map_) {
    min_point_ = gfx::Point(0, 0);
    max_point_ = gfx::Point(0, 0);
    current_x_ = 1;
    current_y_ = 1;
    return;
  }

  // First, subtract the layer origin as cells are stored in layer space.
  query_rect.Offset(-map_layer_rect_.OffsetFromOrigin());

  DCHECK(!target_image_map_->cell_size_.IsEmpty());
  gfx::Size cell_size(target_image_map_->cell_size_);
  // We have to find a cell_size aligned point that corresponds to
  // query_rect. Point is a multiple of cell_size.
  min_point_ = gfx::Point(
      MathUtil::UncheckedRoundDown(query_rect.x(), cell_size.width()),
      MathUtil::UncheckedRoundDown(query_rect.y(), cell_size.height()));
  max_point_ = gfx::Point(
      MathUtil::UncheckedRoundDown(query_rect.right() - 1, cell_size.width()),
      MathUtil::UncheckedRoundDown(query_rect.bottom() - 1,
                                   cell_size.height()));

  // Limit the points to known pixel ref boundaries.
  min_point_ = gfx::Point(
      std::max(min_point_.x(), target_image_map_->min_pixel_cell_.x()),
      std::max(min_point_.y(), target_image_map_->min_pixel_cell_.y()));
  max_point_ = gfx::Point(
      std::min(max_point_.x(), target_image_map_->max_pixel_cell_.x()),
      std::min(max_point_.y(), target_image_map_->max_pixel_cell_.y()));

  // Make the current x be cell_size.width() less than min point, so that
  // the first increment will point at min_point_.
  current_x_ = min_point_.x() - cell_size.width();
  current_y_ = min_point_.y();
  if (current_y_ <= max_point_.y())
    ++(*this);
}

}  // namespace cc
