// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_cache.h"

#include <inttypes.h>

#include "base/auto_reset.h"
#include "base/debug/alias.h"
#include "base/hash.h"
#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/memory_coordinator_client_registry.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/memory_dump_manager.h"
#include "cc/base/devtools_instrumentation.h"
#include "cc/raster/tile_task.h"
#include "cc/tiles/mipmap_util.h"
#include "components/viz/common/gpu/context_provider.h"
#include "components/viz/common/resources/resource_format_utils.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu_image_decode_cache.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/trace_util.h"

namespace cc {
namespace {

// The number or entries to keep in the cache, depending on the memory state of
// the system. This limit can be breached by in-use cache items, which cannot
// be deleted.
static const int kNormalMaxItemsInCache = 2000;
static const int kThrottledMaxItemsInCache = 100;
static const int kSuspendedMaxItemsInCache = 0;

// The factor by which to reduce the GPU memory size of the cache when in the
// THROTTLED memory state.
static const int kThrottledCacheSizeReductionFactor = 2;

// The maximum size in bytes of GPU memory in the cache while SUSPENDED or not
// visible. This limit can be breached by in-use cache items, which cannot be
// deleted.
static const int kSuspendedOrInvisibleMaxGpuImageBytes = 0;

// Returns true if an image would not be drawn and should therefore be
// skipped rather than decoded.
bool SkipImage(const DrawImage& draw_image) {
  if (!SkIRect::Intersects(draw_image.src_rect(), draw_image.image()->bounds()))
    return true;
  if (std::abs(draw_image.scale().width()) <
          std::numeric_limits<float>::epsilon() ||
      std::abs(draw_image.scale().height()) <
          std::numeric_limits<float>::epsilon()) {
    return true;
  }
  return false;
}

// Returns the filter quality to use for scaling the image to upload scale as
// well as for using when passing the decoded image to skia. Due to parity with
// SW and power impliciation, limit the filter quality to medium.
SkFilterQuality CalculateDesiredFilterQuality(const DrawImage& draw_image) {
  return std::min(kMedium_SkFilterQuality, draw_image.filter_quality());
}

// Calculate the mip level to upload-scale the image to before uploading. We use
// mip levels rather than exact scales to increase re-use of scaled images.
int CalculateUploadScaleMipLevel(const DrawImage& draw_image) {
  // Images which are being clipped will have color-bleeding if scaled.
  // TODO(ericrk): Investigate uploading clipped images to handle this case and
  // provide further optimization. crbug.com/620899
  if (draw_image.src_rect() != draw_image.image()->bounds())
    return 0;

  gfx::Size base_size(draw_image.image()->width(),
                      draw_image.image()->height());
  // Ceil our scaled size so that the mip map generated is guaranteed to be
  // larger. Take the abs of the scale, as mipmap functions don't handle
  // (and aren't impacted by) negative image dimensions.
  gfx::Size scaled_size =
      gfx::ScaleToCeiledSize(base_size, std::abs(draw_image.scale().width()),
                             std::abs(draw_image.scale().height()));

  return MipMapUtil::GetLevelForSize(base_size, scaled_size);
}

// Calculates the scale factor which can be used to scale an image to a given
// mip level.
SkSize CalculateScaleFactorForMipLevel(const DrawImage& draw_image,
                                       int mip_level) {
  gfx::Size base_size(draw_image.image()->width(),
                      draw_image.image()->height());
  return MipMapUtil::GetScaleAdjustmentForLevel(base_size, mip_level);
}

// Calculates the size of a given mip level.
gfx::Size CalculateSizeForMipLevel(const DrawImage& draw_image, int mip_level) {
  gfx::Size base_size(draw_image.image()->width(),
                      draw_image.image()->height());
  return MipMapUtil::GetSizeForLevel(base_size, mip_level);
}

// Draws and scales the provided |draw_image| into the |target_pixmap|. If the
// draw/scale can be done directly, calls directly into SkImage::scalePixels,
// if not, decodes to a compatible temporary pixmap and then converts that into
// the |target_pixmap|.
bool DrawAndScaleImage(const DrawImage& draw_image, SkPixmap* target_pixmap) {
  const SkImage* image = draw_image.image().get();
  if (image->dimensions() == target_pixmap->bounds().size() ||
      target_pixmap->info().colorType() == kN32_SkColorType) {
    // If no scaling is occurring, or if the target colortype is already N32,
    // just scale directly.
    return image->scalePixels(*target_pixmap,
                              CalculateDesiredFilterQuality(draw_image),
                              SkImage::kDisallow_CachingHint);
  }

  // If the target colortype is not N32, it may be impossible to scale
  // directly. Instead scale into an N32 pixmap, and convert that into the
  // |target_pixmap|.
  SkImageInfo decode_info =
      target_pixmap->info().makeColorType(kN32_SkColorType);
  SkBitmap decode_bitmap;
  if (!decode_bitmap.tryAllocPixels(decode_info))
    return false;
  SkPixmap decode_pixmap(decode_bitmap.info(), decode_bitmap.getPixels(),
                         decode_bitmap.rowBytes());
  if (!image->scalePixels(decode_pixmap,
                          CalculateDesiredFilterQuality(draw_image),
                          SkImage::kDisallow_CachingHint))
    return false;
  return decode_pixmap.readPixels(*target_pixmap);
}

}  // namespace

// static
GpuImageDecodeCache::InUseCacheKey
GpuImageDecodeCache::InUseCacheKey::FromDrawImage(const DrawImage& draw_image) {
  return InUseCacheKey(draw_image);
}

// Extract the information to uniquely identify a DrawImage for the purposes of
// the |in_use_cache_|.
GpuImageDecodeCache::InUseCacheKey::InUseCacheKey(const DrawImage& draw_image)
    : image_id(draw_image.image()->uniqueID()),
      mip_level(CalculateUploadScaleMipLevel(draw_image)),
      filter_quality(CalculateDesiredFilterQuality(draw_image)),
      target_color_space(draw_image.target_color_space()) {}

bool GpuImageDecodeCache::InUseCacheKey::operator==(
    const InUseCacheKey& other) const {
  return image_id == other.image_id && mip_level == other.mip_level &&
         filter_quality == other.filter_quality &&
         target_color_space == other.target_color_space;
}

size_t GpuImageDecodeCache::InUseCacheKeyHash::operator()(
    const InUseCacheKey& cache_key) const {
  return base::HashInts(
      cache_key.target_color_space.GetHash(),
      base::HashInts(
          cache_key.image_id,
          base::HashInts(cache_key.mip_level, cache_key.filter_quality)));
}

GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(
    scoped_refptr<ImageData> image_data)
    : image_data(std::move(image_data)) {}
GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(const InUseCacheEntry&) =
    default;
GpuImageDecodeCache::InUseCacheEntry::InUseCacheEntry(InUseCacheEntry&&) =
    default;
GpuImageDecodeCache::InUseCacheEntry::~InUseCacheEntry() = default;

// Task which decodes an image and stores the result in discardable memory.
// This task does not use GPU resources and can be run on any thread.
class ImageDecodeTaskImpl : public TileTask {
 public:
  ImageDecodeTaskImpl(GpuImageDecodeCache* cache,
                      const DrawImage& draw_image,
                      const ImageDecodeCache::TracingInfo& tracing_info,
                      GpuImageDecodeCache::DecodeTaskType task_type)
      : TileTask(true),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info),
        task_type_(task_type) {
    DCHECK(!SkipImage(draw_image));
  }

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageDecodeTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    devtools_instrumentation::ScopedImageDecodeTask image_decode_task(
        image_.image().get(),
        devtools_instrumentation::ScopedImageDecodeTask::kGpu,
        ImageDecodeCache::ToScopedTaskType(tracing_info_.task_type));
    cache_->DecodeImage(image_, tracing_info_.task_type);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageDecodeTaskCompleted(image_, task_type_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  GpuImageDecodeCache* cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;
  const GpuImageDecodeCache::DecodeTaskType task_type_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

// Task which creates an image from decoded data. Typically this involves
// uploading data to the GPU, which requires this task be run on the non-
// concurrent thread.
class ImageUploadTaskImpl : public TileTask {
 public:
  ImageUploadTaskImpl(GpuImageDecodeCache* cache,
                      const DrawImage& draw_image,
                      scoped_refptr<TileTask> decode_dependency,
                      const ImageDecodeCache::TracingInfo& tracing_info)
      : TileTask(false),
        cache_(cache),
        image_(draw_image),
        tracing_info_(tracing_info) {
    DCHECK(!SkipImage(draw_image));
    // If an image is already decoded and locked, we will not generate a
    // decode task.
    if (decode_dependency)
      dependencies_.push_back(std::move(decode_dependency));
  }

  // Override from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageUploadTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", tracing_info_.prepare_tiles_id);
    cache_->UploadImage(image_);
  }

  // Overridden from TileTask:
  void OnTaskCompleted() override {
    cache_->OnImageUploadTaskCompleted(image_);
  }

 protected:
  ~ImageUploadTaskImpl() override {}

 private:
  GpuImageDecodeCache* cache_;
  DrawImage image_;
  const ImageDecodeCache::TracingInfo tracing_info_;

  DISALLOW_COPY_AND_ASSIGN(ImageUploadTaskImpl);
};

GpuImageDecodeCache::DecodedImageData::DecodedImageData() = default;
GpuImageDecodeCache::DecodedImageData::~DecodedImageData() {
  ResetData();
}

bool GpuImageDecodeCache::DecodedImageData::Lock() {
  DCHECK(!is_locked_);
  is_locked_ = data_->Lock();
  if (is_locked_)
    ++usage_stats_.lock_count;
  return is_locked_;
}

void GpuImageDecodeCache::DecodedImageData::Unlock() {
  DCHECK(is_locked_);
  data_->Unlock();
  if (usage_stats_.lock_count == 1)
    usage_stats_.first_lock_wasted = !usage_stats_.used;
  is_locked_ = false;
}

void GpuImageDecodeCache::DecodedImageData::SetLockedData(
    std::unique_ptr<base::DiscardableMemory> data,
    bool out_of_raster) {
  DCHECK(!is_locked_);
  DCHECK(data);
  DCHECK(!data_);
  DCHECK_EQ(usage_stats_.lock_count, 1);
  data_ = std::move(data);
  is_locked_ = true;
  usage_stats_.first_lock_out_of_raster = out_of_raster;
}

void GpuImageDecodeCache::DecodedImageData::ResetData() {
  DCHECK(!is_locked_);
  if (data_)
    ReportUsageStats();
  data_ = nullptr;
  usage_stats_ = UsageStats();
}

void GpuImageDecodeCache::DecodedImageData::ReportUsageStats() const {
  // lock_count │ used  │ result state
  // ═══════════╪═══════╪══════════════════
  //  1         │ false │ WASTED_ONCE
  //  1         │ true  │ USED_ONCE
  //  >1        │ false │ WASTED_RELOCKED
  //  >1        │ true  │ USED_RELOCKED
  // Note that it's important not to reorder the following enums, since the
  // numerical values are used in the histogram code.
  enum State : int {
    DECODED_IMAGE_STATE_WASTED_ONCE,
    DECODED_IMAGE_STATE_USED_ONCE,
    DECODED_IMAGE_STATE_WASTED_RELOCKED,
    DECODED_IMAGE_STATE_USED_RELOCKED,
    DECODED_IMAGE_STATE_COUNT
  } state = DECODED_IMAGE_STATE_WASTED_ONCE;

  if (usage_stats_.lock_count == 1) {
    if (usage_stats_.used)
      state = DECODED_IMAGE_STATE_USED_ONCE;
    else
      state = DECODED_IMAGE_STATE_WASTED_ONCE;
  } else {
    if (usage_stats_.used)
      state = DECODED_IMAGE_STATE_USED_RELOCKED;
    else
      state = DECODED_IMAGE_STATE_WASTED_RELOCKED;
  }

  UMA_HISTOGRAM_ENUMERATION("Renderer4.GpuImageDecodeState", state,
                            DECODED_IMAGE_STATE_COUNT);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageDecodeState.FirstLockWasted",
                        usage_stats_.first_lock_wasted);
  if (usage_stats_.first_lock_out_of_raster)
    UMA_HISTOGRAM_BOOLEAN(
        "Renderer4.GpuImageDecodeState.FirstLockWasted.OutOfRaster",
        usage_stats_.first_lock_wasted);
}

