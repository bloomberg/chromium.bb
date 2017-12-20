// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_
#define CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_

#include <stdint.h>

#include <memory>
#include <unordered_map>
#include <unordered_set>

#include "base/atomic_sequence_num.h"
#include "base/containers/mru_cache.h"
#include "base/hash.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/memory_coordinator_client.h"
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_math.h"
#include "base/threading/thread_checker.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/cc_export.h"
#include "cc/paint/draw_image.h"
#include "cc/tiles/image_decode_cache.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

// ImageDecodeCacheKey is a class that gets a cache key out of a given draw
// image. That is, this key uniquely identifies an image in the cache. Note that
// it's insufficient to use SkImage's unique id, since the same image can appear
// in the cache multiple times at different scales and filter qualities.
class CC_EXPORT ImageDecodeCacheKey {
 public:
  static ImageDecodeCacheKey FromDrawImage(const DrawImage& image,
                                           SkColorType color_type);

  ImageDecodeCacheKey(const ImageDecodeCacheKey& other);

  bool operator==(const ImageDecodeCacheKey& other) const {
    // The frame_key always has to be the same. However, after that all original
    // decodes are the same, so if we can use the original decode, return true.
    // If not, then we have to compare every field.
    return frame_key_ == other.frame_key_ &&
           can_use_original_size_decode_ ==
               other.can_use_original_size_decode_ &&
           target_color_space_ == other.target_color_space_ &&
           (can_use_original_size_decode_ ||
            (src_rect_ == other.src_rect_ &&
             target_size_ == other.target_size_ &&
             filter_quality_ == other.filter_quality_));
  }

  bool operator!=(const ImageDecodeCacheKey& other) const {
    return !(*this == other);
  }

  const PaintImage::FrameKey& frame_key() const { return frame_key_; }
  SkFilterQuality filter_quality() const { return filter_quality_; }
  gfx::Rect src_rect() const { return src_rect_; }
  gfx::Size target_size() const { return target_size_; }
  const gfx::ColorSpace& target_color_space() const {
    return target_color_space_;
  }

  bool can_use_original_size_decode() const {
    return can_use_original_size_decode_;
  }
  bool should_use_subrect() const { return should_use_subrect_; }
  size_t get_hash() const { return hash_; }

  // Helper to figure out how much memory the locked image represented by this
  // key would take.
  size_t locked_bytes() const {
    // TODO(vmpstr): Handle formats other than RGBA.
    base::CheckedNumeric<size_t> result = 4;
    result *= target_size_.width();
    result *= target_size_.height();
    return result.ValueOrDefault(std::numeric_limits<size_t>::max());
  }

  std::string ToString() const;

 private:
  ImageDecodeCacheKey(PaintImage::FrameKey frame_key,
                      const gfx::Rect& src_rect,
                      const gfx::Size& size,
                      const gfx::ColorSpace& target_color_space,
                      SkFilterQuality filter_quality,
                      bool can_use_original_size_decode,
                      bool should_use_subrect);

  PaintImage::FrameKey frame_key_;
  gfx::Rect src_rect_;
  gfx::Size target_size_;
  gfx::ColorSpace target_color_space_;
  SkFilterQuality filter_quality_;
  bool can_use_original_size_decode_;
  bool should_use_subrect_;
  size_t hash_;
};

// Hash function for the above ImageDecodeCacheKey.
struct ImageDecodeCacheKeyHash {
  size_t operator()(const ImageDecodeCacheKey& key) const {
    return key.get_hash();
  }
};

