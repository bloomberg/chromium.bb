// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/resources/picture.h"

#include <algorithm>
#include <limits>
#include <set>

#include "base/base64.h"
#include "base/debug/trace_event.h"
#include "base/values.h"
#include "cc/base/math_util.h"
#include "cc/base/util.h"
#include "cc/debug/benchmark_instrumentation.h"
#include "cc/debug/rendering_stats_instrumentation.h"
#include "cc/debug/traced_picture.h"
#include "cc/debug/traced_value.h"
#include "cc/layers/content_layer_client.h"
#include "skia/ext/lazy_pixel_ref_utils.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkDrawFilter.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/utils/SkPictureUtils.h"
#include "ui/gfx/codec/jpeg_codec.h"
#include "ui/gfx/codec/png_codec.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/skia_util.h"

namespace cc {

namespace {

SkData* EncodeBitmap(size_t* offset, const SkBitmap& bm) {
  const int kJpegQuality = 80;
  std::vector<unsigned char> data;

  // If bitmap is opaque, encode as JPEG.
  // Otherwise encode as PNG.
  bool encoding_succeeded = false;
  if (bm.isOpaque()) {
    SkAutoLockPixels lock_bitmap(bm);
    if (bm.empty())
      return NULL;

    encoding_succeeded = gfx::JPEGCodec::Encode(
        reinterpret_cast<unsigned char*>(bm.getAddr32(0, 0)),
        gfx::JPEGCodec::FORMAT_SkBitmap,
        bm.width(),
        bm.height(),
        bm.rowBytes(),
        kJpegQuality,
        &data);
  } else {
    encoding_succeeded = gfx::PNGCodec::EncodeBGRASkBitmap(bm, false, &data);
  }

  if (encoding_succeeded) {
    *offset = 0;
    return SkData::NewWithCopy(&data.front(), data.size());
  }
  return NULL;
}

bool DecodeBitmap(const void* buffer, size_t size, SkBitmap* bm) {
  const unsigned char* data = static_cast<const unsigned char *>(buffer);

  // Try PNG first.
  if (gfx::PNGCodec::Decode(data, size, bm))
    return true;

  // Try JPEG.
  scoped_ptr<SkBitmap> decoded_jpeg(gfx::JPEGCodec::Decode(data, size));
  if (decoded_jpeg) {
    *bm = *decoded_jpeg;
    return true;
  }
  return false;
}

}  // namespace

scoped_refptr<Picture> Picture::Create(gfx::Rect layer_rect) {
  return make_scoped_refptr(new Picture(layer_rect));
}

Picture::Picture(gfx::Rect layer_rect)
    : layer_rect_(layer_rect) {
  // Instead of recording a trace event for object creation here, we wait for
  // the picture to be recorded in Picture::Record.
}

scoped_refptr<Picture> Picture::CreateFromValue(const base::Value* raw_value) {
  const base::DictionaryValue* value = NULL;
  if (!raw_value->GetAsDictionary(&value))
    return NULL;

  // Decode the picture from base64.
  std::string encoded;
  if (!value->GetString("skp64", &encoded))
    return NULL;

  std::string decoded;
  base::Base64Decode(encoded, &decoded);
  SkMemoryStream stream(decoded.data(), decoded.size());

  const base::Value* layer_rect_value = NULL;
  if (!value->Get("params.layer_rect", &layer_rect_value))
    return NULL;

  gfx::Rect layer_rect;
  if (!MathUtil::FromValue(layer_rect_value, &layer_rect))
    return NULL;

  const base::Value* opaque_rect_value = NULL;
  if (!value->Get("params.opaque_rect", &opaque_rect_value))
    return NULL;

  gfx::Rect opaque_rect;
  if (!MathUtil::FromValue(opaque_rect_value, &opaque_rect))
    return NULL;

  // Read the picture. This creates an empty picture on failure.
  SkPicture* skpicture = SkPicture::CreateFromStream(&stream, &DecodeBitmap);
  if (skpicture == NULL)
    return NULL;

  return make_scoped_refptr(new Picture(skpicture, layer_rect, opaque_rect));
}

Picture::Picture(SkPicture* picture,
                 gfx::Rect layer_rect,
                 gfx::Rect opaque_rect) :
    layer_rect_(layer_rect),
    opaque_rect_(opaque_rect),
    picture_(skia::AdoptRef(picture)) {
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
  TRACE_EVENT_OBJECT_DELETED_WITH_ID(
    TRACE_DISABLED_BY_DEFAULT("cc.debug"), "cc::Picture", this);
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

    clone->EmitTraceSnapshot();
  }
}

void Picture::Record(ContentLayerClient* painter,
                     const SkTileGridPicture::TileGridInfo& tile_grid_info,
                     RenderingStatsInstrumentation* stats_instrumentation) {
  TRACE_EVENT1(benchmark_instrumentation::kCategory,
               benchmark_instrumentation::kPictureRecord,
               benchmark_instrumentation::kData, AsTraceableRecordData());

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

  SkRect layer_skrect = SkRect::MakeXYWH(layer_rect_.x(),
                                         layer_rect_.y(),
                                         layer_rect_.width(),
                                         layer_rect_.height());
  canvas->clipRect(layer_skrect);

  gfx::RectF opaque_layer_rect;
  base::TimeTicks start_time = stats_instrumentation->StartRecording();

  painter->PaintContents(canvas, layer_rect_, &opaque_layer_rect);

  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
  stats_instrumentation->AddRecord(duration,
                                   layer_rect_.width() * layer_rect_.height());

  canvas->restore();
  picture_->endRecording();

  opaque_rect_ = gfx::ToEnclosedRect(opaque_layer_rect);

  EmitTraceSnapshot();
}