GpuImageDecodeCache::UploadedImageData::UploadedImageData() = default;
GpuImageDecodeCache::UploadedImageData::~UploadedImageData() {
  SetImage(nullptr);
}

void GpuImageDecodeCache::UploadedImageData::SetImage(sk_sp<SkImage> image) {
  DCHECK(!image_ || !image);
  if (image_) {
    ReportUsageStats();
    usage_stats_ = UsageStats();
  }
  image_ = std::move(image);
}

void GpuImageDecodeCache::UploadedImageData::ReportUsageStats() const {
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageUploadState.Used",
                        usage_stats_.used);
  UMA_HISTOGRAM_BOOLEAN("Renderer4.GpuImageUploadState.FirstRefWasted",
                        usage_stats_.first_ref_wasted);
}

GpuImageDecodeCache::ImageData::ImageData(
    DecodedDataMode mode,
    size_t size,
    const gfx::ColorSpace& target_color_space,
    const SkImage::DeferredTextureImageUsageParams& upload_params)
    : mode(mode),
      size(size),
      target_color_space(target_color_space),
      upload_params(upload_params) {}

GpuImageDecodeCache::ImageData::~ImageData() {
  // We should never delete ImageData while it is in use or before it has been
  // cleaned up.
  DCHECK_EQ(0u, upload.ref_count);
  DCHECK_EQ(0u, decode.ref_count);
  DCHECK_EQ(false, decode.is_locked());
  // This should always be cleaned up before deleting the image, as it needs to
  // be freed with the GL context lock held.
  DCHECK(!upload.image());
}

