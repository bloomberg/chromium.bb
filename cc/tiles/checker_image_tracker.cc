// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/checker_image_tracker.h"

#include "base/bind.h"
#include "base/stl_util.h"
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
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::FilterImagesForCheckeringForTile", "tree",
               tree);
  DCHECK(checkered_images->empty());

  base::EraseIf(*images,
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
}

const ImageIdFlatSet& CheckerImageTracker::TakeImagesToInvalidateOnSyncTree() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::TakeImagesToInvalidateOnSyncTree");
  DCHECK_EQ(invalidated_images_on_current_sync_tree_.size(), 0u)
      << "Sync tree can not be invalidated more than once";

  invalidated_images_on_current_sync_tree_.swap(images_pending_invalidation_);
  images_pending_invalidation_.clear();
  return invalidated_images_on_current_sync_tree_;
}

void CheckerImageTracker::DidActivateSyncTree() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::DidActivateSyncTree");
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
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::DidFinishImageDecode");
  TRACE_EVENT_ASYNC_END0("cc", "CheckerImageTracker::DeferImageDecode",
                         image_id);

  DCHECK_NE(result, ImageController::ImageDecodeResult::DECODE_NOT_REQUIRED);
  DCHECK_NE(pending_image_decodes_.count(image_id), 0u);
  DCHECK_NE(image_async_decode_state_.count(image_id), 0u);

  pending_image_decodes_.erase(image_id);

  image_async_decode_state_[image_id] = DecodePolicy::SYNC_DECODED_ONCE;
  images_pending_invalidation_.insert(image_id);
  client_->NeedsInvalidationForCheckerImagedTiles();
}

bool CheckerImageTracker::ShouldCheckerImage(const sk_sp<const SkImage>& image,
                                             WhichTree tree) {
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

  // If the image is pending invalidation, continue checkering it. All tiles
  // for these images will be invalidated on the next pending tree.
  if (images_pending_invalidation_.find(image->uniqueID()) !=
      images_pending_invalidation_.end()) {
    return true;
  }

  ImageId image_id = image->uniqueID();
  auto insert_result = image_async_decode_state_.insert(
      std::pair<ImageId, DecodePolicy>(image_id, DecodePolicy::ASYNC));
  auto it = insert_result.first;
  if (insert_result.second) {
    it->second = SafeSizeOfImage(image.get()) >= kMinImageSizeToCheckerBytes
                     ? DecodePolicy::ASYNC
                     : DecodePolicy::SYNC_PERMANENT;
  }

  return it->second == DecodePolicy::ASYNC;
}

void CheckerImageTracker::ScheduleImageDecodeIfNecessary(
    const sk_sp<const SkImage>& image) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::ScheduleImageDecodeIfNecessary");
  ImageId image_id = image->uniqueID();

  // Once an image has been decoded, they can still be present in the decode
  // queue (duplicate entries), or while an image is still being skipped on the
  // active tree. Check if the image is still ASYNC to see if a decode is
  // needed.
  auto it = image_async_decode_state_.find(image_id);
  DCHECK(it != image_async_decode_state_.end());
  if (it->second != DecodePolicy::ASYNC)
    return;

  // If a decode request is pending, we don't need to schedule another decode.
  if (pending_image_decodes_.count(image_id) != 0) {
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