void Picture::GatherPixelRefs(
    const SkTileGridPicture::TileGridInfo& tile_grid_info,
    RenderingStatsInstrumentation* stats_instrumentation) {
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

  base::TimeTicks start_time = stats_instrumentation->StartRecording();

  skia::LazyPixelRefList pixel_refs;
  skia::LazyPixelRefUtils::GatherPixelRefs(picture_.get(), &pixel_refs);
  for (skia::LazyPixelRefList::const_iterator it = pixel_refs.begin();
       it != pixel_refs.end();
       ++it) {
    gfx::Point min(
        RoundDown(static_cast<int>(it->pixel_ref_rect.x()),
                  cell_size_.width()),
        RoundDown(static_cast<int>(it->pixel_ref_rect.y()),
                  cell_size_.height()));
    gfx::Point max(
        RoundDown(static_cast<int>(std::ceil(it->pixel_ref_rect.right())),
                  cell_size_.width()),
        RoundDown(static_cast<int>(std::ceil(it->pixel_ref_rect.bottom())),
                  cell_size_.height()));

    for (int y = min.y(); y <= max.y(); y += cell_size_.height()) {
      for (int x = min.x(); x <= max.x(); x += cell_size_.width()) {
        PixelRefMapKey key(x, y);
        pixel_refs_[key].push_back(it->lazy_pixel_ref);
      }
    }

    min_x = std::min(min_x, min.x());
    min_y = std::min(min_y, min.y());
    max_x = std::max(max_x, max.x());
    max_y = std::max(max_y, max.y());
  }

  base::TimeDelta duration = stats_instrumentation->EndRecording(start_time);
  stats_instrumentation->AddImageGathering(duration);

  min_pixel_cell_ = gfx::Point(min_x, min_y);
  max_pixel_cell_ = gfx::Point(max_x, max_y);
}

void Picture::Raster(
    SkCanvas* canvas,
    SkDrawPictureCallback* callback,
    gfx::Rect content_rect,
    float contents_scale) {
  TRACE_EVENT_BEGIN1(benchmark_instrumentation::kCategory,
                     benchmark_instrumentation::kPictureRaster,
                     "data",
                     AsTraceableRasterData(content_rect, contents_scale));

  DCHECK(picture_);

  canvas->save();
  canvas->clipRect(gfx::RectToSkRect(content_rect));
  canvas->scale(contents_scale, contents_scale);
  canvas->translate(layer_rect_.x(), layer_rect_.y());
  picture_->draw(canvas, callback);
  SkIRect bounds;
  canvas->getClipDeviceBounds(&bounds);
  canvas->restore();
  TRACE_EVENT_END1(benchmark_instrumentation::kCategory,
                   benchmark_instrumentation::kPictureRaster,
                   benchmark_instrumentation::kNumPixelsRasterized,
                   bounds.width() * bounds.height());
}

void Picture::Replay(SkCanvas* canvas) {
  TRACE_EVENT_BEGIN0("cc", "Picture::Replay");
  DCHECK(picture_);

  picture_->draw(canvas);
  SkIRect bounds;
  canvas->getClipDeviceBounds(&bounds);
  TRACE_EVENT_END1("cc", "Picture::Replay",
                   "num_pixels_replayed", bounds.width() * bounds.height());
}

scoped_ptr<base::Value> Picture::AsValue() const {
  SkDynamicMemoryWStream stream;

  // Serialize the picture.
  picture_->serialize(&stream, &EncodeBitmap);

  // Encode the picture as base64.
  scoped_ptr<base::DictionaryValue> res(new base::DictionaryValue());
  res->Set("params.layer_rect", MathUtil::AsValue(layer_rect_).release());
  res->Set("params.opaque_rect", MathUtil::AsValue(opaque_rect_).release());

  size_t serialized_size = stream.bytesWritten();
  scoped_ptr<char[]> serialized_picture(new char[serialized_size]);
  stream.copyTo(serialized_picture.get());
  std::string b64_picture;
  base::Base64Encode(std::string(serialized_picture.get(), serialized_size),
                     &b64_picture);
  res->SetString("skp64", b64_picture);
  return res.PassAs<base::Value>();
}

void Picture::EmitTraceSnapshot() {
  TRACE_EVENT_OBJECT_SNAPSHOT_WITH_ID(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
      "cc::Picture", this, TracedPicture::AsTraceablePicture(this));
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

scoped_ptr<base::debug::ConvertableToTraceFormat>
    Picture::AsTraceableRasterData(gfx::Rect rect, float scale) const {
  scoped_ptr<base::DictionaryValue> raster_data(new base::DictionaryValue());
  raster_data->Set("picture_id", TracedValue::CreateIDRef(this).release());
  raster_data->SetDouble("scale", scale);
  raster_data->SetDouble("rect_x", rect.x());
  raster_data->SetDouble("rect_y", rect.y());
  raster_data->SetDouble("rect_width", rect.width());
  raster_data->SetDouble("rect_height", rect.height());
  return TracedValue::FromValue(raster_data.release());
}

scoped_ptr<base::debug::ConvertableToTraceFormat>
    Picture::AsTraceableRecordData() const {
  scoped_ptr<base::DictionaryValue> record_data(new base::DictionaryValue());
  record_data->Set("picture_id", TracedValue::CreateIDRef(this).release());
  record_data->SetInteger(benchmark_instrumentation::kWidth,
                          layer_rect_.width());
  record_data->SetInteger(benchmark_instrumentation::kHeight,
                          layer_rect_.height());
  return TracedValue::FromValue(record_data.release());
}

}  // namespace cc
