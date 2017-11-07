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
// TODO(vmpstr): UMA how many image we should keep around. This number seems
// high.
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

// Tracing ID sequence for use in CacheEntry.
base::AtomicSequenceNumber g_next_tracing_id_;

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
    cache_->DecodeImageInTask(image_key_, image_, task_type_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageDecodeTaskCompleted(image_key_, task_type_);
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

SkImageInfo CreateImageInfo(const SkISize& size, SkColorType color_type) {
  return SkImageInfo::Make(size.width(), size.height(), color_type,
                           kPremul_SkAlphaType);
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

std::unique_ptr<base::DiscardableMemory> AllocateDiscardable(
    const SkImageInfo& info) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"), "AllocateDiscardable");
  return base::DiscardableMemoryAllocator::GetInstance()
      ->AllocateLockedDiscardableMemory(info.minRowBytes() * info.height());
}

}  // namespace

SoftwareImageDecodeCache::SoftwareImageDecodeCache(
    SkColorType color_type,
    size_t locked_memory_limit_bytes)
    : decoded_images_(ImageMRUCache::NO_AUTO_EVICT),
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
  ImageKey key = ImageKey::FromDrawImage(image, color_type_);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetTaskForImageAndRefInternal", "key",
               key.ToString());

  // If the target size is empty, we can skip this image during draw (and thus
  // we don't need to decode it or ref it).
  if (key.target_size().IsEmpty())
    return TaskResult(false);

  base::AutoLock lock(lock_);

  bool new_image_fits_in_memory =
      locked_images_budget_.AvailableMemoryBytes() >= key.locked_bytes();

  // Get or generate the cache entry.
  auto decoded_it = decoded_images_.Get(key);
  CacheEntry* cache_entry = nullptr;
  if (decoded_it == decoded_images_.end()) {
    // There is no reason to create a new entry if we know it won't fit anyway.
    if (!new_image_fits_in_memory)
      return TaskResult(false);
    cache_entry = AddCacheEntry(key);
    if (task_type == DecodeTaskType::USE_OUT_OF_RASTER_TASKS)
      cache_entry->mark_out_of_raster();
  } else {
    cache_entry = decoded_it->second.get();
  }
  DCHECK(cache_entry);

  if (!cache_entry->is_budgeted) {
    if (!new_image_fits_in_memory) {
      // We don't need to ref anything here because this image will be at
      // raster.
      return TaskResult(false);
    }
    AddBudgetForImage(key, cache_entry);
  }
  DCHECK(cache_entry->is_budgeted);

  // The rest of the code will return either true or a task, so we should ref
  // the image once now for the raster to unref.
  ++cache_entry->ref_count;

  // If we already have a locked entry, then we can just use that. Otherwise
  // we'll have to create a task.
  if (cache_entry->is_locked)
    return TaskResult(true);

  scoped_refptr<TileTask>& task =
      task_type == DecodeTaskType::USE_IN_RASTER_TASKS
          ? cache_entry->in_raster_task
          : cache_entry->out_of_raster_task;
  if (!task) {
    // Ref image once for the task.
    ++cache_entry->ref_count;
    task = base::MakeRefCounted<ImageDecodeTaskImpl>(this, key, image,
                                                     task_type, tracing_info);
  }
  return TaskResult(task);
}

void SoftwareImageDecodeCache::AddBudgetForImage(const ImageKey& key,
                                                 CacheEntry* entry) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::AddBudgetForImage", "key",
               key.ToString());
  lock_.AssertAcquired();

  DCHECK(!entry->is_budgeted);
  DCHECK_GE(locked_images_budget_.AvailableMemoryBytes(), key.locked_bytes());
  locked_images_budget_.AddUsage(key.locked_bytes());
  entry->is_budgeted = true;
}

