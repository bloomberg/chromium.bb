// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/image_decode_controller.h"

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/discardable_memory.h"
#include "cc/debug/devtools_instrumentation.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/skia_util.h"

namespace cc {
namespace {

// The amount of memory we can lock ahead of time (128MB). This limit is only
// used to inform the caller of the amount of space available in the cache. The
// caller can still request tasks which can cause this limit to be breached.
const size_t kLockedMemoryLimitBytes = 128 * 1024 * 1024;

// The largest single high quality image to try and process. Images above this
// size will drop down to medium quality.
const size_t kMaxHighQualityImageSizeBytes = 64 * 1024 * 1024;

// The number of entries to keep around in the cache. This limit can be breached
// if more items are locked. That is, locked items ignore this limit.
const size_t kMaxItemsInCache = 100;

class ImageDecodeTaskImpl : public ImageDecodeTask {
 public:
  ImageDecodeTaskImpl(ImageDecodeController* controller,
                      const ImageDecodeController::ImageKey& image_key,
                      const DrawImage& image,
                      uint64_t source_prepare_tiles_id)
      : controller_(controller),
        image_key_(image_key),
        image_(image),
        image_ref_(skia::SharePtr(image.image())),
        source_prepare_tiles_id_(source_prepare_tiles_id) {}

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT1("cc", "ImageDecodeTaskImpl::RunOnWorkerThread",
                 "source_prepare_tiles_id", source_prepare_tiles_id_);
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        image_ref_.get());
    controller_->DecodeImage(image_key_, image_);
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(TileTaskClient* client) override {}
  void CompleteOnOriginThread(TileTaskClient* client) override {
    controller_->RemovePendingTask(image_key_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  ImageDecodeController* controller_;
  ImageDecodeController::ImageKey image_key_;
  DrawImage image_;
  skia::RefPtr<const SkImage> image_ref_;
  uint64_t source_prepare_tiles_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

template <typename Type>
typename std::deque<Type>::iterator FindImage(
    std::deque<Type>* collection,
    const ImageDecodeControllerKey& key) {
  return std::find_if(collection->begin(), collection->end(),
                      [key](const Type& image) { return image.first == key; });
}

SkSize GetScaleAdjustment(const ImageDecodeControllerKey& key) {
  float x_scale =
      key.target_size().width() / static_cast<float>(key.src_rect().width());
  float y_scale =
      key.target_size().height() / static_cast<float>(key.src_rect().height());
  return SkSize::Make(x_scale, y_scale);
}

}  // namespace

ImageDecodeController::ImageDecodeController()
    : is_using_gpu_rasterization_(false),
      locked_images_budget_(kLockedMemoryLimitBytes) {}

ImageDecodeController::~ImageDecodeController() {
  DCHECK_EQ(0u, decoded_images_ref_counts_.size());
  DCHECK_EQ(0u, at_raster_decoded_images_ref_counts_.size());
}

bool ImageDecodeController::GetTaskForImageAndRef(
    const DrawImage& image,
    uint64_t prepare_tiles_id,
    scoped_refptr<ImageDecodeTask>* task) {
  // If the image already exists or if we're going to create a task for it, then
  // we'll likely need to ref this image (the exception is if we're prerolling
  // the image only). That means the image is or will be in the cache. When the
  // ref goes to 0, it will be unpinned but will remain in the cache. If the
  // image does not fit into the budget, then we don't ref this image, since it
  // will be decoded at raster time which is when it will be temporarily put in
  // the cache.
  ImageKey key = ImageKey::FromDrawImage(image);
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::GetTaskForImageAndRef", "key",
               key.ToString());

  // If the target size is empty, we can skip this image during draw (and thus
  // we don't need to decode it or ref it).
  if (key.target_size().IsEmpty()) {
    *task = nullptr;
    return false;
  }

  // If we're not going to do a scale, we will just create a task to preroll the
  // image the first time we see it. This doesn't need to account for memory.
  // TODO(vmpstr): We can also lock the original sized image, in which case it
  // does require memory bookkeeping.
  if (!CanHandleImage(key, image)) {
    base::AutoLock lock(lock_);
    if (prerolled_images_.count(key.image_id()) == 0) {
      scoped_refptr<ImageDecodeTask>& existing_task = pending_image_tasks_[key];
      if (!existing_task) {
        existing_task = make_scoped_refptr(
            new ImageDecodeTaskImpl(this, key, image, prepare_tiles_id));
      }
      *task = existing_task;
    } else {
      *task = nullptr;
    }
    return false;
  }

  base::AutoLock lock(lock_);

  // If we already have the image in cache, then we can return it.
  auto decoded_it = FindImage(&decoded_images_, key);
  bool new_image_fits_in_memory =
      locked_images_budget_.AvailableMemoryBytes() >= key.target_bytes();
  if (decoded_it != decoded_images_.end()) {
    if (decoded_it->second->is_locked() ||
        (new_image_fits_in_memory && decoded_it->second->Lock())) {
      RefImage(key);
      *task = nullptr;
      SanityCheckState(__LINE__, true);
      return true;
    }
    // If the image fits in memory, then we at least tried to lock it and
    // failed. This means that it's not valid anymore.
    if (new_image_fits_in_memory)
      decoded_images_.erase(decoded_it);
  }

  // If the task exists, return it.
  scoped_refptr<ImageDecodeTask>& existing_task = pending_image_tasks_[key];
  if (existing_task) {
    RefImage(key);
    *task = existing_task;
    SanityCheckState(__LINE__, true);
    return true;
  }

  // At this point, we have to create a new image/task, so we need to abort if
  // it doesn't fit into memory and there are currently no raster tasks that
  // would have already accounted for memory. The latter part is possible if
  // there's a running raster task that could not be canceled, and still has a
  // ref to the image that is now being reffed for the new schedule.
  if (!new_image_fits_in_memory && (decoded_images_ref_counts_.find(key) ==
                                    decoded_images_ref_counts_.end())) {
    *task = nullptr;
    SanityCheckState(__LINE__, true);
    return false;
  }

  // Actually create the task. RefImage will account for memory on the first
  // ref.
  RefImage(key);
  existing_task = make_scoped_refptr(
      new ImageDecodeTaskImpl(this, key, image, prepare_tiles_id));
  *task = existing_task;
  SanityCheckState(__LINE__, true);
  return true;
}

void ImageDecodeController::RefImage(const ImageKey& key) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::RefImage", "key", key.ToString());
  lock_.AssertAcquired();
  int ref = ++decoded_images_ref_counts_[key];
  if (ref == 1) {
    DCHECK_GE(locked_images_budget_.AvailableMemoryBytes(), key.target_bytes());
    locked_images_budget_.AddUsage(key.target_bytes());
  }
}

void ImageDecodeController::UnrefImage(const DrawImage& image) {
  // When we unref the image, there are several situations we need to consider:
  // 1. The ref did not reach 0, which means we have to keep the image locked.
  // 2. The ref reached 0, we should unlock it.
  //   2a. The image isn't in the locked cache because we didn't get to decode
  //       it yet (or failed to decode it).
  //   2b. Unlock the image but keep it in list.
  const ImageKey& key = ImageKey::FromDrawImage(image);
  DCHECK(CanHandleImage(key, image));
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::UnrefImage", "key", key.ToString());

  base::AutoLock lock(lock_);
  auto ref_count_it = decoded_images_ref_counts_.find(key);
  DCHECK(ref_count_it != decoded_images_ref_counts_.end());

  --ref_count_it->second;
  if (ref_count_it->second == 0) {
    decoded_images_ref_counts_.erase(ref_count_it);
    locked_images_budget_.SubtractUsage(key.target_bytes());

    auto decoded_image_it = FindImage(&decoded_images_, key);
    // If we've never decoded the image before ref reached 0, then we wouldn't
    // have it in our cache. This would happen if we canceled tasks.
    if (decoded_image_it == decoded_images_.end()) {
      SanityCheckState(__LINE__, true);
      return;
    }
    DCHECK(decoded_image_it->second->is_locked());
    decoded_image_it->second->Unlock();
  }
  SanityCheckState(__LINE__, true);
}

void ImageDecodeController::DecodeImage(const ImageKey& key,
                                        const DrawImage& image) {
  TRACE_EVENT1("cc", "ImageDecodeController::DecodeImage", "key",
               key.ToString());
  if (!CanHandleImage(key, image)) {
    image.image()->preroll();

    base::AutoLock lock(lock_);
    prerolled_images_.insert(key.image_id());
    // Erase the pending task from the queue, since the task won't be doing
    // anything useful after this function terminates. Since we don't preroll
    // images twice, this is actually not necessary but it behaves similar to
    // the other code path: when this function finishes, the task isn't in the
    // pending_image_tasks_ list.
    pending_image_tasks_.erase(key);
    return;
  }

  base::AutoLock lock(lock_);

  auto image_it = FindImage(&decoded_images_, key);
  if (image_it != decoded_images_.end()) {
    if (image_it->second->is_locked() || image_it->second->Lock()) {
      pending_image_tasks_.erase(key);
      return;
    }
    decoded_images_.erase(image_it);
  }

  scoped_refptr<DecodedImage> decoded_image;
  {
    base::AutoUnlock unlock(lock_);
    decoded_image = DecodeImageInternal(key, image.image());
  }

  // Erase the pending task from the queue, since the task won't be doing
  // anything useful after this function terminates. That is, if this image
  // needs to be decoded again, we have to create a new task.
  pending_image_tasks_.erase(key);

  // Abort if we failed to decode the image.
  if (!decoded_image)
    return;

  // At this point, it could have been the case that this image was decoded in
  // place by an already running raster task from a previous schedule. If that's
  // the case, then it would have already been placed into the cache (possibly
  // locked). Remove it if that was the case.
  image_it = FindImage(&decoded_images_, key);
  if (image_it != decoded_images_.end()) {
    if (image_it->second->is_locked() || image_it->second->Lock()) {
      // Make sure to unlock the decode we did in this function.
      decoded_image->Unlock();
      return;
    }
    decoded_images_.erase(image_it);
  }

  // We could have finished all of the raster tasks (cancelled) while this image
  // decode task was running, which means that we now have a locked image but no
  // ref counts. Unlock it immediately in this case.
  if (decoded_images_ref_counts_.find(key) ==
      decoded_images_ref_counts_.end()) {
    decoded_image->Unlock();
  }

  decoded_images_.push_back(AnnotatedDecodedImage(key, decoded_image));
  SanityCheckState(__LINE__, true);
}

scoped_refptr<ImageDecodeController::DecodedImage>
ImageDecodeController::DecodeImageInternal(const ImageKey& key,
                                           const SkImage* image) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::DecodeImageInternal", "key",
               key.ToString());

  // Get the decoded image first (at the original scale).
  SkImageInfo decoded_info = SkImageInfo::MakeN32Premul(
      key.src_rect().width(), key.src_rect().height());
  scoped_ptr<uint8_t[]> decoded_pixels;
  {
    TRACE_EVENT0(
        "disabled-by-default-cc.debug",
        "ImageDecodeController::DecodeImageInternal - allocate decoded pixels");
    decoded_pixels.reset(
        new uint8_t[decoded_info.minRowBytes() * decoded_info.height()]);
  }
  {
    TRACE_EVENT0("disabled-by-default-cc.debug",
                 "ImageDecodeController::DecodeImageInternal - read pixels");
    bool result = image->readPixels(
        decoded_info, decoded_pixels.get(), decoded_info.minRowBytes(),
        key.src_rect().x(), key.src_rect().y(), SkImage::kAllow_CachingHint);

    if (!result)
      return nullptr;
  }

  SkPixmap decoded_pixmap(decoded_info, decoded_pixels.get(),
                          decoded_info.minRowBytes());

  // Now scale the pixels into the destination size.
  DCHECK(!key.target_size().IsEmpty());
  SkImageInfo scaled_info = SkImageInfo::MakeN32Premul(
      key.target_size().width(), key.target_size().height());
  scoped_ptr<base::DiscardableMemory> scaled_pixels;
  {
    TRACE_EVENT0(
        "disabled-by-default-cc.debug",
        "ImageDecodeController::DecodeImageInternal - allocate scaled pixels");
    scaled_pixels = base::DiscardableMemoryAllocator::GetInstance()
                        ->AllocateLockedDiscardableMemory(
                            scaled_info.minRowBytes() * scaled_info.height());
  }
  SkPixmap scaled_pixmap(scaled_info, scaled_pixels->data(),
                         scaled_info.minRowBytes());
  // TODO(vmpstr): Start handling more than just high filter quality.
  DCHECK_EQ(kHigh_SkFilterQuality, key.filter_quality());
  {
    TRACE_EVENT0("disabled-by-default-cc.debug",
                 "ImageDecodeController::DecodeImageInternal - scale pixels");
    bool result =
        decoded_pixmap.scalePixels(scaled_pixmap, kHigh_SkFilterQuality);
    DCHECK(result);
  }
  return make_scoped_refptr(
      new DecodedImage(scaled_info, std::move(scaled_pixels),
                       SkSize::Make(-key.src_rect().x(), -key.src_rect().y())));
}

