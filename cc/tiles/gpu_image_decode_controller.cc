// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/gpu_image_decode_controller.h"

#include "base/memory/discardable_memory_allocator.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_math.h"
#include "base/strings/stringprintf.h"
#include "base/thread_task_runner_handle.h"
#include "cc/debug/devtools_instrumentation.h"
#include "cc/output/context_provider.h"
#include "cc/raster/tile_task.h"
#include "gpu/command_buffer/client/context_support.h"
#include "gpu/command_buffer/client/gles2_interface.h"
#include "gpu_image_decode_controller.h"
#include "skia/ext/texture_handle.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSurface.h"
#include "third_party/skia/include/gpu/GrContext.h"
#include "third_party/skia/include/gpu/GrTexture.h"
#include "ui/gfx/skia_util.h"
#include "ui/gl/trace_util.h"

namespace cc {
namespace {

static const int kMaxGpuImageBytes = 1024 * 1024 * 96;
static const int kMaxDiscardableItems = 2000;

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

SkImage::DeferredTextureImageUsageParams ParamsFromDrawImage(
    const DrawImage& draw_image) {
  SkImage::DeferredTextureImageUsageParams params;
  params.fMatrix = draw_image.matrix();
  params.fQuality = draw_image.filter_quality();

  return params;
}

}  // namespace

// Task which decodes an image and stores the result in discardable memory.
// This task does not use GPU resources and can be run on any thread.
class ImageDecodeTaskImpl : public TileTask {
 public:
  ImageDecodeTaskImpl(GpuImageDecodeController* controller,
                      const DrawImage& draw_image,
                      uint64_t source_prepare_tiles_id)
      : TileTask(true),
        controller_(controller),
        image_(draw_image),
        source_prepare_tiles_id_(source_prepare_tiles_id) {
    DCHECK(!SkipImage(draw_image));
  }

  // Overridden from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageDecodeTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", source_prepare_tiles_id_);
    controller_->DecodeImage(image_);
  }

  // Overridden from TileTask:
  void ScheduleOnOriginThread(RasterBufferProvider* provider) override {}
  void CompleteOnOriginThread(RasterBufferProvider* provider) override {
    controller_->DecodeTaskCompleted(image_);
  }

 protected:
  ~ImageDecodeTaskImpl() override {}

 private:
  GpuImageDecodeController* controller_;
  DrawImage image_;
  const uint64_t source_prepare_tiles_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageDecodeTaskImpl);
};

// Task which creates an image from decoded data. Typically this involves
// uploading data to the GPU, which requires this task be run on the non-
// concurrent thread.
class ImageUploadTaskImpl : public TileTask {
 public:
  ImageUploadTaskImpl(GpuImageDecodeController* controller,
                      const DrawImage& draw_image,
                      scoped_refptr<TileTask> decode_dependency,
                      uint64_t source_prepare_tiles_id)
      : TileTask(false),
        controller_(controller),
        image_(draw_image),
        source_prepare_tiles_id_(source_prepare_tiles_id) {
    DCHECK(!SkipImage(draw_image));
    // If an image is already decoded and locked, we will not generate a
    // decode task.
    if (decode_dependency)
      dependencies_.push_back(std::move(decode_dependency));
  }

  // Override from Task:
  void RunOnWorkerThread() override {
    TRACE_EVENT2("cc", "ImageUploadTaskImpl::RunOnWorkerThread", "mode", "gpu",
                 "source_prepare_tiles_id", source_prepare_tiles_id_);
    controller_->UploadImage(image_);
  }

  void ScheduleOnOriginThread(RasterBufferProvider* provider) override {}
  void CompleteOnOriginThread(RasterBufferProvider* provider) override {
    controller_->UploadTaskCompleted(image_);
  }

 protected:
  ~ImageUploadTaskImpl() override {}

 private:
  GpuImageDecodeController* controller_;
  DrawImage image_;
  uint64_t source_prepare_tiles_id_;

  DISALLOW_COPY_AND_ASSIGN(ImageUploadTaskImpl);
};

GpuImageDecodeController::DecodedImageData::DecodedImageData()
    : ref_count(0), is_locked(false), decode_failure(false) {}

GpuImageDecodeController::DecodedImageData::~DecodedImageData() = default;

GpuImageDecodeController::UploadedImageData::UploadedImageData()
    : budgeted(false), ref_count(0) {}

GpuImageDecodeController::UploadedImageData::~UploadedImageData() = default;