void SoftwareImageDecodeCache::RemoveBudgetForImage(const ImageKey& key,
                                                    CacheEntry* entry) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::RemoveBudgetForImage", "key",
               key.ToString());
  lock_.AssertAcquired();

  DCHECK(entry->is_budgeted);
  locked_images_budget_.SubtractUsage(key.locked_bytes());
  entry->is_budgeted = false;
}

void SoftwareImageDecodeCache::UnrefImage(const DrawImage& image) {
  const ImageKey& key = ImageKey::FromDrawImage(image, color_type_);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::UnrefImage", "key", key.ToString());

  base::AutoLock lock(lock_);
  UnrefImage(key);
}

void SoftwareImageDecodeCache::UnrefImage(const ImageKey& key) {
  lock_.AssertAcquired();
  auto decoded_image_it = decoded_images_.Peek(key);
  DCHECK(decoded_image_it != decoded_images_.end());
  auto* entry = decoded_image_it->second.get();
  DCHECK_GT(entry->ref_count, 0);
  if (--entry->ref_count == 0) {
    if (entry->is_budgeted)
      RemoveBudgetForImage(key, entry);
    if (entry->is_locked)
      entry->Unlock();
  }
}

void SoftwareImageDecodeCache::DecodeImageInTask(const ImageKey& key,
                                                 const DrawImage& image,
                                                 DecodeTaskType task_type) {
  TRACE_EVENT1("cc", "SoftwareImageDecodeCache::DecodeImageInTask", "key",
               key.ToString());
  base::AutoLock lock(lock_);

  auto image_it = decoded_images_.Peek(key);
  DCHECK(image_it != decoded_images_.end());
  auto* cache_entry = image_it->second.get();
  // These two checks must be true because we're running this from a task, which
  // means that we've budgeted this entry when we got the task and the ref count
  // is also held by the task (released in OnTaskCompleted).
  DCHECK_GT(cache_entry->ref_count, 0);
  DCHECK(cache_entry->is_budgeted);

  DecodeImageIfNecessary(key, image, cache_entry);
  DCHECK(cache_entry->decode_failed || cache_entry->is_locked);
  RecordImageMipLevelUMA(
      MipMapUtil::GetLevelForSize(key.src_rect().size(), key.target_size()));
}

void SoftwareImageDecodeCache::DecodeImageIfNecessary(
    const ImageKey& key,
    const DrawImage& draw_image,
    CacheEntry* entry) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DecodeImageIfNecessary", "key",
               key.ToString());
  lock_.AssertAcquired();
  DCHECK_GT(entry->ref_count, 0);

  const PaintImage& paint_image = draw_image.paint_image();
  if (key.target_size().IsEmpty())
    entry->decode_failed = true;

  if (entry->decode_failed)
    return;

  if (entry->memory) {
    if (entry->is_locked)
      return;

    bool lock_succeeded = entry->Lock();
    // TODO(vmpstr): Deprecate the prepaint split, since it doesn't matter.
    RecordLockExistingCachedImageHistogram(TilePriority::NOW, lock_succeeded);

    if (lock_succeeded)
      return;
    entry->memory = nullptr;
  }

  const SkIRect full_size_rect =
      SkIRect::MakeWH(paint_image.width(), paint_image.height());
  const bool need_subset =
      (gfx::RectToSkIRect(key.src_rect()) != full_size_rect);
  const SkIRect target_size_rect =
      SkIRect::MakeWH(key.target_size().width(), key.target_size().height());
  std::unique_ptr<CacheEntry> local_cache_entry;
  {
    // Note that this block runs without the lock held so it's important not to
    // access |entry| here since it's coming from a map that can be accessed by
    // other threads.
    base::AutoUnlock release(lock_);

    // If this is a full size, unsubrected image, we will definitely need to do
    // a decode.
    if (!need_subset && target_size_rect == full_size_rect) {
      local_cache_entry = DoDecodeImage(key, paint_image);
    } else {
      // Use the full image decode to generate a scaled/subrected decode.
      // TODO(vmpstr): Change this part to find a good candidate first other
      // than the full decode. Also this part needs to handle decode to scale.
      DrawImage full_size_draw_image(
          paint_image, full_size_rect, kNone_SkFilterQuality, SkMatrix::I(),
          key.frame_key().frame_index(), key.target_color_space());
      auto decoded_draw_image = GetDecodedImageForDraw(full_size_draw_image);

      AutoDrawWithImageFinished auto_finish_draw(this, full_size_draw_image,
                                                 decoded_draw_image);
      if (!decoded_draw_image.image()) {
        local_cache_entry = nullptr;
      } else {
        local_cache_entry =
            GenerateCacheEntryFromCandidate(key, decoded_draw_image);
      }
    }
  }  // End of the unlock block.

  if (!local_cache_entry) {
    entry->decode_failed = true;
    return;
  }

  // Just in case someone else did this already, just unlock our work.
  // TODO(vmpstr): It's possible to have a pending decode state where the
  // thread would just block on a cv and wait for that decode to finish
  // instead of actually doing the work.
  if (entry->memory) {
    // This would have to be locked because we hold a ref count on the entry. So
    // if someone ever populated the entry with memory, they would not be able
    // to unlock it.
    DCHECK(entry->is_locked);
    // Unlock our local memory though.
    local_cache_entry->Unlock();
  } else {
    local_cache_entry->MoveImageMemoryTo(entry);
    DCHECK(entry->is_locked);
  }
}

