// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/software_image_decode_cache.h"

#include <inttypes.h>
#include <stdint.h>

#include <algorithm>
#include <functional>

#include "base/format_macros.h"
#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/mipmap_util.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkPixmap.h"
#include "ui/gfx/skia_util.h"

using base::trace_event::MemoryAllocatorDump;
using base::trace_event::MemoryDumpLevelOfDetail;

namespace cc {
namespace {

// The number of entries to keep around in the cache. This limit can be breached
// if more items are locked. That is, locked items ignore this limit.
// Depending on the memory state of the system, we limit the amount of items
// differently.
const size_t kNormalMaxItemsInCache = 1000;
const size_t kThrottledMaxItemsInCache = 100;
const size_t kSuspendedMaxItemsInCache = 0;

// If the size of the original sized image breaches kMemoryRatioToSubrect but we
// don't need to scale the image, consider caching only the needed subrect.
// The second part that much be true is that we cache only the needed subrect if
// the total size needed for the subrect is at most kMemoryRatioToSubrect *
// (size needed for the full original image).
// Note that at least one of the dimensions has to be at least
// kMinDimensionToSubrect before an image can breach the threshold.
const size_t kMemoryThresholdToSubrect = 64 * 1024 * 1024;
const int kMinDimensionToSubrect = 4 * 1024;
const float kMemoryRatioToSubrect = 0.5f;

class AutoRemoveKeyFromTaskMap {
 public:
  AutoRemoveKeyFromTaskMap(
      std::unordered_map<SoftwareImageDecodeCache::ImageKey,
                         scoped_refptr<TileTask>,
                         SoftwareImageDecodeCache::ImageKeyHash>* task_map,
      const SoftwareImageDecodeCache::ImageKey& key)
      : task_map_(task_map), key_(key) {}
  ~AutoRemoveKeyFromTaskMap() { task_map_->erase(key_); }

 private:
  std::unordered_map<SoftwareImageDecodeCache::ImageKey,
                     scoped_refptr<TileTask>,
                     SoftwareImageDecodeCache::ImageKeyHash>* task_map_;
  const SoftwareImageDecodeCache::ImageKey& key_;
};

class AutoDrawWithImageFinished {
 public:
  AutoDrawWithImageFinished(SoftwareImageDecodeCache* cache,
                            const DrawImage& draw_image,
                            const DecodedDrawImage& decoded_draw_image)
      : cache_(cache),
        draw_image_(draw_image),
        decoded_draw_image_(decoded_draw_image) {}
  ~AutoDrawWithImageFinished() {
    cache_->DrawWithImageFinished(draw_image_, decoded_draw_image_);
  }

 private:
  SoftwareImageDecodeCache* cache_;
  const DrawImage& draw_image_;
  const DecodedDrawImage& decoded_draw_image_;
};

class ImageDecodeTaskImpl : public TileTask {
 public:
  ImageDecodeTaskImpl(SoftwareImageDecodeCache* cache,
                      const SoftwareImageDecodeCache::ImageKey& image_key,
                      const DrawImage& image,
                      SoftwareImageDecodeCache::DecodeTaskType task_type,
                      const ImageDecodeCache::TracingInfo& tracing_info)
      : TileTask(true),
        cache_(cache),
        image_key_(image_key),
        image_(image),
        task_type_(task_type),
        tracing_info_(tracing_info) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageDecodeTaskImpl::RunOnWorkerThread", "mode",
                 "software", "source_prepare_tiles_id",
                 tracing_info_.prepare_tiles_id);
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        image_.paint_image().GetSkImage().get(),
        devtools_instrumentation::ScopedImageDecodeTask::kSoftware,
        ImageDecodeCache::ToScopedTaskType(tracing_info_.task_type));
    cache_->DecodeImage(image_key_, image_, task_type_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->RemovePendingTask(image_key_, task_type_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  SoftwareImageDecodeCache* cache_;
  SoftwareImageDecodeCache::ImageKey image_key_;
  DrawImage image_;
  SoftwareImageDecodeCache::DecodeTaskType task_type_;
  const ImageDecodeCache::TracingInfo tracing_info_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

SkSize GetScaleAdjustment(const ImageDecodeCacheKey& key) {
  // If the requested filter quality did not require scale, then the adjustment
  // is identity.
  if (key.can_use_original_size_decode() || key.should_use_subrect()) {
    return SkSize::Make(1.f, 1.f);
  } else if (key.filter_quality() == kMedium_SkFilterQuality) {
    return MipMapUtil::GetScaleAdjustmentForSize(key.src_rect().size(),
                                                 key.target_size());
  } else {
    float x_scale =
        key.target_size().width() / static_cast<float>(key.src_rect().width());
    float y_scale = key.target_size().height() /
                    static_cast<float>(key.src_rect().height());
    return SkSize::Make(x_scale, y_scale);
  }
}

SkFilterQuality GetDecodedFilterQuality(const ImageDecodeCacheKey& key) {
  return std::min(key.filter_quality(), kLow_SkFilterQuality);
}

SkImageInfo CreateImageInfo(size_t width,
                            size_t height,
                            SkColorType color_type) {
  return SkImageInfo::Make(width, height, color_type, kPremul_SkAlphaType);
}

void RecordLockExistingCachedImageHistogram(TilePriority::PriorityBin bin,
                                            bool success) {
  switch (bin) {
    case TilePriority::NOW:
      UMA_HISTOGRAM_BOOLEAN("Renderer4.LockExistingCachedImage.Software.NOW",
                            success);
    case TilePriority::SOON:
      UMA_HISTOGRAM_BOOLEAN("Renderer4.LockExistingCachedImage.Software.SOON",
                            success);
    case TilePriority::EVENTUALLY:
      UMA_HISTOGRAM_BOOLEAN(
          "Renderer4.LockExistingCachedImage.Software.EVENTUALLY", success);
  }
}

gfx::Rect GetSrcRect(const DrawImage& image) {
  const SkIRect& src_rect = image.src_rect();
  int x = std::max(0, src_rect.x());
  int y = std::max(0, src_rect.y());
  int right = std::min(image.paint_image().width(), src_rect.right());
  int bottom = std::min(image.paint_image().height(), src_rect.bottom());
  if (x >= right || y >= bottom)
    return gfx::Rect();
  return gfx::Rect(x, y, right - x, bottom - y);
}

}  // namespace

SoftwareImageDecodeCache::SoftwareImageDecodeCache(
    SkColorType color_type,
    size_t locked_memory_limit_bytes)
    : decoded_images_(ImageMRUCache::NO_AUTO_EVICT),
      at_raster_decoded_images_(ImageMRUCache::NO_AUTO_EVICT),
      locked_images_budget_(locked_memory_limit_bytes),
      color_type_(color_type),
      max_items_in_cache_(kNormalMaxItemsInCache) {
  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::SoftwareImageDecodeCache",
        base::ThreadTaskRunnerHandle::Get());
  }
  // Register this component with base::MemoryCoordinatorClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

SoftwareImageDecodeCache::~SoftwareImageDecodeCache() {
  // Debugging crbug.com/650234
  CHECK_EQ(0u, decoded_images_ref_counts_.size());
  CHECK_EQ(0u, at_raster_decoded_images_ref_counts_.size());

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  // Unregister this component with memory_coordinator::ClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(this);
}

ImageDecodeCache::TaskResult SoftwareImageDecodeCache::GetTaskForImageAndRef(
    const DrawImage& image,
    const TracingInfo& tracing_info) {
  DCHECK_EQ(tracing_info.task_type, TaskType::kInRaster);
  return GetTaskForImageAndRefInternal(image, tracing_info,
                                       DecodeTaskType::USE_IN_RASTER_TASKS);
}

ImageDecodeCache::TaskResult
SoftwareImageDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    const DrawImage& image) {
  return GetTaskForImageAndRefInternal(
      image, TracingInfo(0, TilePriority::NOW, TaskType::kOutOfRaster),
      DecodeTaskType::USE_OUT_OF_RASTER_TASKS);
}

