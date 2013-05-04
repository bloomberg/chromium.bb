// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include <algorithm>
#include <limits>
#include <set>

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "cc/base/util.h"
#include "cc/debug/rendering_stats.h"
#include "cc/layers/content_layer_client.h"
#include "skia/ext/analysis_canvas.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDrawFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

// Version ID; to be used in serialization.
const int kPictureVersion = 1;

// Minimum size of a decoded stream that we need.
// 4 bytes for version, 4 * 4 for each of the 2 rects.
const unsigned int kMinPictureSizeBytes = 36;

class DisableLCDTextFilter : public SkDrawFilter {
 public:
  // SkDrawFilter interface.
  virtual bool filter(SkPaint* paint, SkDrawFilter::Type type) OVERRIDE {
    if (type != SkDrawFilter::kText_Type)
      return true;

    paint->setLCDRenderText(false);
    return true;
  }
};

// URI label for a lazily decoded SkPixelRef.
const char kLabelLazyDecoded[] = "lazy";

void GatherPixelRefsForRect(
    SkPicture* picture,
    gfx::Rect rect,
    Picture::PixelRefs* pixel_refs) {
  DCHECK(picture);
  SkData* pixel_ref_data = SkPictureUtils::GatherPixelRefs(
      picture,
      gfx::RectToSkRect(rect));
  if (!pixel_ref_data)
    return;

  void* data = const_cast<void*>(pixel_ref_data->data());
  if (!data) {
    pixel_ref_data->unref();
    return;
  }

  SkPixelRef** refs = reinterpret_cast<SkPixelRef**>(data);
  for (size_t i = 0; i < pixel_ref_data->size() / sizeof(*refs); ++i) {
    if (*refs && (*refs)->getURI() &&
        !strncmp((*refs)->getURI(), kLabelLazyDecoded, 4)) {
      pixel_refs->push_back(static_cast<skia::LazyPixelRef*>(*refs));
    }
    refs++;
  }
  pixel_ref_data->unref();
}

}  // namespace

scoped_refptr<Picture> Picture::Create(gfx::Rect layer_rect) {
  return make_scoped_refptr(new Picture(layer_rect));
}

scoped_refptr<Picture> Picture::CreateFromBase64String(
    const std::string& encoded_string) {
  bool success;
  scoped_refptr<Picture> picture =
    make_scoped_refptr(new Picture(encoded_string, &success));
  if (!success)
    picture = NULL;
  return picture;
}

Picture::Picture(gfx::Rect layer_rect)
    : layer_rect_(layer_rect) {
}

