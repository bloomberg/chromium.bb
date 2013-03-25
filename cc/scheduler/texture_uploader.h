// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_SCHEDULER_TEXTURE_UPLOADER_H_
#define CC_SCHEDULER_TEXTURE_UPLOADER_H_

#include <set>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/base/scoped_ptr_deque.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace WebKit { class WebGraphicsContext3D; }

namespace gfx {
class Rect;
class Size;
class Vector2d;
}

namespace cc {

class CC_EXPORT TextureUploader {
 public:
  static scoped_ptr<TextureUploader> Create(
      WebKit::WebGraphicsContext3D* context,
      bool use_map_tex_sub_image,
      bool use_shallow_flush) {
    return make_scoped_ptr(
        new TextureUploader(context, use_map_tex_sub_image, use_shallow_flush));
  }
  ~TextureUploader();

  size_t NumBlockingUploads();
  void MarkPendingUploadsAsNonBlocking();
  double EstimatedTexturesPerSecond();

  // Let content_rect be a rectangle, and let content_rect be a sub-rectangle of
  // content_rect, expressed in the same coordinate system as content_rect. Let
  // image be a buffer for content_rect. This function will copy the region
  // corresponding to source_rect to dest_offset in this sub-image.
  void Upload(const uint8* image,
              gfx::Rect content_rect,
              gfx::Rect source_rect,
              gfx::Vector2d dest_offset,
              GLenum format,
              gfx::Size size);

  void Flush();
  void ReleaseCachedQueries();

 private:
  class Query {
   public:
    static scoped_ptr<Query> Create(WebKit::WebGraphicsContext3D* context) {
      return make_scoped_ptr(new Query(context));
    }

    virtual ~Query();

    void Begin();
    void End();
    bool IsPending();
    unsigned Value();
    size_t TexturesUploaded();
    void mark_as_non_blocking() {
      is_non_blocking_ = true;
    }
    bool is_non_blocking() const {
      return is_non_blocking_;
    }

   private:
    explicit Query(WebKit::WebGraphicsContext3D* context);

    WebKit::WebGraphicsContext3D* context_;
    unsigned query_id_;
    unsigned value_;
    bool has_value_;
    bool is_non_blocking_;

    DISALLOW_COPY_AND_ASSIGN(Query);
  };

  TextureUploader(WebKit::WebGraphicsContext3D* context,
                  bool use_map_tex_sub_image,
                  bool use_shallow_flush);

  void BeginQuery();
  void EndQuery();
  void ProcessQueries();

  void UploadWithTexSubImage(const uint8* image,
                             gfx::Rect image_rect,
                             gfx::Rect source_rect,
                             gfx::Vector2d dest_offset,
                             GLenum format);
  void UploadWithMapTexSubImage(const uint8* image,
                                gfx::Rect image_rect,
                                gfx::Rect source_rect,
                                gfx::Vector2d dest_offset,
                                GLenum format);

  WebKit::WebGraphicsContext3D* context_;
  ScopedPtrDeque<Query> pending_queries_;
  ScopedPtrDeque<Query> available_queries_;
  std::multiset<double> textures_per_second_history_;
  size_t num_blocking_texture_uploads_;

  bool use_map_tex_sub_image_;
  size_t sub_image_size_;
  scoped_array<uint8> sub_image_;

  bool use_shallow_flush_;
  size_t num_texture_uploads_since_last_flush_;

  DISALLOW_COPY_AND_ASSIGN(TextureUploader);
};

}  // namespace cc

#endif  // CC_SCHEDULER_TEXTURE_UPLOADER_H_