ImageDecodeCache::TaskResult
SoftwareImageDecodeCache::GetTaskForImageAndRefInternal(
    const DrawImage& image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type) {
  // If the image already exists or if we're going to create a task for it, then
  // we'll likely need to ref this image (the exception is if we're prerolling
  // the image only). That means the image is or will be in the cache. When the
  // ref goes to 0, it will be unpinned but will remain in the cache. If the
  // image does not fit into the budget, then we don't ref this image, since it
  // will be decoded at raster time which is when it will be temporarily put in
  // the cache.
  ImageKey key = ImageKey::FromDrawImage(image, color_type_);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetTaskForImageAndRef", "key",
               key.ToString());

  // If the target size is empty, we can skip this image during draw (and thus
  // we don't need to decode it or ref it).
  if (key.target_size().IsEmpty()) {
    return TaskResult(false);
  }

  base::AutoLock lock(lock_);

  // If we already have the image in cache, then we can return it.
  auto decoded_it = decoded_images_.Get(key);
  bool new_image_fits_in_memory =
      locked_images_budget_.AvailableMemoryBytes() >= key.locked_bytes();
  if (decoded_it != decoded_images_.end()) {
    bool image_was_locked = decoded_it->second->is_locked();
    if (image_was_locked ||
        (new_image_fits_in_memory && decoded_it->second->Lock())) {
      RefImage(key);

      // If the image wasn't locked, then we just succeeded in locking it.
      if (!image_was_locked) {
        RecordLockExistingCachedImageHistogram(tracing_info.requesting_tile_bin,
                                               true);
      }
      return TaskResult(true);
    }

    // If the image fits in memory, then we at least tried to lock it and
    // failed. This means that it's not valid anymore.
    if (new_image_fits_in_memory) {
      RecordLockExistingCachedImageHistogram(tracing_info.requesting_tile_bin,
                                             false);
      CleanupDecodedImagesCache(key, decoded_it);
    }
  }

  DCHECK(task_type == DecodeTaskType::USE_IN_RASTER_TASKS ||
         task_type == DecodeTaskType::USE_OUT_OF_RASTER_TASKS);
  // If the task exists, return it. Note that if we always need to create a new
  // task, then just set |existing_task| to reference the passed in task (which
  // is set to nullptr above).
  scoped_refptr<TileTask>& existing_task =
      (task_type == DecodeTaskType::USE_IN_RASTER_TASKS)
          ? pending_in_raster_image_tasks_[key]
          : pending_out_of_raster_image_tasks_[key];
  if (existing_task) {
    RefImage(key);
    return TaskResult(existing_task);
  }

  // At this point, we have to create a new image/task, so we need to abort if
  // it doesn't fit into memory and there are currently no raster tasks that
  // would have already accounted for memory. The latter part is possible if
  // there's a running raster task that could not be canceled, and still has a
  // ref to the image that is now being reffed for the new schedule.
  if (!new_image_fits_in_memory && (decoded_images_ref_counts_.find(key) ==
                                    decoded_images_ref_counts_.end())) {
    return TaskResult(false);
  }

  // Actually create the task. RefImage will account for memory on the first
  // ref.
  RefImage(key);
  existing_task = base::MakeRefCounted<ImageDecodeTaskImpl>(
      this, key, image, task_type, tracing_info);
  return TaskResult(existing_task);
}

void SoftwareImageDecodeCache::RefImage(const ImageKey& key) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::RefImage", "key", key.ToString());
  lock_.AssertAcquired();
  int ref = ++decoded_images_ref_counts_[key];
  if (ref == 1) {
    DCHECK_GE(locked_images_budget_.AvailableMemoryBytes(), key.locked_bytes());
    locked_images_budget_.AddUsage(key.locked_bytes());
  }
}

void SoftwareImageDecodeCache::UnrefImage(const DrawImage& image) {
  // When we unref the image, there are several situations we need to consider:
  // 1. The ref did not reach 0, which means we have to keep the image locked.
  // 2. The ref reached 0, we should unlock it.
  //   2a. The image isn't in the locked cache because we didn't get to decode
  //       it yet (or failed to decode it).
  //   2b. Unlock the image but keep it in list.
  const ImageKey& key = ImageKey::FromDrawImage(image, color_type_);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::UnrefImage", "key", key.ToString());

  base::AutoLock lock(lock_);
  auto ref_count_it = decoded_images_ref_counts_.find(key);
  DCHECK(ref_count_it != decoded_images_ref_counts_.end());

  --ref_count_it->second;
  if (ref_count_it->second == 0) {
    decoded_images_ref_counts_.erase(ref_count_it);
    locked_images_budget_.SubtractUsage(key.locked_bytes());

    auto decoded_image_it = decoded_images_.Peek(key);
    // If we've never decoded the image before ref reached 0, then we wouldn't
    // have it in our cache. This would happen if we canceled tasks.
    if (decoded_image_it == decoded_images_.end())
      return;
    DCHECK(decoded_image_it->second->is_locked());
    decoded_image_it->second->Unlock();
  }
}