std::unique_ptr<SoftwareImageDecodeCache::CacheEntry>
SoftwareImageDecodeCache::DoDecodeImage(const ImageKey& key,
                                        const PaintImage& paint_image) {
  SkISize target_size =
      SkISize::Make(key.target_size().width(), key.target_size().height());
  DCHECK(target_size == paint_image.GetSupportedDecodeSize(target_size));

  SkImageInfo target_info = CreateImageInfo(target_size, color_type_);
  std::unique_ptr<base::DiscardableMemory> target_pixels =
      AllocateDiscardable(target_info);
  DCHECK(target_pixels);

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DoDecodeImage - "
               "decode");
  DCHECK(!key.should_use_subrect());
  bool result = paint_image.Decode(target_pixels->data(), &target_info,
                                   key.target_color_space().ToSkColorSpace(),
                                   key.frame_key().frame_index());
  if (!result) {
    target_pixels->Unlock();
    return nullptr;
  }
  return std::make_unique<CacheEntry>(target_info, std::move(target_pixels),
                                      SkSize::Make(0, 0));
}

std::unique_ptr<SoftwareImageDecodeCache::CacheEntry>
SoftwareImageDecodeCache::GenerateCacheEntryFromCandidate(
    const ImageKey& key,
    const DecodedDrawImage& candidate_image) {
  SkISize target_size =
      SkISize::Make(key.target_size().width(), key.target_size().height());
  SkImageInfo target_info = CreateImageInfo(target_size, color_type_);
  std::unique_ptr<base::DiscardableMemory> target_pixels =
      AllocateDiscardable(target_info);
  DCHECK(target_pixels);

  if (key.should_use_subrect()) {
    TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
                 "SoftwareImageDecodeCache::GenerateCacheEntryFromCandidate - "
                 "subrect");
    bool result = candidate_image.image()->readPixels(
        target_info, target_pixels->data(), target_info.minRowBytes(),
        key.src_rect().x(), key.src_rect().y(), SkImage::kDisallow_CachingHint);
    // We have a decoded image, and we're reading into already allocated memory.
    // This should never fail.
    DCHECK(result);
    return std::make_unique<CacheEntry>(
        target_info.makeColorSpace(candidate_image.image()->refColorSpace()),
        std::move(target_pixels),
        SkSize::Make(-key.src_rect().x(), -key.src_rect().y()));
  }

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GenerateCacheEntryFromCandidate - "
               "scale");
  SkPixmap decoded_pixmap;
  bool result = candidate_image.image()->peekPixels(&decoded_pixmap);
  result = result && decoded_pixmap.extractSubset(
                         &decoded_pixmap, gfx::RectToSkIRect(key.src_rect()));
  DCHECK(result) << key.ToString();

  SkPixmap target_pixmap(target_info, target_pixels->data(),
                         target_info.minRowBytes());
  result = decoded_pixmap.scalePixels(target_pixmap, key.filter_quality());
  DCHECK(result) << key.ToString();
  return std::make_unique<CacheEntry>(
      target_info.makeColorSpace(candidate_image.image()->refColorSpace()),
      std::move(target_pixels),
      SkSize::Make(-key.src_rect().x(), -key.src_rect().y()));
}

