// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_IMAGE_DECODE_CONTROLLER_H_

#include <stdint.h>

#include <unordered_map>
#include <unordered_set>

#include "base/hash.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_math.h"
#include "base/threading/thread_checker.h"
#include "cc/base/cc_export.h"
#include "cc/playback/decoded_draw_image.h"
#include "cc/playback/draw_image.h"
#include "cc/raster/tile_task_runner.h"
#include "skia/ext/refptr.h"
#include "ui/gfx/transform.h"

namespace cc {

// ImageDecodeControllerKey is a class that gets a cache key out of a given draw
// image. That is, this key uniquely identifies an image in the cache. Note that
// it's insufficient to use SkImage's unique id, since the same image can appear
// in the cache multiple times at different scales and filter qualities.
class CC_EXPORT ImageDecodeControllerKey {
 public:
  static ImageDecodeControllerKey FromDrawImage(const DrawImage& image);

  bool operator==(const ImageDecodeControllerKey& other) const {
    return image_id_ == other.image_id_ && src_rect_ == other.src_rect_ &&
           target_size_ == other.target_size_ &&
           filter_quality_ == other.filter_quality_;
  }

  bool operator!=(const ImageDecodeControllerKey& other) const {
    return !(*this == other);
  }

  uint32_t image_id() const { return image_id_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  gfx::Rect src_rect() const { return src_rect_; }
  gfx::Size target_size() const { return target_size_; }

  // Helper to figure out how much memory this decoded and scaled image would
  // take.
  size_t target_bytes() const {
    // TODO(vmpstr): Handle formats other than RGBA.
    base::CheckedNumeric<size_t> result = 4;
    result *= target_size_.width();
    result *= target_size_.height();
    return result.ValueOrDefault(std::numeric_limits<size_t>::max());
  }

  std::string ToString() const;

 private:
  ImageDecodeControllerKey(uint32_t image_id,
                           const gfx::Rect& src_rect,
                           const gfx::Size& size,
                           SkFilterQuality filter_quality);

  uint32_t image_id_;
  gfx::Rect src_rect_;
  gfx::Size target_size_;
  SkFilterQuality filter_quality_;
};

// Hash function for the above ImageDecodeControllerKey.
struct ImageDecodeControllerKeyHash {
  size_t operator()(const ImageDecodeControllerKey& key) const {
    // TODO(vmpstr): This is a mess. Maybe it's faster to just search the vector
    // always (forwards or backwards to account for LRU).
    uint64_t src_rect_hash =
        base::HashInts(static_cast<uint64_t>(base::HashInts(
                           key.src_rect().x(), key.src_rect().y())),
                       static_cast<uint64_t>(base::HashInts(
                           key.src_rect().width(), key.src_rect().height())));

    uint64_t target_size_hash =
        base::HashInts(key.target_size().width(), key.target_size().height());

    return base::HashInts(base::HashInts(src_rect_hash, target_size_hash),
                          base::HashInts(key.image_id(), key.filter_quality()));
  }
};

// ImageDecodeController is responsible for generating decode tasks, decoding
// images, storing images in cache, and being able to return the decoded images
// when requested.

// ImageDecodeController is responsible for the following things:
// 1. Given a DrawImage, it can return an ImageDecodeTask which when run will
//    decode and cache the resulting image. If the image does not need a task to
//    be decoded, then nullptr will be returned. The return value of the
//    function indicates whether the image was or is going to be locked, so an
//    unlock will be required.
// 2. Given a cache key and a DrawImage, it can decode the image and store it in
//    the cache. Note that it is important that this function is only accessed
//    via an image decode task.
// 3. Given a DrawImage, it can return a DecodedDrawImage, which represented the
//    decoded version of the image. Note that if the image is not in the cache
//    and it needs to be scaled/decoded, then this decode will happen as part of
//    getting the image. As such, this should only be accessed from a raster
//    thread.
class CC_EXPORT ImageDecodeController {
 public:
  using ImageKey = ImageDecodeControllerKey;
  using ImageKeyHash = ImageDecodeControllerKeyHash;

  ImageDecodeController();
  ~ImageDecodeController();

  // Fill in an ImageDecodeTask which will decode the given image when run. In
  // case the image is already cached, fills in nullptr. Returns true if the
  // image needs to be unreffed when the caller is finished with it.
  //
  // This is called by the tile manager (on the compositor thread) when creating
  // a raster task.
  bool GetTaskForImageAndRef(const DrawImage& image,
                             uint64_t prepare_tiles_id,
                             scoped_refptr<ImageDecodeTask>* task);
  // Unrefs an image. When the tile is finished, this should be called for every
  // GetTaskForImageAndRef call that returned true.
  void UnrefImage(const DrawImage& image);