void SoftwareImageDecodeCache::DecodeImage(const ImageKey& key,
                                           const DrawImage& image,
                                           DecodeTaskType task_type) {
  TRACE_EVENT1("cc", "SoftwareImageDecodeCache::DecodeImage", "key",
               key.ToString());
  base::AutoLock lock(lock_);
  AutoRemoveKeyFromTaskMap remove_key_from_task_map(
      (task_type == DecodeTaskType::USE_IN_RASTER_TASKS)
          ? &pending_in_raster_image_tasks_
          : &pending_out_of_raster_image_tasks_,
      key);

  // We could have finished all of the raster tasks (cancelled) while the task
  // was just starting to run. Since this task already started running, it
  // wasn't cancelled. So, if the ref count for the image is 0 then we can just
  // abort.
  if (decoded_images_ref_counts_.find(key) ==
      decoded_images_ref_counts_.end()) {
    return;
  }

  auto image_it = decoded_images_.Peek(key);
  if (image_it != decoded_images_.end()) {
    if (image_it->second->is_locked() || image_it->second->Lock())
      return;
    CleanupDecodedImagesCache(key, image_it);
  }

  std::unique_ptr<DecodedImage> decoded_image;
  {
    base::AutoUnlock unlock(lock_);
    decoded_image = DecodeImageInternal(key, image);
  }

  // Abort if we failed to decode the image.
  if (!decoded_image)
    return;

  // At this point, it could have been the case that this image was decoded in
  // place by an already running raster task from a previous schedule. If that's
  // the case, then it would have already been placed into the cache (possibly
  // locked). Remove it if that was the case.
  image_it = decoded_images_.Peek(key);
  if (image_it != decoded_images_.end()) {
    if (image_it->second->is_locked() || image_it->second->Lock()) {
      // Make sure to unlock the decode we did in this function.
      decoded_image->Unlock();
      return;
    }
    CleanupDecodedImagesCache(key, image_it);
  }

  // We could have finished all of the raster tasks (cancelled) while this image
  // decode task was running, which means that we now have a locked image but no
  // ref counts. Unlock it immediately in this case.
  if (decoded_images_ref_counts_.find(key) ==
      decoded_images_ref_counts_.end()) {
    decoded_image->Unlock();
  }

  if (task_type == DecodeTaskType::USE_OUT_OF_RASTER_TASKS)
    decoded_image->mark_out_of_raster();

  RecordImageMipLevelUMA(
      MipMapUtil::GetLevelForSize(key.src_rect().size(), key.target_size()));

  CacheDecodedImages(key, std::move(decoded_image));
}

std::unique_ptr<SoftwareImageDecodeCache::DecodedImage>
SoftwareImageDecodeCache::DecodeImageInternal(const ImageKey& key,
                                              const DrawImage& draw_image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DecodeImageInternal", "key",
               key.ToString());
  const PaintImage& paint_image = draw_image.paint_image();
  if (!paint_image)
    return nullptr;

  // Special case subrect into a special function.
  if (key.should_use_subrect())
    return GetSubrectImageDecode(key, paint_image);

  // There are two cases where we can use the exact size image decode:
  // - If we we're using full image (no subset) and we can decode natively to
  // that scale, or
  // - If we're not doing a scale at all (which is supported by all decoders and
  // subsetting is handled in the draw calls).
  // TODO(vmpstr): See if we can subrect the result of decoded to scale.
  SkIRect full_size_rect =
      SkIRect::MakeWH(paint_image.width(), paint_image.height());
  bool need_subset = (gfx::RectToSkIRect(key.src_rect()) != full_size_rect);
  SkISize exact_size =
      SkISize::Make(key.target_size().width(), key.target_size().height());
  // TODO(vmpstr): If an image of a bigger size is already decoded and is
  // lock-able then it might be faster to just scale that instead of redecoding
  // to exact scale. We need to profile this.
  if ((!need_subset &&
       exact_size == paint_image.GetSupportedDecodeSize(exact_size)) ||
      SkIRect::MakeSize(exact_size) == full_size_rect) {
    return GetExactSizeImageDecode(key, paint_image);
  }
  return GetScaledImageDecode(key, paint_image);
}

DecodedDrawImage SoftwareImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  ImageKey key = ImageKey::FromDrawImage(draw_image, color_type_);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetDecodedImageForDraw", "key",
               key.ToString());
  // If the target size is empty, we can skip this image draw.
  if (key.target_size().IsEmpty())
    return DecodedDrawImage(nullptr, kNone_SkFilterQuality);

  return GetDecodedImageForDrawInternal(key, draw_image);
}