GpuImageDecodeCache::GpuImageDecodeCache(viz::ContextProvider* context,
                                         viz::ResourceFormat decode_format,
                                         size_t max_working_set_bytes,
                                         size_t max_cache_bytes)
    : format_(decode_format),
      context_(context),
      persistent_cache_(PersistentCache::NO_AUTO_EVICT),
      max_working_set_bytes_(max_working_set_bytes),
      normal_max_cache_bytes_(max_cache_bytes) {
  DCHECK_GE(max_working_set_bytes_, normal_max_cache_bytes_);

  // Acquire the context_lock so that we can safely retrieve the
  // GrContextThreadSafeProxy. This proxy can then be used with no lock held.
  {
    viz::ContextProvider::ScopedContextLock context_lock(context_);
    context_threadsafe_proxy_ = sk_sp<GrContextThreadSafeProxy>(
        context->GrContext()->threadSafeProxy());
  }

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeCache", base::ThreadTaskRunnerHandle::Get());
  }
  // Register this component with base::MemoryCoordinatorClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Register(this);
}

GpuImageDecodeCache::~GpuImageDecodeCache() {
  // Debugging crbug.com/650234.
  CHECK_EQ(0u, in_use_cache_.size());

  // SetShouldAggressivelyFreeResources will zero our limits and free all
  // outstanding image memory.
  SetShouldAggressivelyFreeResources(true);

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
  // Unregister this component with memory_coordinator::ClientRegistry.
  base::MemoryCoordinatorClientRegistry::GetInstance()->Unregister(this);
}

bool GpuImageDecodeCache::GetTaskForImageAndRef(const DrawImage& draw_image,
                                                const TracingInfo& tracing_info,
                                                scoped_refptr<TileTask>* task) {
  DCHECK_EQ(tracing_info.task_type, TaskType::kInRaster);
  return GetTaskForImageAndRefInternal(
      draw_image, tracing_info, DecodeTaskType::PART_OF_UPLOAD_TASK, task);
}

bool GpuImageDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(
    const DrawImage& draw_image,
    scoped_refptr<TileTask>* task) {
  return GetTaskForImageAndRefInternal(
      draw_image, TracingInfo(0, TilePriority::NOW, TaskType::kOutOfRaster),
      DecodeTaskType::STAND_ALONE_DECODE_TASK, task);
}

bool GpuImageDecodeCache::GetTaskForImageAndRefInternal(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type,
    scoped_refptr<TileTask>* task) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetTaskForImageAndRef");
  if (SkipImage(draw_image)) {
    *task = nullptr;
    return false;
  }

  base::AutoLock lock(lock_);
  const auto image_id = draw_image.image()->uniqueID();
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  scoped_refptr<ImageData> new_data;
  if (!image_data) {
    // We need an ImageData, create one now.
    new_data = CreateImageData(draw_image);
    image_data = new_data.get();
  } else if (image_data->is_at_raster) {
    // Image is at-raster, just return, this usage will be at-raster as well.
    *task = nullptr;
    return false;
  } else if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image, so just return.
    *task = nullptr;
    return false;
  } else if (image_data->upload.image()) {
    // The image is already uploaded, ref and return.
    RefImage(draw_image);
    *task = nullptr;
    return true;
  } else if (task_type == DecodeTaskType::PART_OF_UPLOAD_TASK &&
             image_data->upload.task) {
    // We had an existing upload task, ref the image and return the task.
    RefImage(draw_image);
    *task = image_data->upload.task;
    return true;
  } else if (task_type == DecodeTaskType::STAND_ALONE_DECODE_TASK &&
             image_data->decode.stand_alone_task) {
    // We had an existing out of raster task, ref the image and return the task.
    RefImage(draw_image);
    *task = image_data->decode.stand_alone_task;
    return true;
  }

  // Ensure that the image we're about to decode/upload will fit in memory.
  if (!EnsureCapacity(image_data->size)) {
    // Image will not fit, do an at-raster decode.
    *task = nullptr;
    return false;
  }

  // If we had to create new image data, add it to our map now that we know it
  // will fit.
  if (new_data)
    persistent_cache_.Put(image_id, std::move(new_data));

  // Ref the image before creating a task - this ref is owned by the caller, and
  // it is their responsibility to release it by calling UnrefImage.
  RefImage(draw_image);

  if (task_type == DecodeTaskType::PART_OF_UPLOAD_TASK) {
    // Ref image and create a upload and decode tasks. We will release this ref
    // in UploadTaskCompleted.
    RefImage(draw_image);
    *task = make_scoped_refptr(new ImageUploadTaskImpl(
        this, draw_image,
        GetImageDecodeTaskAndRef(draw_image, tracing_info, task_type),
        tracing_info));
    image_data->upload.task = *task;
  } else {
    *task = GetImageDecodeTaskAndRef(draw_image, tracing_info, task_type);
  }

  return true;
}

