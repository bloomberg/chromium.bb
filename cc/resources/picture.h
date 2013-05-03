// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RESOURCES_PICTURE_H_
#define CC_RESOURCES_PICTURE_H_

#include <string>
#include <utility>
#include <vector>

#include "base/basictypes.h"
#include "base/hash_tables.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/memory/ref_counted.h"
#include "cc/base/cc_export.h"
#include "cc/base/hash_pair.h"
#include "skia/ext/lazy_pixel_ref.h"
#include "skia/ext/refptr.h"
#include "third_party/skia/include/core/SkPixelRef.h"
#include "third_party/skia/include/core/SkTileGridPicture.h"
#include "ui/gfx/rect.h"

namespace skia {
class AnalysisCanvas;
}

namespace cc {

class ContentLayerClient;
struct RenderingStats;

class CC_EXPORT Picture
    : public base::RefCountedThreadSafe<Picture> {
 public:
  typedef std::pair<int, int> PixelRefMapKey;
  typedef std::vector<skia::LazyPixelRef*> PixelRefs;
  typedef base::hash_map<PixelRefMapKey, PixelRefs> PixelRefMap;

  static scoped_refptr<Picture> Create(gfx::Rect layer_rect);
  static scoped_refptr<Picture> CreateFromBase64String(
      const std::string& encoded_string);

  gfx::Rect LayerRect() const { return layer_rect_; }
  gfx::Rect OpaqueRect() const { return opaque_rect_; }

  // Get thread-safe clone for rasterizing with on a specific thread.
  scoped_refptr<Picture> GetCloneForDrawingOnThread(
      unsigned thread_index) const;

  // Make thread-safe clones for rasterizing with.
  void CloneForDrawing(int num_threads);

  // Record a paint operation. To be able to safely use this SkPicture for
  // playback on a different thread this can only be called once.
  void Record(ContentLayerClient* client,
              const SkTileGridPicture::TileGridInfo& tile_grid_info,
              RenderingStats* stats);

  // Gather pixel refs from recording.
  void GatherPixelRefs(const SkTileGridPicture::TileGridInfo& tile_grid_info,
                       RenderingStats* stats);

  // Has Record() been called yet?
  bool HasRecording() const { return picture_.get() != NULL; }

  // Apply this contents scale and raster the content rect into the canvas.
  void Raster(SkCanvas* canvas,
              gfx::Rect content_rect,
              float contents_scale,
              bool enable_lcd_text);

  void AsBase64String(std::string* output) const;

  class CC_EXPORT PixelRefIterator {
   public:
    PixelRefIterator();
    PixelRefIterator(gfx::Rect layer_rect, const Picture* picture);
    ~PixelRefIterator();

    skia::LazyPixelRef* operator->() const {
      DCHECK_LT(current_index_, current_pixel_refs_->size());
      return (*current_pixel_refs_)[current_index_];
    }

    skia::LazyPixelRef* operator*() const {
      DCHECK_LT(current_index_, current_pixel_refs_->size());
      return (*current_pixel_refs_)[current_index_];
    }

    PixelRefIterator& operator++();
    operator bool() const {
      return current_index_ < current_pixel_refs_->size();
    }

   private:
    static base::LazyInstance<PixelRefs> empty_pixel_refs_;
    const Picture* picture_;
    const PixelRefs* current_pixel_refs_;
    unsigned current_index_;

    gfx::Point min_point_;
    gfx::Point max_point_;
    int current_x_;
    int current_y_;
  };

 private:
  explicit Picture(gfx::Rect layer_rect);
  Picture(const std::string& encoded_string, bool* success);
  // This constructor assumes SkPicture is already ref'd and transfers
  // ownership to this picture.
  Picture(const skia::RefPtr<SkPicture>&,
          gfx::Rect layer_rect,
          gfx::Rect opaque_rect,
          const PixelRefMap& pixel_refs);
  ~Picture();

  gfx::Rect layer_rect_;
  gfx::Rect opaque_rect_;
  skia::RefPtr<SkPicture> picture_;

  typedef std::vector<scoped_refptr<Picture> > PictureVector;
  PictureVector clones_;

  PixelRefMap pixel_refs_;
  gfx::Point min_pixel_cell_;
  gfx::Point max_pixel_cell_;
  gfx::Size cell_size_;

  friend class base::RefCountedThreadSafe<Picture>;
  friend class PixelRefIterator;
  DISALLOW_COPY_AND_ASSIGN(Picture);
};

}  // namespace cc

#endif  // CC_RESOURCES_PICTURE_H_