DecodedDrawImage SoftwareImageDecodeCache::GetDecodedImageForDrawInternal(
    const ImageKey& key,
    const DrawImage& draw_image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetDecodedImageForDrawInternal",
               "key", key.ToString());

  base::AutoLock lock(lock_);
  auto decoded_images_it = decoded_images_.Get(key);
  // If we found the image and it's locked, then return it. If it's not locked,
  // erase it from the cache since it might be put into the at-raster cache.
  std::unique_ptr<DecodedImage> scoped_decoded_image;
  DecodedImage* decoded_image = nullptr;
  if (decoded_images_it != decoded_images_.end()) {
    decoded_image = decoded_images_it->second.get();
    if (decoded_image->is_locked()) {
      RefImage(key);
      decoded_image->mark_used();
      return DecodedDrawImage(
          decoded_image->image(), decoded_image->src_rect_offset(),
          GetScaleAdjustment(key), GetDecodedFilterQuality(key));
    } else {
      scoped_decoded_image = std::move(decoded_images_it->second);
      CleanupDecodedImagesCache(key, decoded_images_it);
    }
  }

  // See if another thread already decoded this image at raster time. If so, we
  // can just use that result directly.
  auto at_raster_images_it = at_raster_decoded_images_.Get(key);
  if (at_raster_images_it != at_raster_decoded_images_.end()) {
    DCHECK(at_raster_images_it->second->is_locked());
    RefAtRasterImage(key);
    DecodedImage* at_raster_decoded_image = at_raster_images_it->second.get();
    at_raster_decoded_image->mark_used();
    auto decoded_draw_image =
        DecodedDrawImage(at_raster_decoded_image->image(),
                         at_raster_decoded_image->src_rect_offset(),
                         GetScaleAdjustment(key), GetDecodedFilterQuality(key));
    decoded_draw_image.set_at_raster_decode(true);
    return decoded_draw_image;
  }

  // Now we know that we don't have a locked image, and we seem to be the first
  // thread encountering this image (that might not be true, since other threads
  // might be decoding it already). This means that we need to decode the image
  // assuming we can't lock the one we found in the cache.
  bool check_at_raster_cache = false;
  if (!decoded_image || !decoded_image->Lock()) {
    // Note that we have to release the lock, since this lock is also accessed
    // on the compositor thread. This means holding on to the lock might stall
    // the compositor thread for the duration of the decode!
    base::AutoUnlock unlock(lock_);
    scoped_decoded_image = DecodeImageInternal(key, draw_image);
    decoded_image = scoped_decoded_image.get();

    // Skip the image if we couldn't decode it.
    if (!decoded_image)
      return DecodedDrawImage(nullptr, kNone_SkFilterQuality);
    check_at_raster_cache = true;
  }

  DCHECK(decoded_image == scoped_decoded_image.get());

  // While we unlocked the lock, it could be the case that another thread
  // already decoded this already and put it in the at-raster cache. Look it up
  // first.
  if (check_at_raster_cache) {
    at_raster_images_it = at_raster_decoded_images_.Get(key);
    if (at_raster_images_it != at_raster_decoded_images_.end()) {
      // We have to drop our decode, since the one in the cache is being used by
      // another thread.
      decoded_image->Unlock();
      decoded_image = at_raster_images_it->second.get();
      scoped_decoded_image = nullptr;
    }
  }

  // If we really are the first ones, or if the other thread already unlocked
  // the image, then put our work into at-raster time cache.
  if (scoped_decoded_image)
    at_raster_decoded_images_.Put(key, std::move(scoped_decoded_image));

  DCHECK(decoded_image);
  DCHECK(decoded_image->is_locked());
  RefAtRasterImage(key);
  decoded_image->mark_used();
  auto decoded_draw_image =
      DecodedDrawImage(decoded_image->image(), decoded_image->src_rect_offset(),
                       GetScaleAdjustment(key), GetDecodedFilterQuality(key));
  decoded_draw_image.set_at_raster_decode(true);
  return decoded_draw_image;
}

std::unique_ptr<SoftwareImageDecodeCache::DecodedImage>
SoftwareImageDecodeCache::GetExactSizeImageDecode(
    const ImageKey& key,
    const PaintImage& paint_image) {
  SkISize decode_size =
      SkISize::Make(key.target_size().width(), key.target_size().height());
  DCHECK(decode_size == paint_image.GetSupportedDecodeSize(decode_size));

  SkImageInfo decoded_info =
      paint_image.CreateDecodeImageInfo(decode_size, color_type_);

  std::unique_ptr<base::DiscardableMemory> decoded_pixels;
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "SoftwareImageDecodeCache::GetOriginalSizeImageDecode - "
                 "allocate decoded pixels");
    decoded_pixels =
        base::DiscardableMemoryAllocator::GetInstance()
            ->AllocateLockedDiscardableMemory(decoded_info.minRowBytes() *
                                              decoded_info.height());
  }
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "SoftwareImageDecodeCache::GetOriginalSizeImageDecode - "
                 "decode");
    bool result = paint_image.Decode(decoded_pixels->data(), &decoded_info,
                                     key.target_color_space().ToSkColorSpace(),
                                     key.frame_key().frame_index());
    if (!result) {
      decoded_pixels->Unlock();
      return nullptr;
    }
  }

  return std::make_unique<DecodedImage>(decoded_info, std::move(decoded_pixels),
                                        SkSize::Make(0, 0),
                                        next_tracing_id_.GetNext());
}

std::unique_ptr<SoftwareImageDecodeCache::DecodedImage>
SoftwareImageDecodeCache::GetSubrectImageDecode(const ImageKey& key,
                                                const PaintImage& image) {
  // Construct a key to use in GetDecodedImageForDrawInternal().
  // This allows us to reuse an image in any cache if available.
  // This uses the original sized image, since when we're subrecting, there is
  // no scale.
  // TODO(vmpstr): This doesn't have to be true, but the Key generation makes it
  // so. We could also subrect scaled images.
  SkIRect exact_size_rect = SkIRect::MakeWH(image.width(), image.height());
  DrawImage exact_size_draw_image(image, exact_size_rect, kNone_SkFilterQuality,
                                  SkMatrix::I(), key.frame_key().frame_index(),
                                  key.target_color_space());
  ImageKey exact_size_key =
      ImageKey::FromDrawImage(exact_size_draw_image, color_type_);

  // Sanity checks.
#if DCHECK_IS_ON()
  SkISize exact_target_size =
      SkISize::Make(exact_size_key.target_size().width(),
                    exact_size_key.target_size().height());
  DCHECK(image.GetSupportedDecodeSize(exact_target_size) == exact_target_size);
  DCHECK(!exact_size_key.should_use_subrect());
#endif

  auto decoded_draw_image =
      GetDecodedImageForDrawInternal(exact_size_key, exact_size_draw_image);
  AutoDrawWithImageFinished auto_finish_draw(this, exact_size_draw_image,
                                             decoded_draw_image);
  if (!decoded_draw_image.image())
    return nullptr;

  DCHECK(SkColorSpace::Equals(decoded_draw_image.image()->colorSpace(),
                              key.target_color_space().ToSkColorSpace().get()));
  SkImageInfo subrect_info = CreateImageInfo(
      key.target_size().width(), key.target_size().height(), color_type_);
  std::unique_ptr<base::DiscardableMemory> subrect_pixels;
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "SoftwareImageDecodeCache::GetSubrectImageDecode - "
                 "allocate subrect pixels");
    // TODO(vmpstr): This is using checked math to diagnose a problem reported
    // in crbug.com/662217. If this is causing crashes, then it should be fixed
    // elsewhere by skipping images that are too large.
    base::CheckedNumeric<size_t> byte_size = subrect_info.minRowBytes();
    byte_size *= subrect_info.height();
    subrect_pixels =
        base::DiscardableMemoryAllocator::GetInstance()
            ->AllocateLockedDiscardableMemory(byte_size.ValueOrDie());
  }
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "SoftwareImageDecodeCache::GetSubrectImageDecode - "
                 "read pixels");
    bool result = decoded_draw_image.image()->readPixels(
        subrect_info, subrect_pixels->data(), subrect_info.minRowBytes(),
        key.src_rect().x(), key.src_rect().y(), SkImage::kDisallow_CachingHint);
    // We have a decoded image, and we're reading into already allocated memory.
    // This should never fail.
    DCHECK(result);
  }

  return base::WrapUnique(new DecodedImage(
      subrect_info.makeColorSpace(decoded_draw_image.image()->refColorSpace()),
      std::move(subrect_pixels),
      SkSize::Make(-key.src_rect().x(), -key.src_rect().y()),
      next_tracing_id_.GetNext()));
}