DecodedDrawImage ImageDecodeController::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  ImageKey key = ImageKey::FromDrawImage(draw_image);
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::GetDecodedImageAndRef", "key",
               key.ToString());
  if (!CanHandleImage(key, draw_image))
    return DecodedDrawImage(draw_image.image(), draw_image.filter_quality());

  // If the target size is empty, we can skip this image draw.
  if (key.target_size().IsEmpty())
    return DecodedDrawImage(nullptr, kNone_SkFilterQuality);

  base::AutoLock lock(lock_);
  auto decoded_images_it = FindImage(&decoded_images_, key);
  // If we found the image and it's locked, then return it. If it's not locked,
  // erase it from the cache since it might be put into the at-raster cache.
  scoped_refptr<DecodedImage> decoded_image;
  if (decoded_images_it != decoded_images_.end()) {
    decoded_image = decoded_images_it->second;
    if (decoded_image->is_locked()) {
      RefImage(key);
      SanityCheckState(__LINE__, true);
      return DecodedDrawImage(decoded_image->image(),
                              decoded_image->src_rect_offset(),
                              GetScaleAdjustment(key), kLow_SkFilterQuality);
    } else {
      decoded_images_.erase(decoded_images_it);
    }
  }

  // See if another thread already decoded this image at raster time. If so, we
  // can just use that result directly.
  auto at_raster_images_it = FindImage(&at_raster_decoded_images_, key);
  if (at_raster_images_it != at_raster_decoded_images_.end()) {
    DCHECK(at_raster_images_it->second->is_locked());
    RefAtRasterImage(key);
    SanityCheckState(__LINE__, true);
    auto decoded_draw_image =
        DecodedDrawImage(at_raster_images_it->second->image(),
                         at_raster_images_it->second->src_rect_offset(),
                         GetScaleAdjustment(key), kLow_SkFilterQuality);
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
    decoded_image = DecodeImageInternal(key, draw_image.image());

    // Skip the image if we couldn't decode it.
    if (!decoded_image)
      return DecodedDrawImage(nullptr, kNone_SkFilterQuality);
    check_at_raster_cache = true;
  }

  // While we unlocked the lock, it could be the case that another thread
  // already decoded this already and put it in the at-raster cache. Look it up
  // first.
  bool need_to_add_image_to_cache = true;
  if (check_at_raster_cache) {
    at_raster_images_it = FindImage(&at_raster_decoded_images_, key);
    if (at_raster_images_it != at_raster_decoded_images_.end()) {
      // We have to drop our decode, since the one in the cache is being used by
      // another thread.
      decoded_image->Unlock();
      decoded_image = at_raster_images_it->second;
      need_to_add_image_to_cache = false;
    }
  }

  // If we really are the first ones, or if the other thread already unlocked
  // the image, then put our work into at-raster time cache.
  if (need_to_add_image_to_cache) {
    at_raster_decoded_images_.push_back(
        AnnotatedDecodedImage(key, decoded_image));
  }

  DCHECK(decoded_image);
  DCHECK(decoded_image->is_locked());
  RefAtRasterImage(key);
  SanityCheckState(__LINE__, true);
  auto decoded_draw_image =
      DecodedDrawImage(decoded_image->image(), decoded_image->src_rect_offset(),
                       GetScaleAdjustment(key), kLow_SkFilterQuality);
  decoded_draw_image.set_at_raster_decode(true);
  return decoded_draw_image;
}

