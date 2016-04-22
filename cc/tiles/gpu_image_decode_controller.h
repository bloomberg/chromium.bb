// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_
#define CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_

#include <memory>
#include <unordered_map>
#include <vector>

#include "base/containers/mru_cache.h"
#include "base/memory/discardable_memory.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/memory_dump_provider.h"
#include "cc/base/cc_export.h"
#include "cc/resources/resource_format.h"
#include "cc/tiles/image_decode_controller.h"
#include "third_party/skia/include/core/SkRefCnt.h"

class SkImageTextureData;

namespace cc {

class ContextProvider;

// GpuImageDecodeController handles the decode and upload of images that will
// be used by Skia's GPU raster path. It also maintains a cache of these
// decoded/uploaded images for later re-use.
//
// Generally, when an image is required for raster, GpuImageDecodeController
// creates two tasks, one to decode the image, and one to upload the image to
// the GPU. These tasks are completed before the raster task which depends on
// the image. We need to seperate decode and upload tasks, as decode can occur
// simultaneously on multiple threads, while upload requires the GL context
// lock must happen on our non-concurrent raster thread.
//
// Decoded and Uploaded image data share a single cache entry. Depending on how
// far we've progressed, this cache entry may contain CPU-side decoded data,
// GPU-side uploaded data, or both. Because CPU-side decoded data is stored in
// discardable memory, and is only locked for short periods of time (until the
// upload completes), this memory is not counted against our sized cache
// limits. Uploaded GPU memory, being non-discardable, always counts against
// our limits.
//
// In cases where the number of images needed exceeds our cache limits, we
// operate in an "at-raster" mode. In this mode, there are no decode/upload
// tasks, and images are decoded/uploaded as needed, immediately before being
// used in raster. Cache entries for at-raster tasks are marked as such, which
// prevents future tasks from taking a dependency on them and extending their
// lifetime longer than is necessary.
class CC_EXPORT GpuImageDecodeController : public ImageDecodeController {
 public:
  explicit GpuImageDecodeController(ContextProvider* context,
                                    ResourceFormat decode_format);
  ~GpuImageDecodeController() override;

  // ImageDecodeController overrides.

  // Finds the existing uploaded image for the provided DrawImage. Creates an
  // upload task to upload the image if an exsiting image does not exist.
  bool GetTaskForImageAndRef(const DrawImage& image,
                             uint64_t prepare_tiles_id,
                             scoped_refptr<TileTask>* task) override;
  void UnrefImage(const DrawImage& image) override;
  DecodedDrawImage GetDecodedImageForDraw(const DrawImage& draw_image) override;
  void DrawWithImageFinished(const DrawImage& image,
                             const DecodedDrawImage& decoded_image) override;
  void ReduceCacheUsage() override;
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override;

  // Called by Decode / Upload tasks.
  void DecodeImage(const DrawImage& image);
  void UploadImage(const DrawImage& image);
  void DecodeTaskCompleted(const DrawImage& image);
  void UploadTaskCompleted(const DrawImage& image);

  // For testing only.
  void SetCachedItemLimitForTesting(size_t limit) {
    cached_items_limit_ = limit;
  }
  void SetCachedBytesLimitForTesting(size_t limit) {
    cached_bytes_limit_ = limit;
  }
  size_t GetBytesUsedForTesting() const { return bytes_used_; }

 private:
  enum class DecodedDataMode { GPU, CPU };

  // Stores the CPU-side decoded bits of an image and supporting fields.
  struct DecodedImageData {
    DecodedImageData();
    ~DecodedImageData();

    // May be null if image not yet decoded.
    std::unique_ptr<base::DiscardableMemory> data;
    uint32_t ref_count;
    bool is_locked;

    // Set to true if the image was corrupt and could not be decoded.
    bool decode_failure;
  };

  // Stores the GPU-side image and supporting fields.
  struct UploadedImageData {
    UploadedImageData();
    ~UploadedImageData();

    // May be null if image not yet uploaded / prepared.
    sk_sp<SkImage> image;
    // True if the image is counting against our memory limits.
    bool budgeted;
    uint32_t ref_count;
  };

  struct ImageData {
    ImageData(DecodedDataMode mode, size_t size);
    ~ImageData();

    const DecodedDataMode mode;
    const size_t size;
    bool is_at_raster;

    DecodedImageData decode;
    UploadedImageData upload;
  };

  using ImageDataMRUCache =
      base::MRUCache<uint32_t, std::unique_ptr<ImageData>>;

  // All private functions should only be called while holding |lock_|. Some
  // functions also require the |context_| lock. These are indicated by
  // additional comments.

  // Similar to GetTaskForImageAndRef, but gets the dependent decode task
  // rather than the upload task, if necessary.
  scoped_refptr<TileTask> GetImageDecodeTaskAndRef(const DrawImage& image,
                                                   uint64_t prepare_tiles_id);

  void RefImageDecode(const DrawImage& draw_image);
  void UnrefImageDecode(const DrawImage& draw_image);
  void RefImage(const DrawImage& draw_image);
  void UnrefImageInternal(const DrawImage& draw_image);
  void RefCountChanged(ImageData* image_data);

  // Ensures that the cache can hold an element of |required_size|, freeing
  // unreferenced cache entries if necessary to make room.
  bool EnsureCapacity(size_t required_size);
  bool CanFitSize(size_t size) const;
  bool ExceedsPreferredCount() const;

  void DecodeImageIfNecessary(const DrawImage& draw_image,
                              ImageData* image_data);

  std::unique_ptr<GpuImageDecodeController::ImageData> CreateImageData(
      const DrawImage& image);
  SkImageInfo CreateImageInfoForDrawImage(const DrawImage& draw_image) const;

  // The following two functions also require the |context_| lock to be held.
  void UploadImageIfNecessary(const DrawImage& draw_image,
                              ImageData* image_data);
  void DeletePendingImages();

  const ResourceFormat format_;
  ContextProvider* context_;
  sk_sp<GrContextThreadSafeProxy> context_threadsafe_proxy_;

  // All members below this point must only be accessed while holding |lock_|.
  base::Lock lock_;

  std::unordered_map<uint32_t, scoped_refptr<TileTask>>
      pending_image_upload_tasks_;
  std::unordered_map<uint32_t, scoped_refptr<TileTask>>
      pending_image_decode_tasks_;

  ImageDataMRUCache image_data_;

  size_t cached_items_limit_;
  size_t cached_bytes_limit_;
  size_t bytes_used_;

  // We can't release GPU backed SkImages without holding the context lock,
  // so we add them to this list and defer deletion until the next time the lock
  // is held.
  std::vector<sk_sp<SkImage>> images_pending_deletion_;
};

}  // namespace cc

#endif  // CC_TILES_GPU_IMAGE_DECODE_CONTROLLER_H_