std::unique_ptr<SoftwareImageDecodeCache::DecodedImage>
SoftwareImageDecodeCache::GetScaledImageDecode(const ImageKey& key,
                                               const PaintImage& image) {
  // Construct a key to use in GetDecodedImageForDrawInternal().
  // This allows us to reuse an image in any cache if available.
  // TODO(vmpstr): If we're using a subrect, then we need to decode the original
  // image and subset it since the subset might be strict. If we plumb whether
  // the subrect is strict or not, we can loosen this condition.
  SkIRect full_size_rect = SkIRect::MakeWH(image.width(), image.height());
  bool need_subset = (gfx::RectToSkIRect(key.src_rect()) != full_size_rect);
  SkIRect exact_size_rect =
      need_subset
          ? full_size_rect
          : SkIRect::MakeSize(image.GetSupportedDecodeSize(SkISize::Make(
                key.target_size().width(), key.target_size().height())));

  DrawImage exact_size_draw_image(image, exact_size_rect, kNone_SkFilterQuality,
                                  SkMatrix::I(), key.frame_key().frame_index(),
                                  key.target_color_space());
  ImageKey exact_size_key =
      ImageKey::FromDrawImage(exact_size_draw_image, color_type_);

  // Sanity checks.
#if DCHECK_IS_ON()
  SkISize exact_target_size =
      SkISize::Make(exact_size_key.target_size().width(),
                    exact_size_key.target_size().height());
  DCHECK(image.GetSupportedDecodeSize(exact_target_size) == exact_target_size);
  DCHECK(!need_subset || exact_size_key.target_size() ==
                             gfx::Size(image.width(), image.height()));
  DCHECK(!exact_size_key.should_use_subrect());
#endif

  auto decoded_draw_image =
      GetDecodedImageForDrawInternal(exact_size_key, exact_size_draw_image);
  AutoDrawWithImageFinished auto_finish_draw(this, exact_size_draw_image,
                                             decoded_draw_image);
  if (!decoded_draw_image.image())
    return nullptr;

  SkPixmap decoded_pixmap;
  bool result = decoded_draw_image.image()->peekPixels(&decoded_pixmap);
  DCHECK(result) << key.ToString();
  if (need_subset) {
    result = decoded_pixmap.extractSubset(&decoded_pixmap,
                                          gfx::RectToSkIRect(key.src_rect()));
    DCHECK(result) << key.ToString();
  }

  DCHECK(!key.target_size().IsEmpty());
  DCHECK(SkColorSpace::Equals(decoded_draw_image.image()->colorSpace(),
                              key.target_color_space().ToSkColorSpace().get()));
  SkImageInfo scaled_info = CreateImageInfo(
      key.target_size().width(), key.target_size().height(), color_type_);
  std::unique_ptr<base::DiscardableMemory> scaled_pixels;
  {
    TRACE_EVENT0(
        TRACE_DISABLED_BY_DEFAULT("cc.debug"),
        "SoftwareImageDecodeCache::ScaleImage - allocate scaled pixels");
    scaled_pixels = base::DiscardableMemoryAllocator::GetInstance()
                        ->AllocateLockedDiscardableMemory(
                            scaled_info.minRowBytes() * scaled_info.height());
  }
  SkPixmap scaled_pixmap(scaled_info, scaled_pixels->data(),
                         scaled_info.minRowBytes());
  DCHECK(key.filter_quality() == kMedium_SkFilterQuality);
  {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "SoftwareImageDecodeCache::ScaleImage - scale pixels");
    bool result =
        decoded_pixmap.scalePixels(scaled_pixmap, key.filter_quality());
    DCHECK(result) << key.ToString();
  }

  return std::make_unique<DecodedImage>(
      scaled_info.makeColorSpace(decoded_draw_image.image()->refColorSpace()),
      std::move(scaled_pixels),
      SkSize::Make(-key.src_rect().x(), -key.src_rect().y()),
      next_tracing_id_.GetNext());
}

void SoftwareImageDecodeCache::DrawWithImageFinished(
    const DrawImage& image,
    const DecodedDrawImage& decoded_image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DrawWithImageFinished", "key",
               ImageKey::FromDrawImage(image, color_type_).ToString());
  ImageKey key = ImageKey::FromDrawImage(image, color_type_);
  if (!decoded_image.image())
    return;

  if (decoded_image.is_at_raster_decode())
    UnrefAtRasterImage(key);
  else
    UnrefImage(image);
}

void SoftwareImageDecodeCache::RefAtRasterImage(const ImageKey& key) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::RefAtRasterImage", "key",
               key.ToString());
  DCHECK(at_raster_decoded_images_.Peek(key) !=
         at_raster_decoded_images_.end());
  ++at_raster_decoded_images_ref_counts_[key];
}

