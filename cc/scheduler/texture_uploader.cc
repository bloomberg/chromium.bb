// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/scheduler/texture_uploader.h"

#include <algorithm>
#include <vector>

#include "base/debug/alias.h"
#include "base/debug/trace_event.h"
#include "base/metrics/histogram.h"
#include "cc/base/util.h"
#include "cc/resources/prioritized_resource.h"
#include "cc/resources/resource.h"
#include "gpu/GLES2/gl2extchromium.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebGraphicsContext3D.h"
#include "third_party/khronos/GLES2/gl2.h"
#include "third_party/khronos/GLES2/gl2ext.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/vector2d.h"

namespace {

// How many previous uploads to use when predicting future throughput.
static const size_t kUploadHistorySizeMax = 1000;
static const size_t kUploadHistorySizeInitial = 100;

// Global estimated number of textures per second to maintain estimates across
// subsequent instances of TextureUploader.
// More than one thread will not access this variable, so we do not need to
// synchronize access.
static const double kDefaultEstimatedTexturesPerSecond = 48.0 * 60.0;

// Flush interval when performing texture uploads.
static const int kTextureUploadFlushPeriod = 4;

}  // anonymous namespace

namespace cc {

TextureUploader::Query::Query(WebKit::WebGraphicsContext3D* context)
    : context_(context),
      query_id_(0),
      value_(0),
      has_value_(false),
      is_non_blocking_(false) {
  query_id_ = context_->createQueryEXT();
}

TextureUploader::Query::~Query() { context_->deleteQueryEXT(query_id_); }

void TextureUploader::Query::Begin() {
  has_value_ = false;
  is_non_blocking_ = false;
  context_->beginQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM, query_id_);
}

void TextureUploader::Query::End() {
  context_->endQueryEXT(GL_COMMANDS_ISSUED_CHROMIUM);
}

bool TextureUploader::Query::IsPending() {
  unsigned available = 1;
  context_->getQueryObjectuivEXT(
      query_id_, GL_QUERY_RESULT_AVAILABLE_EXT, &available);
  return !available;
}

unsigned TextureUploader::Query::Value() {
  if (!has_value_) {
    context_->getQueryObjectuivEXT(query_id_, GL_QUERY_RESULT_EXT, &value_);
    has_value_ = true;
  }
  return value_;
}

TextureUploader::TextureUploader(WebKit::WebGraphicsContext3D* context,
                                 bool use_map_tex_sub_image,
                                 bool use_shallow_flush)
    : context_(context),
      num_blocking_texture_uploads_(0),
      use_map_tex_sub_image_(use_map_tex_sub_image),
      sub_image_size_(0),
      use_shallow_flush_(use_shallow_flush),
      num_texture_uploads_since_last_flush_(0) {
  for (size_t i = kUploadHistorySizeInitial; i > 0; i--)
    textures_per_second_history_.insert(kDefaultEstimatedTexturesPerSecond);
}

TextureUploader::~TextureUploader() {}

size_t TextureUploader::NumBlockingUploads() {
  ProcessQueries();
  return num_blocking_texture_uploads_;
}

void TextureUploader::MarkPendingUploadsAsNonBlocking() {
  for (ScopedPtrDeque<Query>::iterator it = pending_queries_.begin();
       it != pending_queries_.end();
       ++it) {
    if ((*it)->is_non_blocking())
      continue;

    num_blocking_texture_uploads_--;
    (*it)->mark_as_non_blocking();
  }

  DCHECK(!num_blocking_texture_uploads_);
}

double TextureUploader::EstimatedTexturesPerSecond() {
  ProcessQueries();

  // Use the median as our estimate.
  std::multiset<double>::iterator median = textures_per_second_history_.begin();
  std::advance(median, textures_per_second_history_.size() / 2);
  TRACE_COUNTER_ID1("cc", "EstimatedTexturesPerSecond", context_, *median);
  return *median;
}

void TextureUploader::BeginQuery() {
  if (available_queries_.empty())
    available_queries_.push_back(Query::Create(context_));

  available_queries_.front()->Begin();
}

void TextureUploader::EndQuery() {
  available_queries_.front()->End();
  pending_queries_.push_back(available_queries_.take_front());
  num_blocking_texture_uploads_++;
}