GpuImageDecodeController::ImageData::ImageData(DecodedDataMode mode,
                                               size_t size)
    : mode(mode), size(size), is_at_raster(false) {}

GpuImageDecodeController::ImageData::~ImageData() = default;

GpuImageDecodeController::GpuImageDecodeController(ContextProvider* context,
                                                   ResourceFormat decode_format)
    : format_(decode_format),
      context_(context),
      image_data_(ImageDataMRUCache::NO_AUTO_EVICT),
      cached_items_limit_(kMaxDiscardableItems),
      cached_bytes_limit_(kMaxGpuImageBytes),
      bytes_used_(0) {
  // Acquire the context_lock so that we can safely retrieve the
  // GrContextThreadSafeProxy. This proxy can then be used with no lock held.
  {
    ContextProvider::ScopedContextLock context_lock(context_);
    context_threadsafe_proxy_ = sk_sp<GrContextThreadSafeProxy>(
        context->GrContext()->threadSafeProxy());
  }

  // In certain cases, ThreadTaskRunnerHandle isn't set (Android Webview).
  // Don't register a dump provider in these cases.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    base::trace_event::MemoryDumpManager::GetInstance()->RegisterDumpProvider(
        this, "cc::GpuImageDecodeController",
        base::ThreadTaskRunnerHandle::Get());
  }
}

GpuImageDecodeController::~GpuImageDecodeController() {
  // SetShouldAggressivelyFreeResources will zero our limits and free all
  // outstanding image memory.
  SetShouldAggressivelyFreeResources(true);

  // It is safe to unregister, even if we didn't register in the constructor.
  base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
      this);
}

bool GpuImageDecodeController::GetTaskForImageAndRef(
    const DrawImage& draw_image,
    uint64_t prepare_tiles_id,
    scoped_refptr<TileTask>* task) {
  if (SkipImage(draw_image)) {
    *task = nullptr;
    return false;
  }

  base::AutoLock lock(lock_);
  const auto image_id = draw_image.image()->uniqueID();

  auto found = image_data_.Get(image_id);
  if (found != image_data_.end()) {
    ImageData* image_data = found->second.get();
    if (image_data->is_at_raster) {
      // Image is at-raster, just return, this usage will be at-raster as well.
      *task = nullptr;
      return false;
    }

    if (image_data->decode.decode_failure) {
      // We have already tried and failed to decode this image, so just return.
      *task = nullptr;
      return false;
    }

    if (image_data->upload.image) {
      // The image is already uploaded, ref and return.
      RefImage(draw_image);
      *task = nullptr;
      return true;
    }
  }

  // We didn't have a pre-uploaded image, so we need an upload task. Try to find
  // an existing one.
  scoped_refptr<TileTask>& existing_task =
      pending_image_upload_tasks_[image_id];
  if (existing_task) {
    // We had an existing upload task, ref the image and return the task.
    RefImage(draw_image);
    *task = existing_task;
    return true;
  }

  // We will be creating a new upload task. If necessary, create a placeholder
  // ImageData to hold the result.
  std::unique_ptr<ImageData> new_data;
  ImageData* data;
  if (found == image_data_.end()) {
    new_data = CreateImageData(draw_image);
    data = new_data.get();
  } else {
    data = found->second.get();
  }

  // Ensure that the image we're about to decode/upload will fit in memory.
  if (!EnsureCapacity(data->size)) {
    // Image will not fit, do an at-raster decode.
    *task = nullptr;
    return false;
  }

  // If we had to create new image data, add it to our map now that we know it
  // will fit.
  if (new_data)
    found = image_data_.Put(image_id, std::move(new_data));

  // Ref image and create a upload and decode tasks. We will release this ref
  // in UploadTaskCompleted.
  RefImage(draw_image);
  existing_task = make_scoped_refptr(new ImageUploadTaskImpl(
      this, draw_image, GetImageDecodeTaskAndRef(draw_image, prepare_tiles_id),
      prepare_tiles_id));

  // Ref the image again - this ref is owned by the caller, and it is their
  // responsibility to release it by calling UnrefImage.
  RefImage(draw_image);
  *task = existing_task;
  return true;
}

void GpuImageDecodeController::UnrefImage(const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  UnrefImageInternal(draw_image);
}

