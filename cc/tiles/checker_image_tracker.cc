// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/checker_image_tracker.h"

#include "base/bind.h"
#include "base/trace_event/trace_event.h"

namespace cc {
namespace {
// The minimum size of an image that we should consider checkering.
size_t kMinImageSizeToCheckerBytes = 512 * 1024;

size_t SafeSizeOfImage(const SkImage* image) {
  base::CheckedNumeric<size_t> checked_size = 4;
  checked_size *= image->width();
  checked_size *= image->height();
  return checked_size.ValueOrDefault(std::numeric_limits<size_t>::max());
}

}  // namespace

CheckerImageTracker::CheckerImageTracker(ImageController* image_controller,
                                         CheckerImageTrackerClient* client,
                                         bool enable_checker_imaging)
    : image_controller_(image_controller),
      client_(client),
      enable_checker_imaging_(enable_checker_imaging),
      weak_factory_(this) {}

CheckerImageTracker::~CheckerImageTracker() {
  // Unlock all images pending decode requests.
  for (auto it : image_id_to_decode_request_id_)
    image_controller_->UnlockImageDecode(it.second);
}

void CheckerImageTracker::FilterImagesForCheckeringForTile(
    std::vector<DrawImage>* images,
    ImageIdFlatSet* checkered_images,
    WhichTree tree) {
  DCHECK(checkered_images->empty());

  auto images_to_checker = std::remove_if(
      images->begin(), images->end(),
      [this, tree, &checkered_images](const DrawImage& draw_image) {
        const sk_sp<const SkImage>& image = draw_image.image();
        DCHECK(image->isLazyGenerated());
        if (ShouldCheckerImage(image, tree)) {
          ScheduleImageDecodeIfNecessary(image);
          checkered_images->insert(image->uniqueID());
          return true;
        }
        return false;
      });
  images->erase(images_to_checker, images->end());
}

const ImageIdFlatSet& CheckerImageTracker::TakeImagesToInvalidateOnSyncTree() {
  DCHECK_EQ(invalidated_images_on_current_sync_tree_.size(), 0u)
      << "Sync tree can not be invalidated more than once";

  invalidated_images_on_current_sync_tree_.swap(images_pending_invalidation_);
  images_pending_invalidation_.clear();
  return invalidated_images_on_current_sync_tree_;
}

void CheckerImageTracker::DidActivateSyncTree() {
  for (auto image_id : invalidated_images_on_current_sync_tree_) {
    auto it = image_id_to_decode_request_id_.find(image_id);
    image_controller_->UnlockImageDecode(it->second);
    image_id_to_decode_request_id_.erase(it);
  }

  invalidated_images_on_current_sync_tree_.clear();
}

void CheckerImageTracker::DidFinishImageDecode(
    ImageId image_id,
    ImageController::ImageDecodeRequestId request_id,
    ImageController::ImageDecodeResult result) {
  TRACE_EVENT_ASYNC_END0("cc", "CheckerImageTracker::DeferImageDecode",
                         image_id);

  DCHECK_NE(result, ImageController::ImageDecodeResult::DECODE_NOT_REQUIRED);
  DCHECK_NE(pending_image_decodes_.count(image_id), 0u);
  pending_image_decodes_.erase(image_id);

  images_decoded_once_.insert(image_id);
  images_pending_invalidation_.insert(image_id);
  client_->NeedsInvalidationForCheckerImagedTiles();
}

bool CheckerImageTracker::ShouldCheckerImage(const sk_sp<const SkImage>& image,
                                             WhichTree tree) const {
  TRACE_EVENT1("cc", "CheckerImageTracker::ShouldCheckerImage", "image_id",
               image->uniqueID());

  if (!enable_checker_imaging_)
    return false;

  // If the image was invalidated on the current sync tree and the tile is
  // for the active tree, continue checkering it on the active tree to ensure
  // the image update is atomic for the frame.
  if (invalidated_images_on_current_sync_tree_.count(image->uniqueID()) != 0 &&
      tree == WhichTree::ACTIVE_TREE) {
    return true;
  }

  // If a decode request is pending for this image, continue checkering it.
  if (pending_image_decodes_.find(image->uniqueID()) !=
      pending_image_decodes_.end()) {
    return true;
  }

  // If the image is pending invalidation, continue checkering it. All tiles
  // for these images will be invalidated on the next pending tree.
  if (images_pending_invalidation_.find(image->uniqueID()) !=
      images_pending_invalidation_.end()) {
    return true;
  }

  // If the image has been decoded once before, don't checker it again.
  if (images_decoded_once_.find(image->uniqueID()) !=
      images_decoded_once_.end()) {
    return false;
  }

  return SafeSizeOfImage(image.get()) >= kMinImageSizeToCheckerBytes;
}

void CheckerImageTracker::ScheduleImageDecodeIfNecessary(
    const sk_sp<const SkImage>& image) {
  ImageId image_id = image->uniqueID();

  // If the image has already been decoded, or a decode request is pending, we
  // don't need to schedule another decode.
  if (images_decoded_once_.count(image_id) != 0 ||
      pending_image_decodes_.count(image_id) != 0) {
    return;
  }

  TRACE_EVENT_ASYNC_BEGIN0("cc", "CheckerImageTracker::DeferImageDecode",
                           image_id);
  DCHECK_EQ(image_id_to_decode_request_id_.count(image_id), 0U);

  image_id_to_decode_request_id_[image_id] =
      image_controller_->QueueImageDecode(
          image, base::Bind(&CheckerImageTracker::DidFinishImageDecode,
                            weak_factory_.GetWeakPtr(), image_id));
  pending_image_decodes_.insert(image_id);
}

}  // namespace cc