DecodedDrawImage SoftwareImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  base::AutoLock hold(lock_);
  return GetDecodedImageForDrawInternal(
      ImageKey::FromDrawImage(draw_image, color_type_), draw_image);
}

DecodedDrawImage SoftwareImageDecodeCache::GetDecodedImageForDrawInternal(
    const ImageKey& key,
    const DrawImage& draw_image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::GetDecodedImageForDrawInternal",
               "key", key.ToString());

  lock_.AssertAcquired();
  auto decoded_it = decoded_images_.Get(key);
  CacheEntry* cache_entry = nullptr;
  if (decoded_it == decoded_images_.end())
    cache_entry = AddCacheEntry(key);
  else
    cache_entry = decoded_it->second.get();

  // We'll definitely ref this cache entry and use it.
  ++cache_entry->ref_count;
  cache_entry->mark_used();

  DecodeImageIfNecessary(key, draw_image, cache_entry);
  auto decoded_draw_image =
      DecodedDrawImage(cache_entry->image(), cache_entry->src_rect_offset(),
                       GetScaleAdjustment(key), GetDecodedFilterQuality(key));
  decoded_draw_image.set_at_raster_decode(!cache_entry->is_budgeted);
  return decoded_draw_image;
}

void SoftwareImageDecodeCache::DrawWithImageFinished(
    const DrawImage& image,
    const DecodedDrawImage& decoded_image) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "SoftwareImageDecodeCache::DrawWithImageFinished", "key",
               ImageKey::FromDrawImage(image, color_type_).ToString());
  UnrefImage(image);
}

void SoftwareImageDecodeCache::ReduceCacheUsageUntilWithinLimit(size_t limit) {
  TRACE_EVENT0("cc",
               "SoftwareImageDecodeCache::ReduceCacheUsageUntilWithinLimit");
  for (auto it = decoded_images_.rbegin();
       decoded_images_.size() > limit && it != decoded_images_.rend();) {
    if (it->second->ref_count != 0) {
      ++it;
      continue;
    }

    const ImageKey& key = it->first;
    auto vector_it = frame_key_to_image_keys_.find(key.frame_key());
    auto item_it =
        std::find(vector_it->second.begin(), vector_it->second.end(), key);
    DCHECK(item_it != vector_it->second.end());
    vector_it->second.erase(item_it);
    if (vector_it->second.empty())
      frame_key_to_image_keys_.erase(vector_it);

    it = decoded_images_.Erase(it);
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

  for (auto key_it = it->second.begin(); key_it != it->second.end();) {
    // This iterates over the ImageKey vector for the given skimage_id,
    // and deletes all entries from decoded_images_ corresponding to the
    // skimage_id.
    auto image_it = decoded_images_.Peek(*key_it);
    // TODO(sohanjg): Find an optimized way to cleanup locked images.
    if (image_it != decoded_images_.end() && image_it->second->ref_count == 0) {
      decoded_images_.Erase(image_it);
      key_it = it->second.erase(key_it);
    } else {
      ++key_it;
    }
  }
  if (it->second.empty())
    frame_key_to_image_keys_.erase(it);
}

void SoftwareImageDecodeCache::OnImageDecodeTaskCompleted(
    const ImageKey& key,
    DecodeTaskType task_type) {
  base::AutoLock hold(lock_);

  auto image_it = decoded_images_.Peek(key);
  DCHECK(image_it != decoded_images_.end());
  CacheEntry* cache_entry = image_it->second.get();
  auto& task = task_type == DecodeTaskType::USE_IN_RASTER_TASKS
                   ? cache_entry->in_raster_task
                   : cache_entry->out_of_raster_task;
  task = nullptr;

  UnrefImage(key);
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
    for (const auto& image_pair : decoded_images_) {
      int image_id = static_cast<int>(image_pair.first.frame_key().hash());
      CacheEntry* entry = image_pair.second.get();
      DCHECK(entry);
      // We might not have memory for this cache entry, depending on where int
      // he CacheEntry lifecycle we are. If we don't have memory, then we don't
      // have to record it in the dump.
      if (!entry->memory)
        continue;

      std::string dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/%s/image_%" PRIu64 "_id_%d",
          reinterpret_cast<uintptr_t>(this),
          entry->is_budgeted ? "budgeted" : "at_raster", entry->tracing_id(),
          image_id);
      // CreateMemoryAllocatorDump will automatically add tracking values for
      // the total size. We also add a "locked_size" below.
      MemoryAllocatorDump* dump =
          entry->memory->CreateMemoryAllocatorDump(dump_name.c_str(), pmd);
      DCHECK(dump);
      size_t locked_bytes =
          entry->is_locked ? image_pair.first.locked_bytes() : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_bytes);
    }
  }

  // Memory dump can't fail, always return true.
  return true;
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