DecodedDrawImage GpuImageDecodeController::GetDecodedImageForDraw(
    const DrawImage& draw_image) {
  // We are being called during raster. The context lock must already be
  // acquired by the caller.
  context_->GetLock()->AssertAcquired();

  if (SkipImage(draw_image))
    return DecodedDrawImage(nullptr, draw_image.filter_quality());

  TRACE_EVENT0("cc", "GpuImageDecodeController::GetDecodedImageForDraw");

  base::AutoLock lock(lock_);
  const uint32_t unique_id = draw_image.image()->uniqueID();
  auto found = image_data_.Peek(unique_id);
  if (found == image_data_.end()) {
    // We didn't find the image, create a new entry.
    auto data = CreateImageData(draw_image);
    found = image_data_.Put(unique_id, std::move(data));
  }

  ImageData* image_data = found->second.get();

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
  DecodeImageIfNecessary(draw_image, image_data);
  UploadImageIfNecessary(draw_image, image_data);
  // Unref the image decode, but not the image. The image ref will be released
  // in DrawWithImageFinished.
  UnrefImageDecode(draw_image);

  sk_sp<SkImage> image = image_data->upload.image;
  DCHECK(image || image_data->decode.decode_failure);

  DecodedDrawImage decoded_draw_image(std::move(image),
                                      draw_image.filter_quality());
  decoded_draw_image.set_at_raster_decode(image_data->is_at_raster);
  return decoded_draw_image;
}