void SoftwareImageDecodeCache::UnrefAtRasterImage(const ImageKey& key) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::UnrefAtRasterImage", "key",
               key.ToString());
  base::AutoLock lock(lock_);

  auto ref_it = at_raster_decoded_images_ref_counts_.find(key);
  DCHECK(ref_it != at_raster_decoded_images_ref_counts_.end());
  --ref_it->second;
  if (ref_it->second == 0) {
    at_raster_decoded_images_ref_counts_.erase(ref_it);
    auto at_raster_image_it = at_raster_decoded_images_.Peek(key);
    DCHECK(at_raster_image_it != at_raster_decoded_images_.end());

    // The ref for our image reached 0 and it's still locked. We need to figure
    // out what the best thing to do with the image. There are several
    // situations:
    // 1. The image is not in the main cache and...
    //    1a. ... its ref count is 0: unlock our image and put it in the main
    //    cache.
    //    1b. ... ref count is not 0: keep the image locked and put it in the
    //    main cache.
    // 2. The image is in the main cache...
    //    2a. ... and is locked: unlock our image and discard it
    //    2b. ... and is unlocked and...
    //       2b1. ... its ref count is 0: unlock our image and replace the
    //       existing one with ours.
    //       2b2. ... its ref count is not 0: this shouldn't be possible.
    auto image_it = decoded_images_.Peek(key);
    if (image_it == decoded_images_.end()) {
      if (decoded_images_ref_counts_.find(key) ==
          decoded_images_ref_counts_.end()) {
        at_raster_image_it->second->Unlock();
      }
      CacheDecodedImages(key, std::move(at_raster_image_it->second));
    } else if (image_it->second->is_locked()) {
      at_raster_image_it->second->Unlock();
    } else {
      DCHECK(decoded_images_ref_counts_.find(key) ==
             decoded_images_ref_counts_.end());
      at_raster_image_it->second->Unlock();
      // Access decoded_images_ directly here to avoid deletion and entry
      // of same key in ImageKey vector of decoded_images_unique_ids_.
      decoded_images_.Erase(image_it);
      decoded_images_.Put(key, std::move(at_raster_image_it->second));
    }
    at_raster_decoded_images_.Erase(at_raster_image_it);
  }
}

void SoftwareImageDecodeCache::ReduceCacheUsageUntilWithinLimit(size_t limit) {
  TRACE_EVENT0("cc", "SoftwareImageDecodeCache::ReduceCacheUsage");
  size_t num_to_remove =
      (decoded_images_.size() > limit) ? (decoded_images_.size() - limit) : 0;
  for (auto it = decoded_images_.rbegin();
       num_to_remove != 0 && it != decoded_images_.rend();) {
    if (it->second->is_locked()) {
      ++it;
      continue;
    }

    it = decoded_images_.Erase(it);
    --num_to_remove;
  }
}

void SoftwareImageDecodeCache::ReduceCacheUsage() {
  base::AutoLock lock(lock_);
  ReduceCacheUsageUntilWithinLimit(max_items_in_cache_);
}

void SoftwareImageDecodeCache::ClearCache() {
  base::AutoLock lock(lock_);
  ReduceCacheUsageUntilWithinLimit(0);
}

size_t SoftwareImageDecodeCache::GetMaximumMemoryLimitBytes() const {
  return locked_images_budget_.total_limit_bytes();
}

void SoftwareImageDecodeCache::NotifyImageUnused(
    const PaintImage::FrameKey& frame_key) {
  base::AutoLock lock(lock_);

  auto it = frame_key_to_image_keys_.find(frame_key);
  if (it == frame_key_to_image_keys_.end())
    return;

  for (auto key = it->second.begin(); key != it->second.end(); ++key) {
    // This iterates over the ImageKey vector for the given skimage_id,
    // and deletes all entries from decoded_images_ corresponding to the
    // skimage_id.
    auto image_it = decoded_images_.Peek(*key);
    // TODO(sohanjg) :Find an optimized way to cleanup locked images.
    if (image_it != decoded_images_.end() && !image_it->second->is_locked())
      decoded_images_.Erase(image_it);
  }
  frame_key_to_image_keys_.erase(it);
}

void SoftwareImageDecodeCache::RemovePendingTask(const ImageKey& key,
                                                 DecodeTaskType task_type) {
  base::AutoLock lock(lock_);
  switch (task_type) {
    case DecodeTaskType::USE_IN_RASTER_TASKS:
      pending_in_raster_image_tasks_.erase(key);
      break;
    case DecodeTaskType::USE_OUT_OF_RASTER_TASKS:
      pending_out_of_raster_image_tasks_.erase(key);
      break;
  }
}

bool SoftwareImageDecodeCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  base::AutoLock lock(lock_);

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/cache_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                    locked_images_budget_.GetCurrentUsageSafe());
  } else {
    // Dump each of our caches.
    DumpImageMemoryForCache(decoded_images_, "cached", pmd);
    DumpImageMemoryForCache(at_raster_decoded_images_, "at_raster", pmd);
  }

  // Memory dump can't fail, always return true.
  return true;
}

void SoftwareImageDecodeCache::DumpImageMemoryForCache(
    const ImageMRUCache& cache,
    const char* cache_name,
    base::trace_event::ProcessMemoryDump* pmd) const {
  lock_.AssertAcquired();

  for (const auto& image_pair : cache) {
    int image_id = static_cast<int>(image_pair.first.frame_key().hash());
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/cache_0x%" PRIXPTR "/%s/image_%" PRIu64 "_id_%d",
        reinterpret_cast<uintptr_t>(this), cache_name,
        image_pair.second->tracing_id(), image_id);
    // CreateMemoryAllocatorDump will automatically add tracking values for the
    // total size. We also add a "locked_size" below.
    MemoryAllocatorDump* dump =
        image_pair.second->memory()->CreateMemoryAllocatorDump(
            dump_name.c_str(), pmd);
    DCHECK(dump);
    size_t locked_bytes =
        image_pair.second->is_locked() ? image_pair.first.locked_bytes() : 0u;
    dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                    locked_bytes);
  }
}