Picture::Picture(const std::string& encoded_string, bool* success) {
  // Decode the picture from base64.
  std::string decoded;
  base::Base64Decode(encoded_string, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  if (decoded.size() < kMinPictureSizeBytes) {
    *success = false;
    return;
  }

  int version = stream.readS32();
  if (version != kPictureVersion) {
    *success = false;
    return;
  }

  // First, read the layer and opaque rects.
  int layer_rect_x = stream.readS32();
  int layer_rect_y = stream.readS32();
  int layer_rect_width = stream.readS32();
  int layer_rect_height = stream.readS32();
  layer_rect_ = gfx::Rect(layer_rect_x,
                          layer_rect_y,
                          layer_rect_width,
                          layer_rect_height);
  int opaque_rect_x = stream.readS32();
  int opaque_rect_y = stream.readS32();
  int opaque_rect_width = stream.readS32();
  int opaque_rect_height = stream.readS32();
  opaque_rect_ = gfx::Rect(opaque_rect_x,
                           opaque_rect_y,
                           opaque_rect_width,
                           opaque_rect_height);

  // Read the picture. This creates an empty picture on failure.
  picture_ = skia::AdoptRef(new SkPicture(&stream, success, NULL));
}

Picture::Picture(const skia::RefPtr<SkPicture>& picture,
                 gfx::Rect layer_rect,
                 gfx::Rect opaque_rect,
                 const PixelRefMap& pixel_refs) :
    layer_rect_(layer_rect),
    opaque_rect_(opaque_rect),
    picture_(picture),
    pixel_refs_(pixel_refs) {
}

Picture::~Picture() {
}

scoped_refptr<Picture> Picture::GetCloneForDrawingOnThread(
    unsigned thread_index) const {
  // SkPicture is not thread-safe to rasterize with, this returns a clone
  // to rasterize with on a specific thread.
  CHECK_GT(clones_.size(), thread_index);
  return clones_[thread_index];
}

void Picture::CloneForDrawing(int num_threads) {
  TRACE_EVENT1("cc", "Picture::CloneForDrawing", "num_threads", num_threads);

  DCHECK(picture_);
  scoped_ptr<SkPicture[]> clones(new SkPicture[num_threads]);
  picture_->clone(&clones[0], num_threads);

  clones_.clear();
  for (int i = 0; i < num_threads; i++) {
    scoped_refptr<Picture> clone = make_scoped_refptr(
        new Picture(skia::AdoptRef(new SkPicture(clones[i])),
                    layer_rect_,
                    opaque_rect_,
                    pixel_refs_));
    clones_.push_back(clone);
  }
}

void Picture::Record(ContentLayerClient* painter,
                     const SkTileGridPicture::TileGridInfo& tile_grid_info,
                     RenderingStats* stats) {
  TRACE_EVENT2("cc", "Picture::Record",
               "width", layer_rect_.width(),
               "height", layer_rect_.height());

  // Record() should only be called once.
  DCHECK(!picture_);
  DCHECK(!tile_grid_info.fTileInterval.isEmpty());
  picture_ = skia::AdoptRef(new SkTileGridPicture(
      layer_rect_.width(), layer_rect_.height(), tile_grid_info));

  SkCanvas* canvas = picture_->beginRecording(
      layer_rect_.width(),
      layer_rect_.height(),
      SkPicture::kUsePathBoundsForClip_RecordingFlag |
      SkPicture::kOptimizeForClippedPlayback_RecordingFlag);

  canvas->save();
  canvas->translate(SkFloatToScalar(-layer_rect_.x()),
                    SkFloatToScalar(-layer_rect_.y()));

  gfx::RectF opaque_layer_rect;
  base::TimeTicks begin_record_time;
  if (stats)
    begin_record_time = base::TimeTicks::Now();
  painter->PaintContents(canvas, layer_rect_, &opaque_layer_rect);
  if (stats) {
    stats->total_record_time += base::TimeTicks::Now() - begin_record_time;
    stats->total_pixels_recorded +=
        layer_rect_.width() * layer_rect_.height();
  }

  canvas->restore();
  picture_->endRecording();

  opaque_rect_ = gfx::ToEnclosedRect(opaque_layer_rect);
}

void Picture::GatherPixelRefs(
    const SkTileGridPicture::TileGridInfo& tile_grid_info,
    RenderingStats* stats) {
  TRACE_EVENT2("cc", "Picture::GatherPixelRefs",
               "width", layer_rect_.width(),
               "height", layer_rect_.height());

  DCHECK(picture_);
  cell_size_ = gfx::Size(
      tile_grid_info.fTileInterval.width() +
          2 * tile_grid_info.fMargin.width(),
      tile_grid_info.fTileInterval.height() +
          2 * tile_grid_info.fMargin.height());
  DCHECK_GT(cell_size_.width(), 0);
  DCHECK_GT(cell_size_.height(), 0);

  int min_x = std::numeric_limits<int>::max();
  int min_y = std::numeric_limits<int>::max();
  int max_x = 0;
  int max_y = 0;

  base::TimeTicks begin_image_gathering_time;
  if (stats)
    begin_image_gathering_time = base::TimeTicks::Now();

  gfx::Size layer_size(layer_rect_.size());

  // Capture pixel refs for this picture in a grid
  // with cell_size_ sized cells.
  pixel_refs_.clear();
  for (int y = 0; y < layer_rect_.height(); y += cell_size_.height()) {
    for (int x = 0; x < layer_rect_.width(); x += cell_size_.width()) {
      gfx::Rect rect(gfx::Point(x, y), cell_size_);
      rect.Intersect(gfx::Rect(gfx::Point(), layer_rect_.size()));

      PixelRefs pixel_refs;
      GatherPixelRefsForRect(picture_.get(), rect, &pixel_refs);

      // Only capture non-empty cells.
      if (!pixel_refs.empty()) {
        PixelRefMapKey key(x, y);
        pixel_refs_[key].swap(pixel_refs);
        min_x = std::min(min_x, x);
        min_y = std::min(min_y, y);
        max_x = std::max(max_x, x);
        max_y = std::max(max_y, y);
      }
    }
  }

  if (stats) {
    stats->total_image_gathering_time +=
        base::TimeTicks::Now() - begin_image_gathering_time;
    stats->total_image_gathering_count++;
  }

  min_pixel_cell_ = gfx::Point(min_x, min_y);
  max_pixel_cell_ = gfx::Point(max_x, max_y);
}

void Picture::Raster(
    SkCanvas* canvas,
    gfx::Rect content_rect,
    float contents_scale,
    bool enable_lcd_text) {
  TRACE_EVENT2("cc", "Picture::Raster",
               "layer width", layer_rect_.width(),
               "layer height", layer_rect_.height());
  DCHECK(picture_);

  DisableLCDTextFilter disable_lcd_text_filter;

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(content_rect));
  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  // Pictures by default have LCD text enabled.
  if (!enable_lcd_text)
    canvas->setDrawFilter(&disable_lcd_text_filter);
  canvas->drawPicture(*picture_);
  canvas->restore();
}