void GpuImageDecodeCache::UnrefImage(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImage");
  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image);
}

DecodedDrawImage GpuImageDecodeCache::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  TRACE_EVENT0("cc", "GpuImageDecodeCache::GetDecodedImageForDraw");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  context_->GetLock()->AssertAcquired();

  // If we're skipping the image, then the filter quality doesn't matter.
  if (SkipImage(draw_image))
    return DecodedDrawImage(nullptr, kNone_SkFilterQuality);

  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  if (!image_data) {
    // We didn't find the image, create a new entry.
    auto data = CreateImageData(draw_image);
    image_data = data.get();
    persistent_cache_.Put(draw_image.image()->uniqueID(), std::move(data));
  }

  if (!image_data->upload.budgeted) {
    // If image data is not budgeted by this point, it is at-raster.
    image_data->is_at_raster = true;
  }

  // Ref the image and decode so that they stay alive while we are
  // decoding/uploading.
  RefImage(draw_image);
  RefImageDecode(draw_image);

  // We may or may not need to decode and upload the image we've found, the
  // following functions early-out to if we already decoded.
  DecodeImageIfNecessary(draw_image, image_data, TaskType::kInRaster);
  UploadImageIfNecessary(draw_image, image_data);
  // Unref the image decode, but not the image. The image ref will be released
  // in DrawWithImageFinished.
  UnrefImageDecode(draw_image);

  sk_sp<SkImage> image = image_data->upload.image();
  image_data->upload.mark_used();
  DCHECK(image || image_data->decode.decode_failure);

  SkSize scale_factor = CalculateScaleFactorForMipLevel(
      draw_image, image_data->upload_params.fPreScaleMipLevel);
  DecodedDrawImage decoded_draw_image(
      std::move(image), SkSize(), scale_factor,
      CalculateDesiredFilterQuality(draw_image));
  decoded_draw_image.set_at_raster_decode(image_data->is_at_raster);
  return decoded_draw_image;
}

void GpuImageDecodeCache::DrawWithImageFinished(
    const DrawImage& draw_image,
    const DecodedDrawImage& decoded_draw_image) {
  TRACE_EVENT0("cc", "GpuImageDecodeCache::DrawWithImageFinished");

  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  context_->GetLock()->AssertAcquired();

  if (SkipImage(draw_image))
    return;

  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image);

  // We are mid-draw and holding the context lock, ensure we clean up any
  // textures (especially at-raster), which may have just been marked for
  // deletion by UnrefImage.
  DeletePendingImages();
}

void GpuImageDecodeCache::ReduceCacheUsage() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::ReduceCacheUsage");
  base::AutoLock lock(lock_);
  EnsureCapacity(0);
}

void GpuImageDecodeCache::SetShouldAggressivelyFreeResources(
    bool aggressively_free_resources) {
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::SetShouldAggressivelyFreeResources",
               "agressive_free_resources", aggressively_free_resources);
  if (aggressively_free_resources) {
    viz::ContextProvider::ScopedContextLock context_lock(context_);
    base::AutoLock lock(lock_);
    // We want to keep as little in our cache as possible. Set our memory limit
    // to zero and EnsureCapacity to clean up memory.
    cached_bytes_limit_ = kSuspendedOrInvisibleMaxGpuImageBytes;
    EnsureCapacity(0);

    // We are holding the context lock, so finish cleaning up deleted images
    // now.
    DeletePendingImages();
  } else {
    base::AutoLock lock(lock_);
    cached_bytes_limit_ = normal_max_cache_bytes_;
  }
}

void GpuImageDecodeCache::ClearCache() {
  base::AutoLock lock(lock_);
  for (auto& entry : persistent_cache_) {
    if (entry.second->decode.ref_count != 0 ||
        entry.second->upload.ref_count != 0) {
      // Orphan the entry so it will be deleted once no longer in use.
      entry.second->is_orphaned = true;
    } else if (entry.second->upload.image()) {
      bytes_used_ -= entry.second->size;
      images_pending_deletion_.push_back(entry.second->upload.image());
      entry.second->upload.SetImage(nullptr);
      entry.second->upload.budgeted = false;
    }
  }
  persistent_cache_.Clear();
}

size_t GpuImageDecodeCache::GetMaximumMemoryLimitBytes() const {
  return normal_max_cache_bytes_;
}

void GpuImageDecodeCache::NotifyImageUnused(uint32_t skimage_id) {
  auto it = persistent_cache_.Peek(skimage_id);
  if (it != persistent_cache_.end()) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      it->second->is_orphaned = true;
    } else if (it->second->upload.image()) {
      DCHECK(!it->second->decode.is_locked());
      bytes_used_ -= it->second->size;
      images_pending_deletion_.push_back(it->second->upload.image());
      it->second->upload.SetImage(nullptr);
      it->second->upload.budgeted = false;
    }
    persistent_cache_.Erase(it);
  }
}

