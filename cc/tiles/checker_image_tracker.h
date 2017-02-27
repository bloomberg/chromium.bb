// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_CHECKER_IMAGE_TRACKER_H_
#define CC_TILES_CHECKER_IMAGE_TRACKER_H_

#include <unordered_map>
#include <vector>

#include "cc/base/cc_export.h"
#include "cc/playback/image_id.h"
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

  // Given the |images| for a tile, filters the images which will be deferred
  // asynchronously using the image decoded service, eliminating them from
  // |images| adds them to the |checkered_images| set, so they can be skipped
  // during the rasterization of this tile.
  // The entries remaining in |images| are for images for which a cached decode
  // from the image decode service is available, or which must be decoded before
  // before this tile can be rasterized.
  void FilterImagesForCheckeringForTile(std::vector<DrawImage>* images,
                                        ImageIdFlatSet* checkered_images,
                                        WhichTree tree);

  // Returns the set of images to invalidate on the sync tree.
  const ImageIdFlatSet& TakeImagesToInvalidateOnSyncTree();

  void DidActivateSyncTree();

 private:
  void DidFinishImageDecode(ImageId image_id,
                            ImageController::ImageDecodeRequestId request_id,
                            ImageController::ImageDecodeResult result);

  // Returns true if the decode for |image| will be deferred to the image decode
  // service and it should be be skipped during raster.
  bool ShouldCheckerImage(const sk_sp<const SkImage>& image,
                          WhichTree tree) const;

  void ScheduleImageDecodeIfNecessary(const sk_sp<const SkImage>& image);

  ImageController* image_controller_;
  CheckerImageTrackerClient* client_;
  const bool enable_checker_imaging_;

  // A set of images which have been decoded and are pending invalidation for
  // raster on the checkered tiles.
  ImageIdFlatSet images_pending_invalidation_;

  // A set of images which were invalidated on the current sync tree.
  ImageIdFlatSet invalidated_images_on_current_sync_tree_;

  // A set of images which are currently pending decode from the image decode
  // service.
  // TODO(khushalsagar): This should be a queue that gets re-built each time we
  // do a PrepareTiles? See crbug.com/689184.
  ImageIdFlatSet pending_image_decodes_;

  // A set of images which have been decoded at least once from the
  // ImageDecodeService and should not be checkered again.
  // TODO(khushalsagar): Limit the size of this set.
  // TODO(khushalsagar): Plumb navigation changes here to reset this. See
  // crbug.com/693228.
  std::unordered_set<ImageId> images_decoded_once_;

  // A map of image id to image decode request id for images to be unlocked.
  std::unordered_map<ImageId, ImageController::ImageDecodeRequestId>
      image_id_to_decode_request_id_;

  base::WeakPtrFactory<CheckerImageTracker> weak_factory_;
};

}  // namespace cc

#endif  // CC_TILES_CHECKER_IMAGE_TRACKER_H_