void GpuImageDecodeController::DrawWithImageFinished(
    const DrawImage& draw_image,
    const DecodedDrawImage& decoded_draw_image) {
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

void GpuImageDecodeController::ReduceCacheUsage() {
  base::AutoLock lock(lock_);
  EnsureCapacity(0);
}

void GpuImageDecodeController::SetShouldAggressivelyFreeResources(
    bool aggressively_free_resources) {
  if (aggressively_free_resources) {
    ContextProvider::ScopedContextLock context_lock(context_);
    base::AutoLock lock(lock_);
    // We want to keep as little in our cache as possible. Set our memory limit
    // to zero and EnsureCapacity to clean up memory.
    cached_bytes_limit_ = 0;
    EnsureCapacity(0);

    // We are holding the context lock, so finish cleaning up deleted images
    // now.
    DeletePendingImages();
  } else {
    base::AutoLock lock(lock_);
    cached_bytes_limit_ = kMaxGpuImageBytes;
  }
}

bool GpuImageDecodeController::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  for (const auto& image_pair : image_data_) {
    const ImageData* image_data = image_pair.second.get();
    const uint32_t image_id = image_pair.first;

    // If we have discardable decoded data, dump this here.
    if (image_data->decode.data) {
      std::string discardable_dump_name = base::StringPrintf(
          "cc/image_memory/controller_%p/discardable/image_%d", this, image_id);
      base::trace_event::MemoryAllocatorDump* dump =
          image_data->decode.data->CreateMemoryAllocatorDump(
              discardable_dump_name.c_str(), pmd);

      // If our image is locked, dump the "locked_size" as an additional column.
      // This lets us see the amount of discardable which is contributing to
      // memory pressure.
      if (image_data->decode.is_locked) {
        dump->AddScalar("locked_size",
                        base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                        image_data->size);
      }
    }

    // If we have an uploaded image (that is actually on the GPU, not just a CPU
    // wrapper), upload it here.
    if (image_data->upload.image && image_data->mode == DecodedDataMode::GPU) {
      std::string gpu_dump_name = base::StringPrintf(
          "cc/image_memory/controller_%p/gpu/image_%d", this, image_id);
      base::trace_event::MemoryAllocatorDump* dump =
          pmd->CreateAllocatorDump(gpu_dump_name);
      dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                      base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                      image_data->size);

      // Create a global shred GUID to associate this data with its GPU process
      // counterpart.
      GLuint gl_id = skia::GrBackendObjectToGrGLTextureInfo(
                         image_data->upload.image->getTextureHandle(
                             false /* flushPendingGrContextIO */))
                         ->fID;
      base::trace_event::MemoryAllocatorDumpGuid guid =
          gfx::GetGLTextureClientGUIDForTracing(
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

void GpuImageDecodeController::DecodeImage(const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  auto found = image_data_.Peek(draw_image.image()->uniqueID());
  DCHECK(found != image_data_.end());
  DCHECK(!found->second->is_at_raster);
  DecodeImageIfNecessary(draw_image, found->second.get());
}

void GpuImageDecodeController::UploadImage(const DrawImage& draw_image) {
  ContextProvider::ScopedContextLock context_lock(context_);
  base::AutoLock lock(lock_);
  auto found = image_data_.Peek(draw_image.image()->uniqueID());
  DCHECK(found != image_data_.end());
  DCHECK(!found->second->is_at_raster);
  UploadImageIfNecessary(draw_image, found->second.get());
}

void GpuImageDecodeController::DecodeTaskCompleted(
    const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  // Decode task is complete, remove it from our list of pending tasks.
  pending_image_decode_tasks_.erase(draw_image.image()->uniqueID());

  // While the decode task is active, we keep a ref on the decoded data.
  // Release that ref now.
  UnrefImageDecode(draw_image);
}

void GpuImageDecodeController::UploadTaskCompleted(
    const DrawImage& draw_image) {
  base::AutoLock lock(lock_);
  // Upload task is complete, remove it from our list of pending tasks.
  pending_image_upload_tasks_.erase(draw_image.image()->uniqueID());

  // While the upload task is active, we keep a ref on both the image it will be
  // populating, as well as the decode it needs to populate it. Release these
  // refs now.
  UnrefImageDecode(draw_image);
  UnrefImageInternal(draw_image);
}

// Checks if an existing image decode exists. If not, returns a task to produce
// the requested decode.
scoped_refptr<TileTask> GpuImageDecodeController::GetImageDecodeTaskAndRef(
    const DrawImage& draw_image,
    uint64_t prepare_tiles_id) {
  lock_.AssertAcquired();

  const uint32_t image_id = draw_image.image()->uniqueID();

  // This ref is kept alive while an upload task may need this decode. We
  // release this ref in UploadTaskCompleted.
  RefImageDecode(draw_image);

  auto found = image_data_.Peek(image_id);
  if (found != image_data_.end() && found->second->decode.is_locked) {
    // We should never be creating a decode task for an at raster image.
    DCHECK(!found->second->is_at_raster);
    // We should never be creating a decode for an already-uploaded image.
    DCHECK(!found->second->upload.image);
    return nullptr;
  }

  // We didn't have an existing locked image, create a task to lock or decode.
  scoped_refptr<TileTask>& existing_task =
      pending_image_decode_tasks_[image_id];
  if (!existing_task) {
    // Ref image decode and create a decode task. This ref will be released in
    // DecodeTaskCompleted.
    RefImageDecode(draw_image);
    existing_task = make_scoped_refptr(
        new ImageDecodeTaskImpl(this, draw_image, prepare_tiles_id));
  }
  return existing_task;
}

void GpuImageDecodeController::RefImageDecode(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = image_data_.Peek(draw_image.image()->uniqueID());
  DCHECK(found != image_data_.end());
  ++found->second->decode.ref_count;
  RefCountChanged(found->second.get());
}

void GpuImageDecodeController::UnrefImageDecode(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = image_data_.Peek(draw_image.image()->uniqueID());
  DCHECK(found != image_data_.end());
  DCHECK_GT(found->second->decode.ref_count, 0u);
  --found->second->decode.ref_count;
  RefCountChanged(found->second.get());
}

void GpuImageDecodeController::RefImage(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = image_data_.Peek(draw_image.image()->uniqueID());
  DCHECK(found != image_data_.end());
  ++found->second->upload.ref_count;
  RefCountChanged(found->second.get());
}

void GpuImageDecodeController::UnrefImageInternal(const DrawImage& draw_image) {
  lock_.AssertAcquired();
  auto found = image_data_.Peek(draw_image.image()->uniqueID());
  DCHECK(found != image_data_.end());
  DCHECK_GT(found->second->upload.ref_count, 0u);
  --found->second->upload.ref_count;
  RefCountChanged(found->second.get());
}

// Called any time an image or decode ref count changes. Takes care of any
// necessary memory budget book-keeping and cleanup.
void GpuImageDecodeController::RefCountChanged(ImageData* image_data) {
  lock_.AssertAcquired();

  bool has_any_refs =
      image_data->upload.ref_count > 0 || image_data->decode.ref_count > 0;
  if (image_data->is_at_raster && !has_any_refs) {
    // We have an at-raster image which has reached zero refs. If it won't fit
    // in our cache, delete the image to allow it to fit.
    if (image_data->upload.image && !CanFitSize(image_data->size)) {
      images_pending_deletion_.push_back(std::move(image_data->upload.image));
      image_data->upload.image = nullptr;
    }

    // We now have an at-raster image which will fit in our cache. Convert it
    // to not-at-raster.
    image_data->is_at_raster = false;
    if (image_data->upload.image) {
      bytes_used_ += image_data->size;
      image_data->upload.budgeted = true;
    }
  }

  // If we have image refs on a non-at-raster image, it must be budgeted, as it
  // is either uploaded or pending upload.
  if (image_data->upload.ref_count > 0 && !image_data->upload.budgeted &&
      !image_data->is_at_raster) {
    // We should only be taking non-at-raster refs on images that fit in cache.
    DCHECK(CanFitSize(image_data->size));

    bytes_used_ += image_data->size;
    image_data->upload.budgeted = true;
  }

  // If we have no image refs on an image, it should only be budgeted if it has
  // an uploaded image. If no image exists (upload was cancelled), we should
  // un-budget the image.
  if (image_data->upload.ref_count == 0 && image_data->upload.budgeted &&
      !image_data->upload.image) {
    DCHECK_GE(bytes_used_, image_data->size);
    bytes_used_ -= image_data->size;
    image_data->upload.budgeted = false;
  }

  // If we have no decode refs on an image, we should unlock any locked
  // discardable memory.
  if (image_data->decode.ref_count == 0 && image_data->decode.is_locked) {
    DCHECK(image_data->decode.data);
    image_data->decode.data->Unlock();
    image_data->decode.is_locked = false;
  }
}

// Ensures that we can fit a new image of size |required_size| in our cache. In
// doing so, this function will free unreferenced image data as necessary to
// create rooom.
bool GpuImageDecodeController::EnsureCapacity(size_t required_size) {
  lock_.AssertAcquired();

  if (CanFitSize(required_size) && !ExceedsPreferredCount())
    return true;

  // While we are over memory or preferred item capacity, we iterate through
  // our set of cached image data in LRU order. For each image, we can do two
  // things: 1) We can free the uploaded image, reducing the memory usage of
  // the cache and 2) we can remove the entry entirely, reducing the count of
  // elements in the cache.
  for (auto it = image_data_.rbegin(); it != image_data_.rend();) {
    if (it->second->decode.ref_count != 0 ||
        it->second->upload.ref_count != 0) {
      ++it;
      continue;
    }

    // Current entry has no refs. Ensure it is not locked.
    DCHECK(!it->second->decode.is_locked);

    // If an image without refs is budgeted, it must have an associated image
    // upload.
    DCHECK(!it->second->upload.budgeted || it->second->upload.image);

    // Free the uploaded image if possible.
    if (it->second->upload.image) {
      DCHECK(it->second->upload.budgeted);
      DCHECK_GE(bytes_used_, it->second->size);
      bytes_used_ -= it->second->size;
      images_pending_deletion_.push_back(std::move(it->second->upload.image));
      it->second->upload.image = nullptr;
      it->second->upload.budgeted = false;
    }

    // Free the entire entry if necessary.
    if (ExceedsPreferredCount()) {
      it = image_data_.Erase(it);
    } else {
      ++it;
    }

    if (CanFitSize(required_size) && !ExceedsPreferredCount())
      return true;
  }

  // Preferred count is only used as a guideline when triming the cache. Allow
  // new elements to be added as long as we are below our size limit.
  return CanFitSize(required_size);
}

bool GpuImageDecodeController::CanFitSize(size_t size) const {
  lock_.AssertAcquired();

  base::CheckedNumeric<uint32_t> new_size(bytes_used_);
  new_size += size;
  return new_size.IsValid() && new_size.ValueOrDie() <= cached_bytes_limit_;
}

bool GpuImageDecodeController::ExceedsPreferredCount() const {
  lock_.AssertAcquired();

  return image_data_.size() > cached_items_limit_;
}

void GpuImageDecodeController::DecodeImageIfNecessary(
    const DrawImage& draw_image,
    ImageData* image_data) {
  lock_.AssertAcquired();

  DCHECK_GT(image_data->decode.ref_count, 0u);

  if (image_data->decode.decode_failure) {
    // We have already tried and failed to decode this image. Don't try again.
    return;
  }

  if (image_data->upload.image) {
    // We already have an uploaded image, no reason to decode.
    return;
  }

  if (image_data->decode.data &&
      (image_data->decode.is_locked || image_data->decode.data->Lock())) {
    // We already decoded this, or we just needed to lock, early out.
    image_data->decode.is_locked = true;
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeController::DecodeImage");

  image_data->decode.data = nullptr;
  std::unique_ptr<base::DiscardableMemory> backing_memory;
  {
    base::AutoUnlock unlock(lock_);
    switch (image_data->mode) {
      case DecodedDataMode::CPU: {
        backing_memory =
            base::DiscardableMemoryAllocator::GetInstance()
                ->AllocateLockedDiscardableMemory(image_data->size);
        SkImageInfo image_info = CreateImageInfoForDrawImage(draw_image);
        if (!draw_image.image()->readPixels(image_info, backing_memory->data(),
                                            image_info.minRowBytes(), 0, 0,
                                            SkImage::kDisallow_CachingHint)) {
          backing_memory.reset();
        }
        break;
      }
      case DecodedDataMode::GPU: {
        backing_memory =
            base::DiscardableMemoryAllocator::GetInstance()
                ->AllocateLockedDiscardableMemory(image_data->size);
        auto params = ParamsFromDrawImage(draw_image);
        if (!draw_image.image()->getDeferredTextureImageData(
                *context_threadsafe_proxy_.get(), &params, 1,
                backing_memory->data())) {
          backing_memory.reset();
        }
        break;
      }
    }
  }

  if (image_data->decode.data) {
    // An at-raster task decoded this before us. Ingore our decode.
    return;
  }

  if (!backing_memory) {
    // If |backing_memory| was not populated, we had a non-decodable image.
    image_data->decode.decode_failure = true;
    return;
  }

  image_data->decode.data = std::move(backing_memory);
  DCHECK(!image_data->decode.is_locked);
  image_data->decode.is_locked = true;
}

void GpuImageDecodeController::UploadImageIfNecessary(
    const DrawImage& draw_image,
    ImageData* image_data) {
  context_->GetLock()->AssertAcquired();
  lock_.AssertAcquired();

  if (image_data->decode.decode_failure) {
    // We were unnable to decode this image. Don't try to upload.
    return;
  }

  if (image_data->upload.image) {
    // Someone has uploaded this image before us (at raster).
    return;
  }

  TRACE_EVENT0("cc", "GpuImageDecodeController::UploadImage");
  DCHECK(image_data->decode.is_locked);
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
        SkImageInfo image_info = CreateImageInfoForDrawImage(draw_image);
        SkPixmap pixmap(image_info, image_data->decode.data->data(),
                        image_info.minRowBytes());
        uploaded_image =
            SkImage::MakeFromRaster(pixmap, [](const void*, void*) {}, nullptr);
        break;
      }
      case DecodedDataMode::GPU: {
        uploaded_image = SkImage::MakeFromDeferredTextureImageData(
            context_->GrContext(), image_data->decode.data->data(),
            SkBudgeted::kNo);
        break;
      }
    }
  }
  DCHECK(uploaded_image);

  // At-raster may have decoded this while we were unlocked. If so, ignore our
  // result.
  if (!image_data->upload.image) {
    image_data->upload.image = std::move(uploaded_image);
  }
}

std::unique_ptr<GpuImageDecodeController::ImageData>
GpuImageDecodeController::CreateImageData(const DrawImage& draw_image) {
  lock_.AssertAcquired();

  DecodedDataMode mode;
  SkImageInfo info = CreateImageInfoForDrawImage(draw_image);
  SkImage::DeferredTextureImageUsageParams params =
      ParamsFromDrawImage(draw_image);
  size_t data_size = draw_image.image()->getDeferredTextureImageData(
      *context_threadsafe_proxy_.get(), &params, 1, nullptr);

  if (data_size == 0) {
    // Can't upload image, too large or other failure. Try to use SW fallback.
    data_size = info.getSafeSize(info.minRowBytes());
    mode = DecodedDataMode::CPU;
  } else {
    mode = DecodedDataMode::GPU;
  }

  return base::WrapUnique(new ImageData(mode, data_size));
}

void GpuImageDecodeController::DeletePendingImages() {
  context_->GetLock()->AssertAcquired();
  lock_.AssertAcquired();
  images_pending_deletion_.clear();
}

SkImageInfo GpuImageDecodeController::CreateImageInfoForDrawImage(
    const DrawImage& draw_image) const {
  return SkImageInfo::Make(
      draw_image.image()->width(), draw_image.image()->height(),
      ResourceFormatToClosestSkColorType(format_), kPremul_SkAlphaType);
}

}  // namespace cc