void TextureUploader::Upload(const uint8* image,
                             gfx::Rect image_rect,
                             gfx::Rect source_rect,
                             gfx::Vector2d dest_offset,
                             GLenum format,
                             gfx::Size size) {
  CHECK(image_rect.Contains(source_rect));

  bool is_full_upload = dest_offset.IsZero() && source_rect.size() == size;

  if (is_full_upload)
    BeginQuery();

  if (use_map_tex_sub_image_) {
    UploadWithMapTexSubImage(
        image, image_rect, source_rect, dest_offset, format);
  } else {
    UploadWithTexSubImage(image, image_rect, source_rect, dest_offset, format);
  }

  if (is_full_upload)
    EndQuery();

  num_texture_uploads_since_last_flush_++;
  if (num_texture_uploads_since_last_flush_ >= kTextureUploadFlushPeriod)
    Flush();
}

void TextureUploader::Flush() {
  if (!num_texture_uploads_since_last_flush_)
    return;

  if (use_shallow_flush_)
    context_->shallowFlushCHROMIUM();

  num_texture_uploads_since_last_flush_ = 0;
}

void TextureUploader::ReleaseCachedQueries() {
  ProcessQueries();
  available_queries_.clear();
}

void TextureUploader::UploadWithTexSubImage(const uint8* image,
                                            gfx::Rect image_rect,
                                            gfx::Rect source_rect,
                                            gfx::Vector2d dest_offset,
                                            GLenum format) {
  // Instrumentation to debug issue 156107
  int source_rect_x = source_rect.x();
  int source_rect_y = source_rect.y();
  int source_rect_width = source_rect.width();
  int source_rect_height = source_rect.height();
  int image_rect_x = image_rect.x();
  int image_rect_y = image_rect.y();
  int image_rect_width = image_rect.width();
  int image_rect_height = image_rect.height();
  int dest_offset_x = dest_offset.x();
  int dest_offset_y = dest_offset.y();
  base::debug::Alias(&image);
  base::debug::Alias(&source_rect_x);
  base::debug::Alias(&source_rect_y);
  base::debug::Alias(&source_rect_width);
  base::debug::Alias(&source_rect_height);
  base::debug::Alias(&image_rect_x);
  base::debug::Alias(&image_rect_y);
  base::debug::Alias(&image_rect_width);
  base::debug::Alias(&image_rect_height);
  base::debug::Alias(&dest_offset_x);
  base::debug::Alias(&dest_offset_y);
  TRACE_EVENT0("cc", "TextureUploader::UploadWithTexSubImage");

  // Offset from image-rect to source-rect.
  gfx::Vector2d offset(source_rect.origin() - image_rect.origin());

  const uint8* pixel_source;
  unsigned int bytes_per_pixel = Resource::BytesPerPixel(format);
  // Use 4-byte row alignment (OpenGL default) for upload performance.
  // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
  unsigned int upload_image_stride =
      RoundUp(bytes_per_pixel * source_rect.width(), 4u);

  if (upload_image_stride == image_rect.width() * bytes_per_pixel &&
      !offset.x()) {
    pixel_source = &image[image_rect.width() * bytes_per_pixel * offset.y()];
  } else {
    size_t needed_size = upload_image_stride * source_rect.height();
    if (sub_image_size_ < needed_size) {
      sub_image_.reset(new uint8[needed_size]);
      sub_image_size_ = needed_size;
    }
    // Strides not equal, so do a row-by-row memcpy from the
    // paint results into a temp buffer for uploading.
    for (int row = 0; row < source_rect.height(); ++row)
      memcpy(&sub_image_[upload_image_stride * row],
             &image[bytes_per_pixel *
                 (offset.x() + (offset.y() + row) * image_rect.width())],
             source_rect.width() * bytes_per_pixel);

    pixel_source = &sub_image_[0];
  }

  context_->texSubImage2D(GL_TEXTURE_2D,
                          0,
                          dest_offset.x(),
                          dest_offset.y(),
                          source_rect.width(),
                          source_rect.height(),
                          format,
                          GL_UNSIGNED_BYTE,
                          pixel_source);
}

