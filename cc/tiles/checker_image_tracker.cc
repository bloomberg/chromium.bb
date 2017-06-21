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

std::string ToString(PaintImage::Id paint_image_id,
                     SkImageId sk_image_id,
                     bool complete,
                     bool static_image,
                     bool fits_size_constraints,
                     bool is_multipart,
                     size_t size) {
  std::ostringstream str;
  str << "paint_image_id[" << paint_image_id << "] sk_image_id[" << sk_image_id
      << "] complete[" << complete << "] static[" << static_image
      << "], fits_size_constraints[" << fits_size_constraints << "], size["
      << size << "]"
      << " is_multipart[" << is_multipart << "]";
  return str.str();
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
      DCHECK_EQ(it->second.policy, DecodePolicy::SYNC);
      it->second.policy = DecodePolicy::ASYNC;
    }
  }
  images_pending_invalidation_.clear();
}

void CheckerImageTracker::DisallowCheckeringForImage(const PaintImage& image) {
  image_async_decode_state_.insert(
      std::make_pair(image.stable_id(), DecodeState()));
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

  it->second.policy = DecodePolicy::SYNC;
  images_pending_invalidation_.insert(image_id);
  ScheduleNextImageDecode();
  client_->NeedsInvalidationForCheckerImagedTiles();
}

bool CheckerImageTracker::ShouldCheckerImage(const DrawImage& draw_image,
                                             WhichTree tree) {
  const PaintImage& image = draw_image.paint_image();
  PaintImage::Id image_id = image.stable_id();
  TRACE_EVENT1("cc", "CheckerImageTracker::ShouldCheckerImage", "image_id",
               image_id);

  if (!enable_checker_imaging_)
    return false;

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
      std::pair<PaintImage::Id, DecodeState>(image_id, DecodeState()));
  auto it = insert_result.first;
  if (insert_result.second) {
    bool complete =
        image.completion_state() == PaintImage::CompletionState::DONE;
    bool static_image =
        image.animation_type() == PaintImage::AnimationType::STATIC;
    size_t size = SafeSizeOfImage(image.sk_image().get());
    bool fits_size_constraints =
        size >= kMinImageSizeToCheckerBytes &&
        size <= image_controller_->image_cache_max_limit_bytes();

    // The following conditions must be true for an image to be checkerable:
    //
    // 1) Complete: The data for the image should have been completely loaded.
    //
    // 2) Static: Animated images/video frames can not be checkered.
    //
    // 3) Size constraints: Small images for which the decode is expected to
    // be fast and large images which would breach the image cache budget and
    // go through the at-raster decode path are not checkered.
    //
    // 4) Multipart images: Multipart images can be used to display mjpg video
    // frames, checkering which would cause each video frame to flash and
    // therefore should not be checkered.
    bool can_checker_image = complete && static_image &&
                             fits_size_constraints && !image.is_multipart();
    if (can_checker_image)
      it->second.policy = DecodePolicy::ASYNC;

    TRACE_EVENT2(
        TRACE_DISABLED_BY_DEFAULT("cc.debug"),
        "CheckerImageTracker::CheckerImagingDecision", "can_checker_image",
        can_checker_image, "image_params",
        ToString(image_id, image.sk_image()->uniqueID(), complete, static_image,
                 fits_size_constraints, image.is_multipart(), size));
  }

  // Update the decode state from the latest image we have seen. Note that it
  // is not necessary to perform this in the early out cases above since in
  // each of those cases the image has already been decoded.
  UpdateDecodeState(draw_image, image_id, &it->second);

  return it->second.policy == DecodePolicy::ASYNC;
}

void CheckerImageTracker::UpdateDecodeState(const DrawImage& draw_image,
                                            PaintImage::Id paint_image_id,
                                            DecodeState* decode_state) {
  // If the policy is not async then either we decoded this image already or
  // we decided not to ever checker it.
  if (decode_state->policy != DecodePolicy::ASYNC)
    return;

  // If the decode is already in flight, then we will have to live with what we
  // have now.
  if (outstanding_image_decode_.has_value() &&
      outstanding_image_decode_.value().stable_id() == paint_image_id) {
    return;
  }

  // Choose the max scale and filter quality. This keeps the memory usage to the
  // minimum possible while still increasing the possibility of getting a cache
  // hit.
  decode_state->scale = SkSize::Make(
      std::max(decode_state->scale.fWidth, draw_image.scale().fWidth),
      std::max(decode_state->scale.fHeight, draw_image.scale().fHeight));
  decode_state->filter_quality =
      std::max(decode_state->filter_quality, draw_image.filter_quality());
  decode_state->color_space = draw_image.target_color_space();
}

void CheckerImageTracker::ScheduleNextImageDecode() {
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("cc.debug"),
               "CheckerImageTracker::ScheduleNextImageDecode");
  // We can have only one outstanding decode pending completion with the decode
  // service. We'll come back here when it is completed.
  if (outstanding_image_decode_.has_value())
    return;

  DrawImage draw_image;
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
    if (it->second.policy != DecodePolicy::ASYNC)
      continue;

    draw_image = DrawImage(candidate, candidate.sk_image()->bounds(),
                           it->second.filter_quality,
                           SkMatrix::MakeScale(it->second.scale.width(),
                                               it->second.scale.height()),
                           it->second.color_space);
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
          draw_image, base::Bind(&CheckerImageTracker::DidFinishImageDecode,
                                 weak_factory_.GetWeakPtr(), image_id));

  image_id_to_decode_.emplace(image_id, base::MakeUnique<ScopedDecodeHolder>(
                                            image_controller_, request_id));
}

}  // namespace cc