bool GpuImageDecodeCache::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  using base::trace_event::MemoryAllocatorDump;
  using base::trace_event::MemoryAllocatorDumpGuid;
  using base::trace_event::MemoryDumpLevelOfDetail;

  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnMemoryDump");

  if (args.level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND) {
    std::string dump_name = base::StringPrintf(
        "cc/image_memory/cache_0x%" PRIXPTR, reinterpret_cast<uintptr_t>(this));
    MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(dump_name);
    dump->AddScalar(MemoryAllocatorDump::kNameSize,
                    MemoryAllocatorDump::kUnitsBytes, bytes_used_);

    // Early out, no need for more detail in a BACKGROUND dump.
    return true;
  }

  for (const auto& image_pair : persistent_cache_) {
    const ImageData* image_data = image_pair.second.get();
    const uint32_t image_id = image_pair.first;

    // If we have discardable decoded data, dump this here.
    if (image_data->decode.data()) {
      std::string discardable_dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/discardable/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      MemoryAllocatorDump* dump =
          image_data->decode.data()->CreateMemoryAllocatorDump(
              discardable_dump_name.c_str(), pmd);
      // Dump the "locked_size" as an additional column.
      // This lets us see the amount of discardable which is contributing to
      // memory pressure.
      size_t locked_size =
          image_data->decode.is_locked() ? image_data->size : 0u;
      dump->AddScalar("locked_size", MemoryAllocatorDump::kUnitsBytes,
                      locked_size);
    }

    // If we have an uploaded image (that is actually on the GPU, not just a
    // CPU
    // wrapper), upload it here.
    if (image_data->upload.image() &&
        image_data->mode == DecodedDataMode::GPU) {
      std::string gpu_dump_name = base::StringPrintf(
          "cc/image_memory/cache_0x%" PRIXPTR "/gpu/image_%d",
          reinterpret_cast<uintptr_t>(this), image_id);
      MemoryAllocatorDump* dump = pmd->CreateAllocatorDump(gpu_dump_name);
      dump->AddScalar(MemoryAllocatorDump::kNameSize,
                      MemoryAllocatorDump::kUnitsBytes, image_data->size);

      // Create a global shred GUID to associate this data with its GPU
      // process
      // counterpart.
      GLuint gl_id = skia::GrBackendObjectToGrGLTextureInfo(
                         image_data->upload.image()->getTextureHandle(
                             false /* flushPendingGrContextIO */))
                         ->fID;
      MemoryAllocatorDumpGuid guid = gl::GetGLTextureClientGUIDForTracing(
          context_->ContextSupport()->ShareGroupTracingGUID(), gl_id);

      // kImportance is somewhat arbitrary - we chose 3 to be higher than the
      // value used in the GPU process (1), and Skia (2), causing us to appear
      // as the owner in memory traces.
      const int kImportance = 3;
      pmd->CreateSharedGlobalAllocatorDump(guid);
      pmd->AddOwnershipEdge(dump->guid(), guid, kImportance);
    }
  }

  return true;
}

void GpuImageDecodeCache::DecodeImage(const DrawImage& draw_image,
                                      TaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::DecodeImage");
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(!image_data->is_at_raster);
  DecodeImageIfNecessary(draw_image, image_data, task_type);
}

void GpuImageDecodeCache::UploadImage(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UploadImage");
  viz::ContextProvider::ScopedContextLock context_lock(context_);
  base::AutoLock lock(lock_);
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(!image_data->is_at_raster);
  UploadImageIfNecessary(draw_image, image_data);
}

void GpuImageDecodeCache::OnImageDecodeTaskCompleted(
    const DrawImage& draw_image,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageDecodeTaskCompleted");
  base::AutoLock lock(lock_);
  // Decode task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  if (task_type == DecodeTaskType::PART_OF_UPLOAD_TASK) {
    DCHECK(image_data->decode.task);
    image_data->decode.task = nullptr;
  } else {
    DCHECK(task_type == DecodeTaskType::STAND_ALONE_DECODE_TASK);
    DCHECK(image_data->decode.stand_alone_task);
    image_data->decode.stand_alone_task = nullptr;
  }

  // While the decode task is active, we keep a ref on the decoded data.
  // Release that ref now.
  UnrefImageDecode(draw_image);
}

void GpuImageDecodeCache::OnImageUploadTaskCompleted(
    const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::OnImageUploadTaskCompleted");
  base::AutoLock lock(lock_);
  // Upload task is complete, remove our reference to it.
  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  DCHECK(image_data->upload.task);
  image_data->upload.task = nullptr;

  // While the upload task is active, we keep a ref on both the image it will be
  // populating, as well as the decode it needs to populate it. Release these
  // refs now.
  UnrefImageDecode(draw_image);
  UnrefImageInternal(draw_image);
}

// Checks if an existing image decode exists. If not, returns a task to produce
// the requested decode.
scoped_refptr<TileTask> GpuImageDecodeCache::GetImageDecodeTaskAndRef(
    const DrawImage& draw_image,
    const TracingInfo& tracing_info,
    DecodeTaskType task_type) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDecodeTaskAndRef");
  lock_.AssertAcquired();

  // This ref is kept alive while an upload task may need this decode. We
  // release this ref in UploadTaskCompleted.
  if (task_type == DecodeTaskType::PART_OF_UPLOAD_TASK)
    RefImageDecode(draw_image);

  ImageData* image_data = GetImageDataForDrawImage(draw_image);
  DCHECK(image_data);
  if (image_data->decode.is_locked()) {
    // We should never be creating a decode task for an at raster image.
    DCHECK(!image_data->is_at_raster);
    // We should never be creating a decode for an already-uploaded image.
    DCHECK(!image_data->upload.image());
    return nullptr;
  }

  // We didn't have an existing locked image, create a task to lock or decode.
  scoped_refptr<TileTask>& existing_task =
      (task_type == DecodeTaskType::PART_OF_UPLOAD_TASK)
          ? image_data->decode.task
          : image_data->decode.stand_alone_task;
  if (!existing_task) {
    // Ref image decode and create a decode task. This ref will be released in
    // DecodeTaskCompleted.
    RefImageDecode(draw_image);
    existing_task = make_scoped_refptr(
        new ImageDecodeTaskImpl(this, draw_image, tracing_info, task_type));
  }
  return existing_task;
}

