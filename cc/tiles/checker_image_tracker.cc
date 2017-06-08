// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/checker_image_tracker.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
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

CheckerImageTracker::~CheckerImageTracker() = default;

void CheckerImageTracker::ScheduleImageDecodeQueue(
    ImageDecodeQueue image_decode_queue) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::ScheduleImageDecodeQueue");
  // Only checker-imaged (async updated) images are decoded using the image
  // decode service. If |enable_checker_imaging_| is false, no image should
  // be checkered.
  DCHECK(image_decode_queue.empty() || enable_checker_imaging_);

  image_decode_queue_ = std::move(image_decode_queue);
  ScheduleNextImageDecode();
}

const PaintImageIdFlatSet&
CheckerImageTracker::TakeImagesToInvalidateOnSyncTree() {
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
  for (auto image_id : invalidated_images_on_current_sync_tree_)
    image_id_to_decode_.erase(image_id);
  invalidated_images_on_current_sync_tree_.clear();
}

void CheckerImageTracker::ClearTracker(bool can_clear_decode_policy_tracking) {
  // Unlock all images and tracking for images pending invalidation. The
  // |images_invalidated_on_current_sync_tree_| will be cleared when the sync
  // tree is activated.
  //
  // Note that we assume that any images with DecodePolicy::ASYNC, which may be
  // checkered, are safe to stop tracking here and will either be re-checkered
  // and invalidated when the decode completes or be invalidated externally.
  // This is because the policy decision for checkering an image is based on
  // inputs received from a PaintImage in the DisplayItemList. The policy chosen
  // for a PaintImage should remain unchanged.
  // If the external inputs for deciding the decode policy for an image change,
  // they should be accompanied with an invalidation during paint.
  image_id_to_decode_.clear();

  if (can_clear_decode_policy_tracking) {
    image_async_decode_state_.clear();
  } else {
    // If we can't clear the decode policy, we need to make sure we still
    // re-decode and checker images that were pending invalidation.
    for (auto image_id : images_pending_invalidation_) {
      auto it = image_async_decode_state_.find(image_id);
      DCHECK(it != image_async_decode_state_.end());
      DCHECK_EQ(it->second, DecodePolicy::SYNC);
      it->second = DecodePolicy::ASYNC;
    }
  }
  images_pending_invalidation_.clear();
}

void CheckerImageTracker::DisallowCheckeringForImage(const PaintImage& image) {
  image_async_decode_state_.insert(
      std::make_pair(image.stable_id(), DecodePolicy::SYNC));
}

void CheckerImageTracker::DidFinishImageDecode(
    PaintImage::Id image_id,
    ImageController::ImageDecodeRequestId request_id,
    ImageController::ImageDecodeResult result) {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::DidFinishImageDecode");
  TRACE_EVENT_ASYNC_END0("cc", "CheckerImageTracker::DeferImageDecode",
                         image_id);

  DCHECK_NE(ImageController::ImageDecodeResult::DECODE_NOT_REQUIRED, result);
  DCHECK_EQ(outstanding_image_decode_.value().stable_id(), image_id);
  outstanding_image_decode_.reset();

  // The async decode state may have been cleared if the tracker was cleared
  // before this decode could be finished.
  auto it = image_async_decode_state_.find(image_id);
  if (it == image_async_decode_state_.end()) {
    DCHECK_EQ(image_id_to_decode_.count(image_id), 0u);
    return;
  }

  it->second = DecodePolicy::SYNC;
  images_pending_invalidation_.insert(image_id);
  ScheduleNextImageDecode();
  client_->NeedsInvalidationForCheckerImagedTiles();
}

bool CheckerImageTracker::ShouldCheckerImage(const PaintImage& image,
                                             WhichTree tree) {
  TRACE_EVENT1("cc", "CheckerImageTracker::ShouldCheckerImage", "image_id",
               image.stable_id());

  if (!enable_checker_imaging_)
    return false;

  PaintImage::Id image_id = image.stable_id();

  // If the image was invalidated on the current sync tree and the tile is
  // for the active tree, continue checkering it on the active tree to ensure
  // the image update is atomic for the frame.
  if (invalidated_images_on_current_sync_tree_.count(image_id) != 0 &&
      tree == WhichTree::ACTIVE_TREE) {
    return true;
  }

  // If the image is pending invalidation, continue checkering it. All tiles
  // for these images will be invalidated on the next pending tree.
  if (images_pending_invalidation_.find(image_id) !=
      images_pending_invalidation_.end()) {
    return true;
  }

  auto insert_result = image_async_decode_state_.insert(
      std::pair<PaintImage::Id, DecodePolicy>(image_id, DecodePolicy::SYNC));
  auto it = insert_result.first;
  if (insert_result.second) {
    bool can_checker_image =
        image.animation_type() == PaintImage::AnimationType::STATIC &&
        image.completion_state() == PaintImage::CompletionState::DONE;
    if (can_checker_image) {
      size_t size = SafeSizeOfImage(image.sk_image().get());
      it->second = (size >= kMinImageSizeToCheckerBytes &&
                    size <= image_controller_->image_cache_max_limit_bytes())
                       ? DecodePolicy::ASYNC
                       : DecodePolicy::SYNC;
    }
  }

  return it->second == DecodePolicy::ASYNC;
}

void CheckerImageTracker::ScheduleNextImageDecode() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::ScheduleNextImageDecode");
  // We can have only one outsanding decode pending completion with the decode
  // service. We'll come back here when it is completed.
  if (outstanding_image_decode_.has_value())
    return;

  while (!image_decode_queue_.empty()) {
    auto candidate = std::move(image_decode_queue_.front());
    image_decode_queue_.erase(image_decode_queue_.begin());

    // Once an image has been decoded, it can still be present in the decode
    // queue (duplicate entries), or while an image is still being skipped on
    // the active tree. Check if the image is still ASYNC to see if a decode is
    // needed.
    PaintImage::Id image_id = candidate.stable_id();
    auto it = image_async_decode_state_.find(image_id);
    DCHECK(it != image_async_decode_state_.end());
    if (it->second != DecodePolicy::ASYNC)
      continue;

    outstanding_image_decode_.emplace(candidate);
    break;
  }

  // We either found an image to decode or we reached the end of the queue. If
  // we couldn't find an image, we're done.
  if (!outstanding_image_decode_.has_value()) {
    DCHECK(image_decode_queue_.empty());
    return;
  }

  PaintImage::Id image_id = outstanding_image_decode_.value().stable_id();
  DCHECK_EQ(image_id_to_decode_.count(image_id), 0u);
  TRACE_EVENT_ASYNC_BEGIN0("cc", "CheckerImageTracker::DeferImageDecode",
                           image_id);
  ImageController::ImageDecodeRequestId request_id =
      image_controller_->QueueImageDecode(
          outstanding_image_decode_.value().sk_image(),
          base::Bind(&CheckerImageTracker::DidFinishImageDecode,
                     weak_factory_.GetWeakPtr(), image_id));

  image_id_to_decode_.emplace(image_id, base::MakeUnique<ScopedDecodeHolder>(
                                            image_controller_, request_id));
}

}  // namespace cc