void TextureUploader::UploadWithMapTexSubImage(const uint8* image,
                                               gfx::Rect image_rect,
                                               gfx::Rect source_rect,
                                               gfx::Vector2d dest_offset,
                                               GLenum format) {
  // Instrumentation to debug issue 156107
  int source_rect_x = source_rect.x();
  int source_rect_y = source_rect.y();
  int source_rect_width = source_rect.width();
  int source_rect_height = source_rect.height();
  int image_rect_x = image_rect.x();
  int image_rect_y = image_rect.y();
  int image_rect_width = image_rect.width();
  int image_rect_height = image_rect.height();
  int dest_offset_x = dest_offset.x();
  int dest_offset_y = dest_offset.y();
  base::debug::Alias(&image);
  base::debug::Alias(&source_rect_x);
  base::debug::Alias(&source_rect_y);
  base::debug::Alias(&source_rect_width);
  base::debug::Alias(&source_rect_height);
  base::debug::Alias(&image_rect_x);
  base::debug::Alias(&image_rect_y);
  base::debug::Alias(&image_rect_width);
  base::debug::Alias(&image_rect_height);
  base::debug::Alias(&dest_offset_x);
  base::debug::Alias(&dest_offset_y);

  TRACE_EVENT0("cc", "TextureUploader::UploadWithMapTexSubImage");

  // Offset from image-rect to source-rect.
  gfx::Vector2d offset(source_rect.origin() - image_rect.origin());

  unsigned int bytes_per_pixel = Resource::BytesPerPixel(format);
  // Use 4-byte row alignment (OpenGL default) for upload performance.
  // Assuming that GL_UNPACK_ALIGNMENT has not changed from default.
  unsigned int upload_image_stride =
      RoundUp(bytes_per_pixel * source_rect.width(), 4u);

  // Upload tile data via a mapped transfer buffer
  uint8* pixel_dest = static_cast<uint8*>(
      context_->mapTexSubImage2DCHROMIUM(GL_TEXTURE_2D,
                                         0,
                                         dest_offset.x(),
                                         dest_offset.y(),
                                         source_rect.width(),
                                         source_rect.height(),
                                         format,
                                         GL_UNSIGNED_BYTE,
                                         GL_WRITE_ONLY));

  if (!pixel_dest) {
    UploadWithTexSubImage(image, image_rect, source_rect, dest_offset, format);
    return;
  }

  if (upload_image_stride == image_rect.width() * bytes_per_pixel &&
      !offset.x()) {
    memcpy(pixel_dest,
           &image[image_rect.width() * bytes_per_pixel * offset.y()],
           source_rect.height() * image_rect.width() * bytes_per_pixel);
  } else {
    // Strides not equal, so do a row-by-row memcpy from the
    // paint results into the pixel_dest.
    for (int row = 0; row < source_rect.height(); ++row) {
      memcpy(&pixel_dest[upload_image_stride * row],
             &image[bytes_per_pixel *
                 (offset.x() + (offset.y() + row) * image_rect.width())],
             source_rect.width() * bytes_per_pixel);
    }
  }

  context_->unmapTexSubImage2DCHROMIUM(pixel_dest);
}

void TextureUploader::ProcessQueries() {
  while (!pending_queries_.empty()) {
    if (pending_queries_.front()->IsPending())
      break;

    unsigned us_elapsed = pending_queries_.front()->Value();
    UMA_HISTOGRAM_CUSTOM_COUNTS(
        "Renderer4.TextureGpuUploadTimeUS", us_elapsed, 0, 100000, 50);

    // Clamp the queries to saner values in case the queries fail.
    us_elapsed = std::max(1u, us_elapsed);
    us_elapsed = std::min(15000u, us_elapsed);

    if (!pending_queries_.front()->is_non_blocking())
      num_blocking_texture_uploads_--;

    // Remove the min and max value from our history and insert the new one.
    double textures_per_second = 1.0 / (us_elapsed * 1e-6);
    if (textures_per_second_history_.size() >= kUploadHistorySizeMax) {
      textures_per_second_history_.erase(textures_per_second_history_.begin());
      textures_per_second_history_.erase(--textures_per_second_history_.end());
    }
    textures_per_second_history_.insert(textures_per_second);

    available_queries_.push_back(pending_queries_.take_front());
  }
}

}  // namespace cc