void ImageDecodeController::DrawWithImageFinished(
    const DrawImage& image,
    const DecodedDrawImage& decoded_image) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::DrawWithImageFinished", "key",
               ImageKey::FromDrawImage(image).ToString());
  ImageKey key = ImageKey::FromDrawImage(image);
  if (!decoded_image.image() || !CanHandleImage(key, image))
    return;

  if (decoded_image.is_at_raster_decode())
    UnrefAtRasterImage(key);
  else
    UnrefImage(image);
  SanityCheckState(__LINE__, false);
}

void ImageDecodeController::RefAtRasterImage(const ImageKey& key) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::RefAtRasterImage", "key",
               key.ToString());
  DCHECK(FindImage(&at_raster_decoded_images_, key) !=
         at_raster_decoded_images_.end());
  ++at_raster_decoded_images_ref_counts_[key];
}

void ImageDecodeController::UnrefAtRasterImage(const ImageKey& key) {
  TRACE_EVENT1("disabled-by-default-cc.debug",
               "ImageDecodeController::UnrefAtRasterImage", "key",
               key.ToString());
  base::AutoLock lock(lock_);

  auto ref_it = at_raster_decoded_images_ref_counts_.find(key);
  DCHECK(ref_it != at_raster_decoded_images_ref_counts_.end());
  --ref_it->second;
  if (ref_it->second == 0) {
    at_raster_decoded_images_ref_counts_.erase(ref_it);
    auto at_raster_image_it = FindImage(&at_raster_decoded_images_, key);
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
    auto image_it = FindImage(&decoded_images_, key);
    if (image_it == decoded_images_.end()) {
      if (decoded_images_ref_counts_.find(key) ==
          decoded_images_ref_counts_.end()) {
        at_raster_image_it->second->Unlock();
      }
      decoded_images_.push_back(*at_raster_image_it);
    } else if (image_it->second->is_locked()) {
      at_raster_image_it->second->Unlock();
    } else {
      DCHECK(decoded_images_ref_counts_.find(key) ==
             decoded_images_ref_counts_.end());
      at_raster_image_it->second->Unlock();
      decoded_images_.erase(image_it);
      decoded_images_.push_back(*at_raster_image_it);
    }
    at_raster_decoded_images_.erase(at_raster_image_it);
  }
}