  // Returns a decoded draw image. If the image isn't found in the cache, a
  // decode will happen.
  //
  // This is called by a raster task (on a worker thread) when an image is
  // required.
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image);
  // Unrefs an image. This should be called for every GetDecodedImageForDraw
  // when the draw with the image is finished.
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image);

  // Decode the given image and store it in the cache. This is only called by an
  // image decode task from a worker thread.
  void DecodeImage(const ImageKey& key, const DrawImage& image);

  void ReduceCacheUsage();

  void RemovePendingTask(const ImageKey& key);

  // Info the controller whether we're using gpu rasterization or not. Since the
  // decode and caching behavior is different for SW and GPU decodes, when the
  // state changes, we clear all of the caches. This means that this is only
  // safe to call when there are no pending tasks (and no refs on any images).
  void SetIsUsingGpuRasterization(bool is_using_gpu_rasterization);

 private:
  // DecodedImage is a convenience storage for discardable memory. It can also
  // construct an image out of SkImageInfo and stored discardable memory.
  // TODO(vmpstr): Make this scoped_ptr.
  class DecodedImage : public base::RefCounted<DecodedImage> {
   public:
    DecodedImage(const SkImageInfo& info,
                 scoped_ptr<base::DiscardableMemory> memory,
                 const SkSize& src_rect_offset);

    SkImage* image() const {
      DCHECK(locked_);
      return image_.get();
    }

    const SkSize& src_rect_offset() const { return src_rect_offset_; }

    bool is_locked() const { return locked_; }
    bool Lock();
    void Unlock();

   private:
    friend class base::RefCounted<DecodedImage>;

    ~DecodedImage();

    bool locked_;
    SkImageInfo image_info_;
    scoped_ptr<base::DiscardableMemory> memory_;
    skia::RefPtr<SkImage> image_;
    SkSize src_rect_offset_;
  };

  // MemoryBudget is a convenience class for memory bookkeeping and ensuring
  // that we don't go over the limit when pre-decoding.
  // TODO(vmpstr): Add memory infra to keep track of memory usage of this class.
  class MemoryBudget {
   public:
    explicit MemoryBudget(size_t limit_bytes);

    size_t AvailableMemoryBytes() const;
    void AddUsage(size_t usage);
    void SubtractUsage(size_t usage);
    void ResetUsage();

   private:
    size_t GetCurrentUsageSafe() const;

    size_t limit_bytes_;
    base::CheckedNumeric<size_t> current_usage_bytes_;
  };

  using AnnotatedDecodedImage =
      std::pair<ImageKey, scoped_refptr<DecodedImage>>;

  // Looks for the key in the cache and returns true if it was found and was
  // successfully locked (or if it was already locked). Note that if this
  // function returns true, then a ref count is increased for the image.
  bool LockDecodedImageIfPossibleAndRef(const ImageKey& key);

  // Actually decode the image. Note that this function can (and should) be
  // called with no lock acquired, since it can do a lot of work. Note that it
  // can also return nullptr to indicate the decode failed.
  scoped_refptr<DecodedImage> DecodeImageInternal(const ImageKey& key,
                                                  const SkImage* image);
  void SanityCheckState(int line, bool lock_acquired);
  void RefImage(const ImageKey& key);
  void RefAtRasterImage(const ImageKey& key);
  void UnrefAtRasterImage(const ImageKey& key);

  // These functions indicate whether the images can be handled and cached by
  // ImageDecodeController or whether they will fall through to Skia (with
  // exception of possibly prerolling them). Over time these should return
  // "false" in less cases, as the ImageDecodeController should start handling
  // more of them.
  bool CanHandleImage(const ImageKey& key, const DrawImage& image);
  bool CanHandleFilterQuality(SkFilterQuality filter_quality);

  bool is_using_gpu_rasterization_;

  std::unordered_map<ImageKey, scoped_refptr<ImageDecodeTask>, ImageKeyHash>
      pending_image_tasks_;

  // The members below this comment can only be accessed if the lock is held to
  // ensure that they are safe to access on multiple threads.
  base::Lock lock_;

  std::deque<AnnotatedDecodedImage> decoded_images_;
  std::unordered_map<ImageKey, int, ImageKeyHash> decoded_images_ref_counts_;
  std::deque<AnnotatedDecodedImage> at_raster_decoded_images_;
  std::unordered_map<ImageKey, int, ImageKeyHash>
      at_raster_decoded_images_ref_counts_;
  MemoryBudget locked_images_budget_;

  // Note that this is used for cases where the only thing we do is preroll the
  // image the first time we see it. This mimics the previous behavior and
  // should over time change as the compositor starts to handle more cases.
  std::unordered_set<uint32_t> prerolled_images_;
};

}  // namespace cc

#endif  // CC_TILES_IMAGE_DECODE_CONTROLLER_H_