// CacheEntry
SoftwareImageDecodeCache::CacheEntry::CacheEntry()
    : tracing_id_(g_next_tracing_id_.GetNext()) {}
SoftwareImageDecodeCache::CacheEntry::CacheEntry(
    const SkImageInfo& info,
    std::unique_ptr<base::DiscardableMemory> in_memory,
    const SkSize& src_rect_offset)
    : is_locked(true),
      memory(std::move(in_memory)),
      image_info_(info),
      src_rect_offset_(src_rect_offset),
      tracing_id_(g_next_tracing_id_.GetNext()) {
  DCHECK(memory);
  SkPixmap pixmap(image_info_, memory->data(), image_info_.minRowBytes());
  image_ = SkImage::MakeFromRaster(
      pixmap, [](const void* pixels, void* context) {}, nullptr);
}

SoftwareImageDecodeCache::CacheEntry::~CacheEntry() {
  DCHECK(!is_locked);
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

void SoftwareImageDecodeCache::CacheEntry::MoveImageMemoryTo(
    CacheEntry* entry) {
  DCHECK(!is_budgeted);
  DCHECK_EQ(ref_count, 0);

  // Copy/move most things except budgeted and ref counts.
  entry->decode_failed = decode_failed;
  entry->is_locked = is_locked;
  is_locked = false;

  entry->memory = std::move(memory);
  entry->image_info_ = std::move(image_info_);
  entry->src_rect_offset_ = std::move(src_rect_offset_);
  entry->image_ = std::move(image_);
}

bool SoftwareImageDecodeCache::CacheEntry::Lock() {
  if (!memory)
    return false;

  DCHECK(!is_locked);
  bool success = memory->Lock();
  if (!success) {
    usage_stats_.last_lock_failed = true;
    return false;
  }
  is_locked = true;
  ++usage_stats_.lock_count;
  return true;
}

void SoftwareImageDecodeCache::CacheEntry::Unlock() {
  if (!memory)
    return;

  DCHECK(is_locked);
  memory->Unlock();
  is_locked = false;
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

SoftwareImageDecodeCache::CacheEntry* SoftwareImageDecodeCache::AddCacheEntry(
    const ImageKey& key) {
  lock_.AssertAcquired();
  frame_key_to_image_keys_[key.frame_key()].push_back(key);
  auto it = decoded_images_.Put(key, std::make_unique<CacheEntry>());
  return it->second.get();
}

}  // namespace cc