bool ImageDecodeController::CanHandleImage(const ImageKey& key,
                                           const DrawImage& image) {
  // TODO(vmpstr): Handle GPU rasterization.
  if (is_using_gpu_rasterization_)
    return false;
  if (!CanHandleFilterQuality(key.filter_quality()))
    return false;
  return true;
}

bool ImageDecodeController::CanHandleFilterQuality(
    SkFilterQuality filter_quality) {
  // We don't need to handle low quality filters.
  if (filter_quality == kLow_SkFilterQuality ||
      filter_quality == kNone_SkFilterQuality) {
    return false;
  }

  // TODO(vmpstr): We need to start caching mipmaps for medium quality and
  // caching the interpolated values from those. For now, we don't have this.
  if (filter_quality == kMedium_SkFilterQuality)
    return false;
  DCHECK(filter_quality == kHigh_SkFilterQuality);
  return true;
}

void ImageDecodeController::ReduceCacheUsage() {
  TRACE_EVENT0("cc", "ImageDecodeController::ReduceCacheUsage");
  base::AutoLock lock(lock_);
  size_t num_to_remove = (decoded_images_.size() > kMaxItemsInCache)
                             ? (decoded_images_.size() - kMaxItemsInCache)
                             : 0;
  for (auto it = decoded_images_.begin();
       num_to_remove != 0 && it != decoded_images_.end();) {
    if (it->second->is_locked()) {
      ++it;
      continue;
    }

    it = decoded_images_.erase(it);
    --num_to_remove;
  }
}