// SoftwareImageDecodeCacheKey
ImageDecodeCacheKey ImageDecodeCacheKey::FromDrawImage(const DrawImage& image,
                                                       SkColorType color_type) {
  const PaintImage::FrameKey frame_key = image.frame_key();

  const SkSize& scale = image.scale();
  // If the src_rect falls outside of the image, we need to clip it since
  // otherwise we might end up with uninitialized memory in the decode process.
  // Note that the scale is still unchanged and the target size is now a
  // function of the new src_rect.
  const gfx::Rect& src_rect = GetSrcRect(image);

  // Start with the exact target size. However, this will be adjusted below to
  // be either a mip level, the original size, or a subrect size. This is done
  // to keep memory accounting correct.
  gfx::Size target_size(
      SkScalarRoundToInt(std::abs(src_rect.width() * scale.width())),
      SkScalarRoundToInt(std::abs(src_rect.height() * scale.height())));

  // If the target size is empty, then we'll be skipping the decode anyway, so
  // the filter quality doesn't matter. Early out instead.
  if (target_size.IsEmpty()) {
    return ImageDecodeCacheKey(frame_key, src_rect, target_size,
                               image.target_color_space(), kLow_SkFilterQuality,
                               true /* can_use_original_decode */,
                               false /* should_use_subrect */);
  }

  // Start with the given filter quality.
  SkFilterQuality quality = image.filter_quality();

  int mip_level = MipMapUtil::GetLevelForSize(src_rect.size(), target_size);
  // If any of the following conditions hold, then use at most low filter
  // quality and adjust the target size to match the original image:
  // - Quality is none: We need a pixelated image, so we can't upgrade it.
  // - Format is 4444: Skia doesn't support scaling these, so use low
  //   filter quality.
  // - Mip level is 0: The required mip is the original image, so just use low
  //   filter quality.
  // - Matrix is not decomposable: There's perspective on this image and we
  //   can't determine the size, so use the original.
  if (quality == kNone_SkFilterQuality ||
      color_type == kARGB_4444_SkColorType || mip_level == 0 ||
      !image.matrix_is_decomposable()) {
    quality = std::min(quality, kLow_SkFilterQuality);
    // Update the size to be the original image size.
    target_size =
        gfx::Size(image.paint_image().width(), image.paint_image().height());
  } else {
    quality = kMedium_SkFilterQuality;
    // Update the target size to be a mip level size.
    // TODO(vmpstr): MipMapUtil and JPEG decoders disagree on what to do with
    // odd sizes. If width = 2k + 1, and the mip level is 1, then this will
    // return width = k; JPEG decoder, however, will support decoding to width =
    // k + 1. We need to figure out what to do in this case.
    SkSize mip_scale_adjustment =
        MipMapUtil::GetScaleAdjustmentForLevel(src_rect.size(), mip_level);
    target_size.set_width(src_rect.width() * mip_scale_adjustment.width());
    target_size.set_height(src_rect.height() * mip_scale_adjustment.height());
  }

  // Now the quality is fixed, determine the other parameters we need.
  bool can_use_original_size_decode =
      quality == kLow_SkFilterQuality || quality == kNone_SkFilterQuality;
  bool should_use_subrect = false;

  // If the original image is large, we might want to do a subrect instead if
  // the subrect would be kMemoryRatioToSubrect times smaller.
  if (can_use_original_size_decode &&
      (image.paint_image().width() >= kMinDimensionToSubrect ||
       image.paint_image().height() >= kMinDimensionToSubrect)) {
    base::CheckedNumeric<size_t> checked_original_size = 4u;
    checked_original_size *= image.paint_image().width();
    checked_original_size *= image.paint_image().height();
    size_t original_size = checked_original_size.ValueOrDefault(
        std::numeric_limits<size_t>::max());

    base::CheckedNumeric<size_t> checked_src_rect_size = 4u;
    checked_src_rect_size *= src_rect.width();
    checked_src_rect_size *= src_rect.height();
    size_t src_rect_size = checked_src_rect_size.ValueOrDefault(
        std::numeric_limits<size_t>::max());

    // If the sizes are such that we get good savings by subrecting, then do
    // that. Also update the target size to be the src rect size since that's
    // the rect we want to use.
    if (original_size > kMemoryThresholdToSubrect &&
        src_rect_size <= original_size * kMemoryRatioToSubrect) {
      should_use_subrect = true;
      can_use_original_size_decode = false;
      target_size = src_rect.size();
    }
  }

  return ImageDecodeCacheKey(frame_key, src_rect, target_size,
                             image.target_color_space(), quality,
                             can_use_original_size_decode, should_use_subrect);
}

ImageDecodeCacheKey::ImageDecodeCacheKey(
    PaintImage::FrameKey frame_key,
    const gfx::Rect& src_rect,
    const gfx::Size& target_size,
    const gfx::ColorSpace& target_color_space,
    SkFilterQuality filter_quality,
    bool can_use_original_size_decode,
    bool should_use_subrect)
    : frame_key_(frame_key),
      src_rect_(src_rect),
      target_size_(target_size),
      target_color_space_(target_color_space),
      filter_quality_(filter_quality),
      can_use_original_size_decode_(can_use_original_size_decode),
      should_use_subrect_(should_use_subrect) {
  if (can_use_original_size_decode_) {
    hash_ = frame_key_.hash();
  } else {
    // TODO(vmpstr): This is a mess. Maybe it's faster to just search the vector
    // always (forwards or backwards to account for LRU).
    uint64_t src_rect_hash = base::HashInts(
        static_cast<uint64_t>(base::HashInts(src_rect_.x(), src_rect_.y())),
        static_cast<uint64_t>(
            base::HashInts(src_rect_.width(), src_rect_.height())));

    uint64_t target_size_hash =
        base::HashInts(target_size_.width(), target_size_.height());

    hash_ = base::HashInts(base::HashInts(src_rect_hash, target_size_hash),
                           base::HashInts(frame_key_.hash(), filter_quality_));
  }
  // Include the target color space in the hash regardless of scaling.
  hash_ = base::HashInts(hash_, target_color_space.GetHash());
}

ImageDecodeCacheKey::ImageDecodeCacheKey(const ImageDecodeCacheKey& other) =
    default;

std::string ImageDecodeCacheKey::ToString() const {
  std::ostringstream str;
  str << "frame_key[" << frame_key_.ToString() << "] src_rect[" << src_rect_.x()
      << "," << src_rect_.y() << " " << src_rect_.width() << "x"
      << src_rect_.height() << "] target_size[" << target_size_.width() << "x"
      << target_size_.height() << "] target_color_space"
      << target_color_space_.ToString() << " filter_quality[" << filter_quality_
      << "] can_use_original_size_decode [" << can_use_original_size_decode_
      << "] should_use_subrect [" << should_use_subrect_ << "] hash [" << hash_
      << "]";
  return str.str();
}