void GpuImageDecodeCache::RefImageDecode(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImageDecode");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(InUseCacheKey::FromDrawImage(draw_image));
  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageDecode(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::UnrefImageDecode");
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(InUseCacheKey::FromDrawImage(draw_image));
  DCHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->decode.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->decode.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

void GpuImageDecodeCache::RefImage(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::RefImage");
  lock_.AssertAcquired();
  InUseCacheKey key = InUseCacheKey::FromDrawImage(draw_image);
  auto found = in_use_cache_.find(key);

  // If no secondary cache entry was found for the given |draw_image|, then
  // the draw_image only exists in the |persistent_cache_|. Create an in-use
  // cache entry now.
  if (found == in_use_cache_.end()) {
    auto found_image = persistent_cache_.Peek(draw_image.image()->uniqueID());
    DCHECK(found_image != persistent_cache_.end());
    DCHECK(IsCompatible(found_image->second.get(), draw_image));
    found = in_use_cache_
                .insert(InUseCache::value_type(
                    key, InUseCacheEntry(found_image->second)))
                .first;
  }

  DCHECK(found != in_use_cache_.end());
  ++found->second.ref_count;
  ++found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
}

void GpuImageDecodeCache::UnrefImageInternal(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = in_use_cache_.find(InUseCacheKey::FromDrawImage(draw_image));
  DCHECK(found != in_use_cache_.end());
  DCHECK_GT(found->second.image_data->upload.ref_count, 0u);
  DCHECK_GT(found->second.ref_count, 0u);
  --found->second.ref_count;
  --found->second.image_data->upload.ref_count;
  OwnershipChanged(draw_image, found->second.image_data.get());
  if (found->second.ref_count == 0u) {
    in_use_cache_.erase(found);
  }
}

// Called any time an image or decode ref count changes. Takes care of any
// necessary memory budget book-keeping and cleanup.
void GpuImageDecodeCache::OwnershipChanged(const DrawImage& draw_image,
                                           ImageData* image_data) {
  lock_.AssertAcquired();

  bool has_any_refs =
      image_data->upload.ref_count > 0 || image_data->decode.ref_count > 0;

  // Don't keep around completely empty images. This can happen if an image's
  // decode/upload tasks were both cancelled before completing.
  if (!has_any_refs && !image_data->upload.image() &&
      !image_data->decode.data()) {
    auto found_persistent =
        persistent_cache_.Peek(draw_image.image()->uniqueID());
    if (found_persistent != persistent_cache_.end())
      persistent_cache_.Erase(found_persistent);
  }

  // Don't keep around orphaned images.
  if (image_data->is_orphaned && !has_any_refs) {
    images_pending_deletion_.push_back(std::move(image_data->upload.image()));
    image_data->upload.SetImage(nullptr);
  }

  // Don't keep CPU images if they are unused, these images can be recreated by
  // re-locking discardable (rather than requiring a full upload like GPU
  // images).
  if (image_data->mode == DecodedDataMode::CPU && !has_any_refs) {
    images_pending_deletion_.push_back(image_data->upload.image());
    image_data->upload.SetImage(nullptr);
  }

  if (image_data->is_at_raster && !has_any_refs) {
    // We have an at-raster image which has reached zero refs. If it won't fit
    // in our cache, delete the image to allow it to fit.
    if (image_data->upload.image() && !CanFitInCache(image_data->size)) {
      images_pending_deletion_.push_back(image_data->upload.image());
      image_data->upload.SetImage(nullptr);
    }

    // We now have an at-raster image which will fit in our cache. Convert it
    // to not-at-raster.
    image_data->is_at_raster = false;
    if (image_data->upload.image()) {
      bytes_used_ += image_data->size;
      image_data->upload.budgeted = true;
    }
  }

  // If we have image refs on a non-at-raster image, it must be budgeted, as it
  // is either uploaded or pending upload.
  if (image_data->upload.ref_count > 0 && !image_data->upload.budgeted &&
      !image_data->is_at_raster) {
    // We should only be taking non-at-raster refs on images that fit in cache.
    DCHECK(CanFitInWorkingSet(image_data->size));

    bytes_used_ += image_data->size;
    image_data->upload.budgeted = true;
  }

  // If we have no image refs on an image, it should only be budgeted if it has
  // an uploaded image. If no image exists (upload was cancelled), we should
  // un-budget the image.
  if (image_data->upload.ref_count == 0 && image_data->upload.budgeted &&
      !image_data->upload.image()) {
    DCHECK_GE(bytes_used_, image_data->size);
    bytes_used_ -= image_data->size;
    image_data->upload.budgeted = false;
  }

  // We should unlock the discardable memory for the image in two cases:
  // 1) The image is no longer being used (no decode or upload refs).
  // 2) This is a GPU backed image that has already been uploaded (no decode
  //    refs, and we actually already have an image).
  bool should_unlock_discardable =
      !has_any_refs ||
      (image_data->mode == DecodedDataMode::GPU &&
       !image_data->decode.ref_count && image_data->upload.image());

  if (should_unlock_discardable && image_data->decode.is_locked()) {
    DCHECK(image_data->decode.data());
    image_data->decode.Unlock();
  }

  // EnsureCapacity to make sure we are under our cache limits.
  EnsureCapacity(0);

#if DCHECK_IS_ON()
  // Sanity check the above logic.
  if (image_data->upload.image()) {
    DCHECK(image_data->is_at_raster || image_data->upload.budgeted);
    if (image_data->mode == DecodedDataMode::CPU)
      DCHECK(image_data->decode.is_locked());
  } else {
    DCHECK(!image_data->upload.budgeted || image_data->upload.ref_count > 0);
  }
#endif
}

// Ensures that we can fit a new image of size |required_size| in our working
// set. In doing so, this function will free unreferenced image data as
// necessary to create rooom.
bool GpuImageDecodeCache::EnsureCapacity(size_t required_size) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::EnsureCapacity");
  lock_.AssertAcquired();

  // While we only care whether |required_size| fits in our working set, we
  // also want to keep our cache under-budget if possible. Working set size
  // will always match or exceed cache size, so keeping the cache under budget
  // may be impossible.
  if (CanFitInCache(required_size) && !ExceedsPreferredCount())
    return true;

  // While we are over memory or preferred item capacity, we iterate through
  // our set of cached image data in LRU order. For each image, we can do two
  // things: 1) We can free the uploaded image, reducing the memory usage of
  // the cache and 2) we can remove the entry entirely, reducing the count of
  // elements in the cache.
  for (auto it = persistent_cache_.rbegin(); it != persistent_cache_.rend();) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      ++it;
      continue;
    }

    // Current entry has no refs. Ensure it is not locked.
    DCHECK(!it->second->decode.is_locked());

    // If an image without refs is budgeted, it must have an associated image
    // upload.
    DCHECK(!it->second->upload.budgeted || it->second->upload.image());

    // Free the uploaded image if possible.
    if (it->second->upload.image()) {
      DCHECK(it->second->upload.budgeted);
      DCHECK_GE(bytes_used_, it->second->size);
      bytes_used_ -= it->second->size;
      images_pending_deletion_.push_back(it->second->upload.image());
      it->second->upload.SetImage(nullptr);
      it->second->upload.budgeted = false;
    }

    // Free the entire entry if necessary.
    if (ExceedsPreferredCount()) {
      it = persistent_cache_.Erase(it);
    } else {
      ++it;
    }

    if (CanFitInCache(required_size) && !ExceedsPreferredCount())
      return true;
  }

  return CanFitInWorkingSet(required_size);
}