void ImageDecodeController::RemovePendingTask(const ImageKey& key) {
  base::AutoLock lock(lock_);
  pending_image_tasks_.erase(key);
}

void ImageDecodeController::SetIsUsingGpuRasterization(
    bool is_using_gpu_rasterization) {
  if (is_using_gpu_rasterization_ == is_using_gpu_rasterization)
    return;
  is_using_gpu_rasterization_ = is_using_gpu_rasterization;

  base::AutoLock lock(lock_);

  DCHECK_EQ(0u, decoded_images_ref_counts_.size());
  DCHECK_EQ(0u, at_raster_decoded_images_ref_counts_.size());
  DCHECK(std::find_if(decoded_images_.begin(), decoded_images_.end(),
                      [](const AnnotatedDecodedImage& image) {
                        return image.second->is_locked();
                      }) == decoded_images_.end());
  DCHECK(std::find_if(at_raster_decoded_images_.begin(),
                      at_raster_decoded_images_.end(),
                      [](const AnnotatedDecodedImage& image) {
                        return image.second->is_locked();
                      }) == at_raster_decoded_images_.end());
  decoded_images_.clear();
  at_raster_decoded_images_.clear();
}

void ImageDecodeController::SanityCheckState(int line, bool lock_acquired) {
#if DCHECK_IS_ON()
  if (!lock_acquired) {
    base::AutoLock lock(lock_);
    SanityCheckState(line, true);
    return;
  }

  MemoryBudget budget(kLockedMemoryLimitBytes);
  for (const auto& annotated_image : decoded_images_) {
    auto ref_it = decoded_images_ref_counts_.find(annotated_image.first);
    if (annotated_image.second->is_locked()) {
      budget.AddUsage(annotated_image.first.target_bytes());
      DCHECK(ref_it != decoded_images_ref_counts_.end()) << line;
    } else {
      DCHECK(ref_it == decoded_images_ref_counts_.end() ||
             pending_image_tasks_.find(annotated_image.first) !=
                 pending_image_tasks_.end())
          << line;
    }
  }
  DCHECK_GE(budget.AvailableMemoryBytes(),
            locked_images_budget_.AvailableMemoryBytes())
      << line;
#endif  // DCHECK_IS_ON()
}