// DecodedImage
SoftwareImageDecodeCache::DecodedImage::DecodedImage(
    const SkImageInfo& info,
    std::unique_ptr<base::DiscardableMemory> memory,
    const SkSize& src_rect_offset,
    uint64_t tracing_id)
    : locked_(true),
      image_info_(info),
      memory_(std::move(memory)),
      src_rect_offset_(src_rect_offset),
      tracing_id_(tracing_id) {
  SkPixmap pixmap(image_info_, memory_->data(), image_info_.minRowBytes());
  image_ = SkImage::MakeFromRaster(
      pixmap, [](const void* pixels, void* context) {}, nullptr);
}

SoftwareImageDecodeCache::DecodedImage::~DecodedImage() {
  DCHECK(!locked_);
  // lock_count | used  | last lock failed | result state
  // ===========+=======+==================+==================
  //  1         | false | false            | WASTED
  //  1         | false | true             | WASTED
  //  1         | true  | false            | USED
  //  1         | true  | true             | USED_RELOCK_FAILED
  //  >1        | false | false            | WASTED_RELOCKED
  //  >1        | false | true             | WASTED_RELOCKED
  //  >1        | true  | false            | USED_RELOCKED
  //  >1        | true  | true             | USED_RELOCKED
  // Note that it's important not to reorder the following enums, since the
  // numerical values are used in the histogram code.
  enum State : int {
    DECODED_IMAGE_STATE_WASTED,
    DECODED_IMAGE_STATE_USED,
    DECODED_IMAGE_STATE_USED_RELOCK_FAILED,
    DECODED_IMAGE_STATE_WASTED_RELOCKED,
    DECODED_IMAGE_STATE_USED_RELOCKED,
    DECODED_IMAGE_STATE_COUNT
  } state = DECODED_IMAGE_STATE_WASTED;

  if (usage_stats_.lock_count == 1) {
    if (!usage_stats_.used)
      state = DECODED_IMAGE_STATE_WASTED;
    else if (usage_stats_.last_lock_failed)
      state = DECODED_IMAGE_STATE_USED_RELOCK_FAILED;
    else
      state = DECODED_IMAGE_STATE_USED;
  } else {
    if (usage_stats_.used)
      state = DECODED_IMAGE_STATE_USED_RELOCKED;
    else
      state = DECODED_IMAGE_STATE_WASTED_RELOCKED;
  }

  UMA_HISTOGRAM_ENUMERATION("Renderer4.SoftwareImageDecodeState", state,
                            DECODED_IMAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.SoftwareImageDecodeState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
  if (usage_stats_.first_lock_out_of_raster)
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer4.SoftwareImageDecodeState.FirstLockWasted.OutOfRaster",
        usage_stats_.first_lock_wasted);
}

bool SoftwareImageDecodeCache::DecodedImage::Lock() {
  DCHECK(!locked_);
  bool success = memory_->Lock();
  if (!success) {
    usage_stats_.last_lock_failed = true;
    return false;
  }
  locked_ = true;
  ++usage_stats_.lock_count;
  return true;
}

void SoftwareImageDecodeCache::DecodedImage::Unlock() {
  DCHECK(locked_);
  memory_->Unlock();
  locked_ = false;
  if (usage_stats_.lock_count == 1)
    usage_stats_.first_lock_wasted = !usage_stats_.used;
}

// MemoryBudget
SoftwareImageDecodeCache::MemoryBudget::MemoryBudget(size_t limit_bytes)
    : limit_bytes_(limit_bytes), current_usage_bytes_(0u) {}

size_t SoftwareImageDecodeCache::MemoryBudget::AvailableMemoryBytes() const {
  size_t usage = GetCurrentUsageSafe();
  return usage >= limit_bytes_ ? 0u : (limit_bytes_ - usage);
}

void SoftwareImageDecodeCache::MemoryBudget::AddUsage(size_t usage) {
  current_usage_bytes_ += usage;
}

void SoftwareImageDecodeCache::MemoryBudget::SubtractUsage(size_t usage) {
  DCHECK_GE(current_usage_bytes_.ValueOrDefault(0u), usage);
  current_usage_bytes_ -= usage;
}

void SoftwareImageDecodeCache::MemoryBudget::ResetUsage() {
  current_usage_bytes_ = 0;
}

size_t SoftwareImageDecodeCache::MemoryBudget::GetCurrentUsageSafe() const {
  return current_usage_bytes_.ValueOrDie();
}

void SoftwareImageDecodeCache::OnMemoryStateChange(base::MemoryState state) {
  {
    base::AutoLock hold(lock_);
    switch (state) {
      case base::MemoryState::NORMAL:
        max_items_in_cache_ = kNormalMaxItemsInCache;
        break;
      case base::MemoryState::THROTTLED:
        max_items_in_cache_ = kThrottledMaxItemsInCache;
        break;
      case base::MemoryState::SUSPENDED:
        max_items_in_cache_ = kSuspendedMaxItemsInCache;
        break;
      case base::MemoryState::UNKNOWN:
        NOTREACHED();
        return;
    }
  }
}

void SoftwareImageDecodeCache::OnPurgeMemory() {
  base::AutoLock lock(lock_);
  ReduceCacheUsageUntilWithinLimit(0);
}

void SoftwareImageDecodeCache::CleanupDecodedImagesCache(
    const ImageKey& key,
    ImageMRUCache::iterator it) {
  lock_.AssertAcquired();
  auto vector_it = frame_key_to_image_keys_.find(key.frame_key());

  // TODO(sohanjg): Check if we can DCHECK here.
  if (vector_it != frame_key_to_image_keys_.end()) {
    auto iter =
        std::find(vector_it->second.begin(), vector_it->second.end(), key);
    DCHECK(iter != vector_it->second.end());
    vector_it->second.erase(iter);
    if (vector_it->second.empty())
      frame_key_to_image_keys_.erase(vector_it);
  }

  decoded_images_.Erase(it);
}

void SoftwareImageDecodeCache::CacheDecodedImages(
    const ImageKey& key,
    std::unique_ptr<DecodedImage> decoded_image) {
  lock_.AssertAcquired();
  frame_key_to_image_keys_[key.frame_key()].push_back(key);
  decoded_images_.Put(key, std::move(decoded_image));
}

}  // namespace cc