bool GpuImageDecodeCache::CanFitInCache(size_t size) const {
  lock_.AssertAcquired();

  size_t bytes_limit;
  if (memory_state_ == base::MemoryState::NORMAL) {
    bytes_limit = cached_bytes_limit_;
  } else if (memory_state_ == base::MemoryState::THROTTLED) {
    bytes_limit = cached_bytes_limit_ / kThrottledCacheSizeReductionFactor;
  } else {
    DCHECK_EQ(base::MemoryState::SUSPENDED, memory_state_);
    bytes_limit = kSuspendedOrInvisibleMaxGpuImageBytes;
  }

  base::CheckedNumeric<uint32_t> new_size(bytes_used_);
  new_size += size;
  return new_size.IsValid() && new_size.ValueOrDie() <= bytes_limit;
}

bool GpuImageDecodeCache::CanFitInWorkingSet(size_t size) const {
  lock_.AssertAcquired();

  base::CheckedNumeric<uint32_t> new_size(bytes_used_);
  new_size += size;
  return new_size.IsValid() && new_size.ValueOrDie() <= max_working_set_bytes_;
}

bool GpuImageDecodeCache::ExceedsPreferredCount() const {
  lock_.AssertAcquired();

  size_t items_limit;
  if (memory_state_ == base::MemoryState::NORMAL) {
    items_limit = kNormalMaxItemsInCache;
  } else if (memory_state_ == base::MemoryState::THROTTLED) {
    items_limit = kThrottledMaxItemsInCache;
  } else {
    DCHECK_EQ(base::MemoryState::SUSPENDED, memory_state_);
    items_limit = kSuspendedMaxItemsInCache;
  }

  return persistent_cache_.size() > items_limit;
}

void GpuImageDecodeCache::DecodeImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data,
                                                 TaskType task_type) {
  lock_.AssertAcquired();

  DCHECK_GT(image_data->decode.ref_count, 0u);

  if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image. Don't try again.
    return;
  }

  if (image_data->upload.image()) {
    // We already have an uploaded image, no reason to decode.
    return;
  }

  if (image_data->decode.data() &&
      (image_data->decode.is_locked() || image_data->decode.Lock())) {
    // We already decoded this, or we just needed to lock, early out.
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeCache::DecodeImage");
  RecordImageMipLevelUMA(image_data->upload_params.fPreScaleMipLevel);

  image_data->decode.ResetData();
  std::unique_ptr<base::DiscardableMemory> backing_memory;
  {
    base::AutoUnlock unlock(lock_);

    backing_memory = base::DiscardableMemoryAllocator::GetInstance()
                         ->AllocateLockedDiscardableMemory(image_data->size);

    switch (image_data->mode) {
      case DecodedDataMode::CPU: {
        SkImageInfo image_info = CreateImageInfoForDrawImage(
            draw_image, image_data->upload_params.fPreScaleMipLevel);
        // In order to match GPU scaling quality (which uses mip-maps at high
        // quality), we want to use at most medium filter quality for the
        // scale.
        SkPixmap image_pixmap(image_info.makeColorSpace(nullptr),
                              backing_memory->data(), image_info.minRowBytes());
        // Note that scalePixels falls back to readPixels if the scale is 1x, so
        // no need to special case that as an optimization.
        if (!DrawAndScaleImage(draw_image, &image_pixmap)) {
          DLOG(ERROR) << "scalePixels failed.";
          backing_memory->Unlock();
          backing_memory.reset();
        }
        break;
      }
      case DecodedDataMode::GPU: {
        // TODO(crbug.com/649167): Params should not have changed since initial
        // sizing. Somehow this still happens. We should investigate and re-add
        // DCHECKs here to enforce this.
        if (!draw_image.image()->getDeferredTextureImageData(
                *context_threadsafe_proxy_.get(), &image_data->upload_params, 1,
                backing_memory->data(), nullptr,
                viz::ResourceFormatToClosestSkColorType(format_))) {
          DLOG(ERROR) << "getDeferredTextureImageData failed despite params "
                      << "having validated.";
          backing_memory->Unlock();
          backing_memory.reset();
        }
        break;
      }
    }
  }

  if (image_data->decode.data()) {
    // An at-raster task decoded this before us. Ingore our decode.
    return;
  }

  if (!backing_memory) {
    // If |backing_memory| was not populated, we had a non-decodable image.
    image_data->decode.decode_failure = true;
    return;
  }

  image_data->decode.SetLockedData(std::move(backing_memory),
                                   task_type == TaskType::kOutOfRaster);
}