// ImageDecodeControllerKey
ImageDecodeControllerKey ImageDecodeControllerKey::FromDrawImage(
    const DrawImage& image) {
  const SkSize& scale = image.scale();
  gfx::Size target_size(
      SkScalarRoundToInt(std::abs(image.src_rect().width() * scale.width())),
      SkScalarRoundToInt(std::abs(image.src_rect().height() * scale.height())));

  // Start with the quality that was requested.
  SkFilterQuality quality = image.filter_quality();

  // If we're not going to do a scale, we can use low filter quality. Note that
  // checking if the sizes are the same is better than checking if scale is 1.f,
  // because even non-1 scale can result in the same (rounded) width/height.
  if (target_size.width() == image.src_rect().width() &&
      target_size.height() == image.src_rect().height()) {
    quality = std::min(quality, kLow_SkFilterQuality);
  }

  // Drop from high to medium if the image has perspective applied, the matrix
  // we applied wasn't decomposable, or if the scaled image will be too large.
  if (quality == kHigh_SkFilterQuality) {
    if (image.matrix_has_perspective() || !image.matrix_is_decomposable()) {
      quality = kMedium_SkFilterQuality;
    } else {
      base::CheckedNumeric<size_t> size = 4u;
      size *= target_size.width();
      size *= target_size.height();
      if (size.ValueOrDefault(std::numeric_limits<size_t>::max()) >
          kMaxHighQualityImageSizeBytes) {
        quality = kMedium_SkFilterQuality;
      }
    }
  }

  // Drop from medium to low if the matrix we applied wasn't decomposable or if
  // we're enlarging the image in both dimensions.
  if (quality == kMedium_SkFilterQuality) {
    if (!image.matrix_is_decomposable() ||
        (scale.width() >= 1.f && scale.height() >= 1.f)) {
      quality = kLow_SkFilterQuality;
    }
  }

  return ImageDecodeControllerKey(image.image()->uniqueID(),
                                  gfx::SkIRectToRect(image.src_rect()),
                                  target_size, quality);
}

