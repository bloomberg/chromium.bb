// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_CHECKER_IMAGE_TRACKER_H_
#define CC_TILES_CHECKER_IMAGE_TRACKER_H_

#include <unordered_map>
#include <vector>

#include "base/optional.h"
#include "cc/cc_export.h"
#include "cc/paint/image_id.h"
#include "cc/tiles/image_controller.h"
#include "third_party/skia/include/core/SkImage.h"

namespace cc {

class CC_EXPORT CheckerImageTrackerClient {
 public:
  virtual ~CheckerImageTrackerClient() = default;

  virtual void NeedsInvalidationForCheckerImagedTiles() = 0;
};

// CheckerImageTracker is used to track the set of images in a frame which are
// decoded asynchronously, using the ImageDecodeService, from the rasterization
// of tiles which depend on them. Once decoded, these images are stored for
// invalidation on the next sync tree. TakeImagesToInvalidateOnSyncTree will
// return this set and maintain a copy to keeps these images locked until the
// sync tree is activated.
// Note: It is illegal to call TakeImagesToInvalidateOnSyncTree for the next
// sync tree until the previous tree is activated.
class CC_EXPORT CheckerImageTracker {
 public:
  CheckerImageTracker(ImageController* image_controller,
                      CheckerImageTrackerClient* client,
                      bool enable_checker_imaging);
  ~CheckerImageTracker();

  // Returns true if the decode for |image| will be deferred to the image decode
  // service and it should be be skipped during raster.
  bool ShouldCheckerImage(const PaintImage& image, WhichTree tree);

  using ImageDecodeQueue = std::vector<PaintImage>;
  void ScheduleImageDecodeQueue(ImageDecodeQueue image_decode_queue);

  // Returns the set of images to invalidate on the sync tree.
  const PaintImageIdFlatSet& TakeImagesToInvalidateOnSyncTree();

  // Called when the sync tree is activated. Each call to
  // TakeImagesToInvalidateOnSyncTree() must be followed by this when the
  // invalidated sync tree is activated.
  void DidActivateSyncTree();

  // Called to reset the tracker state on navigation. This will release all
  // cached images. Setting |can_clear_decode_policy_tracking| will also result
  // in re-checkering any images already decoded by the tracker.
  void ClearTracker(bool can_clear_decode_policy_tracking);

  // Informs the tracker to not checker the given image. This can be used to opt
  // out of the checkering behavior for certain images, such as ones that were
  // decoded using the img.decode api.
  // Note that if the image is already being checkered, then it will continue to
  // do so. This call is meant to be issued prior to the image appearing during
  // raster.
  void DisallowCheckeringForImage(const PaintImage& image);

 private:
  enum class DecodePolicy {
    // The image can be decoded asynchronously from raster. When set, the image
    // is always skipped during rasterization of content that includes this
    // image until it has been decoded using the decode service.
    ASYNC,
    // The image has been decoded asynchronously once and should now be
    // synchronously rasterized with the content or the image has been
    // permanently vetoed from being decoded async.
    SYNC
  };

  // Wrapper to unlock an image decode requested from the ImageController on
  // destruction.
  class ScopedDecodeHolder {
   public:
    ScopedDecodeHolder(ImageController* controller,
                       ImageController::ImageDecodeRequestId request_id)
        : controller_(controller), request_id_(request_id) {}
    ~ScopedDecodeHolder() { controller_->UnlockImageDecode(request_id_); }

   private:
    ImageController* controller_;
    ImageController::ImageDecodeRequestId request_id_;

    DISALLOW_COPY_AND_ASSIGN(ScopedDecodeHolder);
  };

  void DidFinishImageDecode(PaintImage::Id image_id,
                            ImageController::ImageDecodeRequestId request_id,
                            ImageController::ImageDecodeResult result);

  // Called when the next request in the |image_decode_queue_| should be
  // scheduled with the image decode service.
  void ScheduleNextImageDecode();

  ImageController* image_controller_;
  CheckerImageTrackerClient* client_;
  const bool enable_checker_imaging_;

  // A set of images which have been decoded and are pending invalidation for
  // raster on the checkered tiles.
  PaintImageIdFlatSet images_pending_invalidation_;

  // A set of images which were invalidated on the current sync tree.
  PaintImageIdFlatSet invalidated_images_on_current_sync_tree_;

  // The queue of images pending decode. We maintain a queue to ensure that the
  // order in which images are decoded is aligned with the priority of the tiles
  // dependent on these images.
  ImageDecodeQueue image_decode_queue_;

  // The currently outstanding image decode that has been scheduled with the
  // decode service. There can be only one outstanding decode at a time.
  base::Optional<PaintImage> outstanding_image_decode_;

  // A map of ImageId to its DecodePolicy.
  std::unordered_map<PaintImage::Id, DecodePolicy> image_async_decode_state_;

  // A map of image id to image decode request id for images to be unlocked.
  std::unordered_map<PaintImage::Id, std::unique_ptr<ScopedDecodeHolder>>
      image_id_to_decode_;

  base::WeakPtrFactory<CheckerImageTracker> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CheckerImageTracker);
};

}  // namespace cc

#endif  // CC_TILES_CHECKER_IMAGE_TRACKER_H_