void GpuImageDecodeCache::UploadImageIfNecessary(const DrawImage& draw_image,
                                                 ImageData* image_data) {
  context_->GetLock()->AssertAcquired();
  lock_.AssertAcquired();

  if (image_data->decode.decode_failure) {
    // We were unnable to decode this image. Don't try to upload.
    return;
  }

  if (image_data->upload.image()) {
    // Someone has uploaded this image before us (at raster).
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeCache::UploadImage");
  DCHECK(image_data->decode.is_locked());
  DCHECK_GT(image_data->decode.ref_count, 0u);
  DCHECK_GT(image_data->upload.ref_count, 0u);

  // We are about to upload a new image and are holding the context lock.
  // Ensure that any images which have been marked for deletion are actually
  // cleaned up so we don't exceed our memory limit during this upload.
  DeletePendingImages();

  sk_sp<SkImage> uploaded_image;
  {
    base::AutoUnlock unlock(lock_);
    switch (image_data->mode) {
      case DecodedDataMode::CPU: {
        SkImageInfo image_info = CreateImageInfoForDrawImage(
            draw_image, image_data->upload_params.fPreScaleMipLevel);
        SkPixmap pixmap(image_info, image_data->decode.data()->data(),
                        image_info.minRowBytes());
        uploaded_image =
            SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
        break;
      }
      case DecodedDataMode::GPU: {
        uploaded_image = SkImage::MakeFromDeferredTextureImageData(
            context_->GrContext(), image_data->decode.data()->data(),
            SkBudgeted::kNo);
        break;
      }
    }
  }
  image_data->decode.mark_used();

  // TODO(crbug.com/740737): uploaded_image is sometimes null for reasons that
  // need investigation.

  if (uploaded_image && draw_image.target_color_space().IsValid()) {
    TRACE_EVENT0("cc", "GpuImageDecodeCache::UploadImage - color conversion");
    uploaded_image = uploaded_image->makeColorSpace(
        draw_image.target_color_space().ToSkColorSpace(),
        SkTransferFunctionBehavior::kIgnore);
  }

  // At-raster may have decoded this while we were unlocked. If so, ignore our
  // result.
  if (!image_data->upload.image())
    image_data->upload.SetImage(std::move(uploaded_image));
}

scoped_refptr<GpuImageDecodeCache::ImageData>
GpuImageDecodeCache::CreateImageData(const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::CreateImageData");
  lock_.AssertAcquired();

  DecodedDataMode mode;
  int upload_scale_mip_level = CalculateUploadScaleMipLevel(draw_image);
  auto params = SkImage::DeferredTextureImageUsageParams(
      draw_image.matrix(), CalculateDesiredFilterQuality(draw_image),
      upload_scale_mip_level);
  size_t data_size = draw_image.image()->getDeferredTextureImageData(
      *context_threadsafe_proxy_.get(), &params, 1, nullptr, nullptr,
      viz::ResourceFormatToClosestSkColorType(format_));

  if (data_size == 0) {
    // Can't upload image, too large or other failure. Try to use SW fallback.
    SkImageInfo image_info =
        CreateImageInfoForDrawImage(draw_image, upload_scale_mip_level);
    data_size = image_info.getSafeSize(image_info.minRowBytes());
    mode = DecodedDataMode::CPU;
  } else {
    mode = DecodedDataMode::GPU;
  }

  return make_scoped_refptr(
      new ImageData(mode, data_size, draw_image.target_color_space(), params));
}

void GpuImageDecodeCache::DeletePendingImages() {
  context_->GetLock()->AssertAcquired();
  lock_.AssertAcquired();
  images_pending_deletion_.clear();
}

SkImageInfo GpuImageDecodeCache::CreateImageInfoForDrawImage(
    const DrawImage& draw_image,
    int upload_scale_mip_level) const {
  gfx::Size mip_size =
      CalculateSizeForMipLevel(draw_image, upload_scale_mip_level);
  return SkImageInfo::Make(mip_size.width(), mip_size.height(),
                           viz::ResourceFormatToClosestSkColorType(format_),
                           kPremul_SkAlphaType,
                           draw_image.target_color_space().ToSkColorSpace());
}

// Tries to find an ImageData that can be used to draw the provided
// |draw_image|. First looks for an exact entry in our |in_use_cache_|. If one
// cannot be found, it looks for a compatible entry in our |persistent_cache_|.
GpuImageDecodeCache::ImageData* GpuImageDecodeCache::GetImageDataForDrawImage(
    const DrawImage& draw_image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "GpuImageDecodeCache::GetImageDataForDrawImage");
  lock_.AssertAcquired();
  auto found_in_use =
      in_use_cache_.find(InUseCacheKey::FromDrawImage(draw_image));
  if (found_in_use != in_use_cache_.end())
    return found_in_use->second.image_data.get();

  auto found_persistent = persistent_cache_.Get(draw_image.image()->uniqueID());
  if (found_persistent != persistent_cache_.end()) {
    ImageData* image_data = found_persistent->second.get();
    if (IsCompatible(image_data, draw_image)) {
      return image_data;
    } else {
      found_persistent->second->is_orphaned = true;
      // Call OwnershipChanged before erasing the orphaned task from the
      // persistent cache. This ensures that if the orphaned task has 0
      // references, it is cleaned up safely before it is deleted.
      OwnershipChanged(draw_image, image_data);
      persistent_cache_.Erase(found_persistent);
    }
  }

  return nullptr;
}

// Determines if we can draw the provided |draw_image| using the provided
// |image_data|. This is true if the |image_data| is not scaled, or if it
// is scaled at an equal or larger scale and equal or larger quality to
// the provided |draw_image|.
bool GpuImageDecodeCache::IsCompatible(const ImageData* image_data,
                                       const DrawImage& draw_image) const {
  bool is_scaled = image_data->upload_params.fPreScaleMipLevel != 0;
  bool scale_is_compatible = CalculateUploadScaleMipLevel(draw_image) >=
                             image_data->upload_params.fPreScaleMipLevel;
  bool quality_is_compatible = CalculateDesiredFilterQuality(draw_image) <=
                               image_data->upload_params.fQuality;
  bool color_is_compatible =
      image_data->target_color_space == draw_image.target_color_space();
  if (!color_is_compatible)
    return false;
  if (is_scaled && (!scale_is_compatible || !quality_is_compatible))
    return false;
  return true;
}

size_t GpuImageDecodeCache::GetDrawImageSizeForTesting(const DrawImage& image) {
  base::AutoLock lock(lock_);
  scoped_refptr<ImageData> data = CreateImageData(image);
  return data->size;
}

void GpuImageDecodeCache::SetImageDecodingFailedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.image()->uniqueID());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  image_data->decode.decode_failure = true;
}

bool GpuImageDecodeCache::DiscardableIsLockedForTesting(
    const DrawImage& image) {
  base::AutoLock lock(lock_);
  auto found = persistent_cache_.Peek(image.image()->uniqueID());
  DCHECK(found != persistent_cache_.end());
  ImageData* image_data = found->second.get();
  return image_data->decode.is_locked();
}

bool GpuImageDecodeCache::IsInInUseCacheForTesting(
    const DrawImage& image) const {
  auto found = in_use_cache_.find(InUseCacheKey::FromDrawImage(image));
  return found != in_use_cache_.end();
}

void GpuImageDecodeCache::OnMemoryStateChange(base::MemoryState state) {
  memory_state_ = state;
}

void GpuImageDecodeCache::OnPurgeMemory() {
  base::AutoLock lock(lock_);
  // Temporary changes |memory_state_| to free up cache as much as possible.
  base::AutoReset<base::MemoryState> reset(&memory_state_,
                                           base::MemoryState::SUSPENDED);
  EnsureCapacity(0);
}

}  // namespace cc