void Picture::AsBase64String(std::string* output) const {
  SkDynamicMemoryWStream stream;

  // First save the version, layer_rect_ and opaque_rect.
  stream.write32(kPictureVersion);

  stream.write32(layer_rect_.x());
  stream.write32(layer_rect_.y());
  stream.write32(layer_rect_.width());
  stream.write32(layer_rect_.height());

  stream.write32(opaque_rect_.x());
  stream.write32(opaque_rect_.y());
  stream.write32(opaque_rect_.width());
  stream.write32(opaque_rect_.height());

  // Serialize the picture.
  picture_->serialize(&stream);

  // Encode the picture as base64.
  size_t serialized_size = stream.bytesWritten();
  scoped_ptr<char[]> serialized_picture(new char[serialized_size]);
  stream.copyTo(serialized_picture.get());
  base::Base64Encode(std::string(serialized_picture.get(), serialized_size),
                     output);
}

base::LazyInstance<Picture::PixelRefs>
    Picture::PixelRefIterator::empty_pixel_refs_;

Picture::PixelRefIterator::PixelRefIterator()
    : picture_(NULL),
      current_pixel_refs_(empty_pixel_refs_.Pointer()),
      current_index_(0),
      min_point_(-1, -1),
      max_point_(-1, -1),
      current_x_(0),
      current_y_(0) {
}

Picture::PixelRefIterator::PixelRefIterator(
    gfx::Rect query_rect,
    const Picture* picture)
    : picture_(picture),
      current_pixel_refs_(empty_pixel_refs_.Pointer()),
      current_index_(0) {
  gfx::Rect layer_rect = picture->layer_rect_;
  gfx::Size cell_size = picture->cell_size_;

  // Early out if the query rect doesn't intersect this picture
  if (!query_rect.Intersects(layer_rect)) {
    min_point_ = gfx::Point(0, 0);
    max_point_ = gfx::Point(0, 0);
    current_x_ = 1;
    current_y_ = 1;
    return;
  }

  // First, subtract the layer origin as cells are stored in layer space.
  query_rect.Offset(-layer_rect.OffsetFromOrigin());

  // We have to find a cell_size aligned point that corresponds to
  // query_rect. Point is a multiple of cell_size.
  min_point_ = gfx::Point(
      RoundDown(query_rect.x(), cell_size.width()),
      RoundDown(query_rect.y(), cell_size.height()));
  max_point_ = gfx::Point(
      RoundDown(query_rect.right() - 1, cell_size.width()),
      RoundDown(query_rect.bottom() - 1, cell_size.height()));

  // Limit the points to known pixel ref boundaries.
  min_point_ = gfx::Point(
      std::max(min_point_.x(), picture->min_pixel_cell_.x()),
      std::max(min_point_.y(), picture->min_pixel_cell_.y()));
  max_point_ = gfx::Point(
      std::min(max_point_.x(), picture->max_pixel_cell_.x()),
      std::min(max_point_.y(), picture->max_pixel_cell_.y()));

  // Make the current x be cell_size.width() less than min point, so that
  // the first increment will point at min_point_.
  current_x_ = min_point_.x() - cell_size.width();
  current_y_ = min_point_.y();
  if (current_y_ <= max_point_.y())
    ++(*this);
}

Picture::PixelRefIterator::~PixelRefIterator() {
}

Picture::PixelRefIterator& Picture::PixelRefIterator::operator++() {
  ++current_index_;
  // If we're not at the end of the list, then we have the next item.
  if (current_index_ < current_pixel_refs_->size())
    return *this;

  DCHECK(current_y_ <= max_point_.y());
  while (true) {
    gfx::Size cell_size = picture_->cell_size_;

    // Advance the current grid cell.
    current_x_ += cell_size.width();
    if (current_x_ > max_point_.x()) {
      current_y_ += cell_size.height();
      current_x_ = min_point_.x();
      if (current_y_ > max_point_.y()) {
        current_pixel_refs_ = empty_pixel_refs_.Pointer();
        current_index_ = 0;
        break;
      }
    }

    // If there are no pixel refs at this grid cell, keep incrementing.
    PixelRefMapKey key(current_x_, current_y_);
    PixelRefMap::const_iterator iter = picture_->pixel_refs_.find(key);
    if (iter == picture_->pixel_refs_.end())
      continue;

    // We found a non-empty list: store it and get the first pixel ref.
    current_pixel_refs_ = &iter->second;
    current_index_ = 0;
    break;
  }
  return *this;
}

}  // namespace cc