ImageDecodeControllerKey::ImageDecodeControllerKey(
    uint32_t image_id,
    const gfx::Rect& src_rect,
    const gfx::Size& target_size,
    SkFilterQuality filter_quality)
    : image_id_(image_id),
      src_rect_(src_rect),
      target_size_(target_size),
      filter_quality_(filter_quality) {}

std::string ImageDecodeControllerKey::ToString() const {
  std::ostringstream str;
  str << "id[" << image_id_ << "] src_rect[" << src_rect_.x() << ","
      << src_rect_.y() << " " << src_rect_.width() << "x" << src_rect_.height()
      << "] target_size[" << target_size_.width() << "x"
      << target_size_.height() << "] filter_quality[" << filter_quality_ << "]";
  return str.str();
}

// DecodedImage
ImageDecodeController::DecodedImage::DecodedImage(
    const SkImageInfo& info,
    scoped_ptr<base::DiscardableMemory> memory,
    const SkSize& src_rect_offset)
    : locked_(true),
      image_info_(info),
      memory_(std::move(memory)),
      src_rect_offset_(src_rect_offset) {
  image_ = skia::AdoptRef(SkImage::NewFromRaster(
      image_info_, memory_->data(), image_info_.minRowBytes(),
      [](const void* pixels, void* context) {}, nullptr));
}

ImageDecodeController::DecodedImage::~DecodedImage() {
  DCHECK(!locked_);
}

bool ImageDecodeController::DecodedImage::Lock() {
  DCHECK(!locked_);
  bool success = memory_->Lock();
  if (!success)
    return false;
  locked_ = true;
  return true;
}

void ImageDecodeController::DecodedImage::Unlock() {
  DCHECK(locked_);
  memory_->Unlock();
  locked_ = false;
}

// MemoryBudget
ImageDecodeController::MemoryBudget::MemoryBudget(size_t limit_bytes)
    : limit_bytes_(limit_bytes), current_usage_bytes_(0u) {}

size_t ImageDecodeController::MemoryBudget::AvailableMemoryBytes() const {
  size_t usage = GetCurrentUsageSafe();
  return usage >= limit_bytes_ ? 0u : (limit_bytes_ - usage);
}

void ImageDecodeController::MemoryBudget::AddUsage(size_t usage) {
  current_usage_bytes_ += usage;
}

void ImageDecodeController::MemoryBudget::SubtractUsage(size_t usage) {
  DCHECK_GE(current_usage_bytes_.ValueOrDefault(0u), usage);
  current_usage_bytes_ -= usage;
}

void ImageDecodeController::MemoryBudget::ResetUsage() {
  current_usage_bytes_ = 0;
}

size_t ImageDecodeController::MemoryBudget::GetCurrentUsageSafe() const {
  return current_usage_bytes_.ValueOrDie();
}

}  // namespace cc