class CC_EXPORT SoftwareImageDecodeCache
    : public ImageDecodeCache,
      public base::trace_event::MemoryDumpProvider,
      public base::MemoryCoordinatorClient {
 public:
  using ImageKey = ImageDecodeCacheKey;
  using ImageKeyHash = ImageDecodeCacheKeyHash;

  enum class DecodeTaskType { USE_IN_RASTER_TASKS, USE_OUT_OF_RASTER_TASKS };

  SoftwareImageDecodeCache(SkColorType color_type,
                           size_t locked_memory_limit_bytes);
  ~SoftwareImageDecodeCache() override;

  // ImageDecodeCache overrides.
  TaskResult GetTaskForImageAndRef(const DrawImage& image,
                                   const TracingInfo& tracing_info) override;
  TaskResult GetOutOfRasterDecodeTaskForImageAndRef(
      const DrawImage& image) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  // Software doesn't keep outstanding images pinned, so this is a no-op.
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override {}
  void ClearCache() override;
  size_t GetMaximumMemoryLimitBytes() const override;
  void NotifyImageUnused(const PaintImage::FrameKey& frame_key) override;

  // Decode the given image and store it in the cache. This is only called by an
  // image decode task from a worker thread.
  void DecodeImageInTask(const ImageKey& key,
                         const PaintImage& paint_image,
                         DecodeTaskType task_type);

  void OnImageDecodeTaskCompleted(const ImageKey& key,
                                  DecodeTaskType task_type);

  // MemoryDumpProvider overrides.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  size_t GetNumCacheEntriesForTesting() const { return decoded_images_.size(); }

 private:
  // CacheEntry is a convenience storage for discardable memory. It can also
  // construct an image out of SkImageInfo and stored discardable memory.
  class CacheEntry {
   public:
    CacheEntry();
    CacheEntry(const SkImageInfo& info,
               std::unique_ptr<base::DiscardableMemory> memory,
               const SkSize& src_rect_offset);
    ~CacheEntry();

    void MoveImageMemoryTo(CacheEntry* entry);

    sk_sp<SkImage> image() const {
      if (!memory)
        return nullptr;
      DCHECK(is_locked);
      return image_;
    }
    const SkSize& src_rect_offset() const { return src_rect_offset_; }

    bool Lock();
    void Unlock();

    // An ID which uniquely identifies this CacheEntry within the image decode
    // cache. Used in memory tracing.
    uint64_t tracing_id() const { return tracing_id_; }
    // Mark this image as being used in either a draw or as a source for a
    // scaled image. Either case represents this decode as being valuable and
    // not wasted.
    void mark_used() { usage_stats_.used = true; }
    void mark_cached() { cached_ = true; }
    void mark_out_of_raster() { usage_stats_.first_lock_out_of_raster = true; }

    // Since this is an inner class, we expose these variables publicly for
    // simplicity.
    // TODO(vmpstr): A good simple clean-up would be to rethink this class
    // and its interactions to instead expose a few functions which would also
    // facilitate easier DCHECKs.
    int ref_count = 0;
    bool decode_failed = false;
    bool is_locked = false;
    bool is_budgeted = false;

    scoped_refptr<TileTask> in_raster_task;
    scoped_refptr<TileTask> out_of_raster_task;

    std::unique_ptr<base::DiscardableMemory> memory;

   private:
    struct UsageStats {
      // We can only create a decoded image in a locked state, so the initial
      // lock count is 1.
      int lock_count = 1;
      bool used = false;
      bool last_lock_failed = false;
      bool first_lock_wasted = false;
      bool first_lock_out_of_raster = false;
    };

    SkImageInfo image_info_;
    sk_sp<SkImage> image_;
    SkSize src_rect_offset_;
    uint64_t tracing_id_;
    UsageStats usage_stats_;
    // Indicates whether this entry was ever in the cache.
    bool cached_ = false;
  };

  // MemoryBudget is a convenience class for memory bookkeeping and ensuring
  // that we don't go over the limit when pre-decoding.
  class MemoryBudget {
   public:
    explicit MemoryBudget(size_t limit_bytes);

    size_t AvailableMemoryBytes() const;
    void AddUsage(size_t usage);
    void SubtractUsage(size_t usage);
    void ResetUsage();
    size_t total_limit_bytes() const { return limit_bytes_; }
    size_t GetCurrentUsageSafe() const;

   private:
    const size_t limit_bytes_;
    base::CheckedNumeric<size_t> current_usage_bytes_;
  };

  using ImageMRUCache = base::
      HashingMRUCache<ImageKey, std::unique_ptr<CacheEntry>, ImageKeyHash>;

  // Actually decode the image. Note that this function can (and should) be
  // called with no lock acquired, since it can do a lot of work. Note that it
  // can also return nullptr to indicate the decode failed.
  std::unique_ptr<CacheEntry> DecodeImageInternal(const ImageKey& key,
                                                  const DrawImage& draw_image);

  // Get the decoded draw image for the given key and paint_image. Note that
  // this function has to be called with no lock acquired, since it will acquire
  // its own locks and might call DecodeImageInternal above. Note that
  // when used internally, we still require that DrawWithImageFinished() is
  // called afterwards.
  DecodedDrawImage GetDecodedImageForDrawInternal(
      const ImageKey& key,
      const PaintImage& paint_image);

  // Removes unlocked decoded images until the number of decoded images is
  // reduced within the given limit.
  void ReduceCacheUsageUntilWithinLimit(size_t limit);

  // Overriden from base::MemoryCoordinatorClient.
  void OnMemoryStateChange(base::MemoryState state) override;
  void OnPurgeMemory() override;

  // Helper method to get the different tasks. Note that this should be used as
  // if it was public (ie, all of the locks need to be properly acquired).
  TaskResult GetTaskForImageAndRefInternal(const DrawImage& image,
                                           const TracingInfo& tracing_info,
                                           DecodeTaskType type);

  CacheEntry* AddCacheEntry(const ImageKey& key);

  void DecodeImageIfNecessary(const ImageKey& key,
                              const PaintImage& paint_image,
                              CacheEntry* cache_entry);
  void AddBudgetForImage(const ImageKey& key, CacheEntry* entry);
  void RemoveBudgetForImage(const ImageKey& key, CacheEntry* entry);

  std::unique_ptr<CacheEntry> DoDecodeImage(const ImageKey& key,
                                            const PaintImage& image);
  std::unique_ptr<CacheEntry> GenerateCacheEntryFromCandidate(
      const ImageKey& key,
      const DecodedDrawImage& candidate,
      bool needs_extract_subset);

  void UnrefImage(const ImageKey& key);

  std::unordered_map<ImageKey, scoped_refptr<TileTask>, ImageKeyHash>
      pending_in_raster_image_tasks_;
  std::unordered_map<ImageKey, scoped_refptr<TileTask>, ImageKeyHash>
      pending_out_of_raster_image_tasks_;

  // The members below this comment can only be accessed if the lock is held to
  // ensure that they are safe to access on multiple threads.
  // The exception is accessing |locked_images_budget_.total_limit_bytes()|,
  // which is const and thread safe.
  base::Lock lock_;

  // Decoded images and ref counts (predecode path).
  ImageMRUCache decoded_images_;

  // A map of PaintImage::FrameKey to the ImageKeys for cached decodes of this
  // PaintImage.
  std::unordered_map<PaintImage::FrameKey,
                     std::vector<ImageKey>,
                     PaintImage::FrameKeyHash>
      frame_key_to_image_keys_;

  MemoryBudget locked_images_budget_;

  SkColorType color_type_;
  size_t max_items_in_cache_;
  // Records the maximum number of items in the cache over the lifetime of the
  // cache. This is updated anytime we are requested to reduce cache usage.
  size_t lifetime_max_items_in_cache_ = 0u;
};

}  // namespace cc

#endif  // CC_TILES_SOFTWARE_IMAGE_DECODE_CACHE_H_
