// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/tiles/checker_image_tracker.h"

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/test/skia_common.h"
#include "cc/tiles/image_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {
namespace {

const int kCheckerableImageDimension = 512;
const int kNonCheckerableImageDimension = 16;

class TestImageController : public ImageController {
 public:
  // We can use the same thread for the image worker because all use of it in
  // the ImageController is over-ridden here.
  TestImageController()
      : ImageController(base::ThreadTaskRunnerHandle::Get().get(),
                        base::ThreadTaskRunnerHandle::Get()) {}

  ~TestImageController() override { DCHECK_EQ(locked_images_.size(), 0U); }

  int num_of_locked_images() const { return locked_images_.size(); }

  void UnlockImageDecode(ImageDecodeRequestId id) override {
    DCHECK_EQ(locked_images_.count(id), 1U);
    locked_images_.erase(id);
  }

  ImageDecodeRequestId QueueImageDecode(
      sk_sp<const SkImage> image,
      const ImageDecodedCallback& callback) override {
    ImageDecodeRequestId request_id = next_image_request_id_++;

    // The tracker should request a decode only once.
    EXPECT_EQ(decodes_requested_.count(image->uniqueID()), 0u);
    decodes_requested_.insert(image->uniqueID());

    locked_images_.insert(request_id);

    // Post the callback asynchronously to match the behaviour in
    // ImageController.
    worker_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(callback, request_id, ImageDecodeResult::SUCCESS));

    return request_id;
  }

 private:
  ImageDecodeRequestId next_image_request_id_ = 1U;
  std::unordered_set<ImageDecodeRequestId> locked_images_;
  ImageIdFlatSet decodes_requested_;
};

class CheckerImageTrackerTest : public testing::Test,
                                public CheckerImageTrackerClient {
 public:
  enum class ImageType { CHECKERABLE, NON_CHECKERABLE };

  void SetUpTracker(bool checker_images_enabled) {
    checker_image_tracker_ = base::MakeUnique<CheckerImageTracker>(
        &image_controller_, this, checker_images_enabled);
  }

  void TearDown() override { checker_image_tracker_.reset(); }

  DrawImage CreateImage(ImageType image_type) {
    int dimension = image_type == ImageType::CHECKERABLE
                        ? kCheckerableImageDimension
                        : kNonCheckerableImageDimension;
    sk_sp<SkImage> image =
        CreateDiscardableImage(gfx::Size(dimension, dimension));
    return DrawImage(image, SkIRect::MakeWH(image->width(), image->height()),
                     kNone_SkFilterQuality, SkMatrix::I());
  }

  // CheckerImageTrackerClient implementation.
  void NeedsInvalidationForCheckerImagedTiles() override {
    invalidation_request_pending_ = true;
  }

 protected:
  TestImageController image_controller_;
  std::unique_ptr<CheckerImageTracker> checker_image_tracker_;

  bool invalidation_request_pending_ = false;
};

TEST_F(CheckerImageTrackerTest, CheckerImagesDisabled) {
  // Ensures that the tracker doesn't filter any images for checkering if it is
  // disabled.
  SetUpTracker(false);

  std::vector<DrawImage> draw_images;
  ImageIdFlatSet checkered_images;
  draw_images.push_back(CreateImage(ImageType::CHECKERABLE));
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 1U);
  EXPECT_EQ(checkered_images.size(), 0U);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 0);
}

TEST_F(CheckerImageTrackerTest, UpdatesImagesAtomically) {
  // Ensures that the tracker updates images atomically for each frame.
  SetUpTracker(true);

  DrawImage checkerable_image = CreateImage(ImageType::CHECKERABLE);
  DrawImage non_checkerable_image = CreateImage(ImageType::NON_CHECKERABLE);
  ImageIdFlatSet checkered_images;
  std::vector<DrawImage> draw_images;

  // First request to filter images.
  draw_images.push_back(checkerable_image);
  draw_images.push_back(non_checkerable_image);
  draw_images.push_back(checkerable_image);
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);

  EXPECT_EQ(draw_images.size(), 1U);
  EXPECT_EQ(draw_images[0].image(), non_checkerable_image.image());
  EXPECT_EQ(checkered_images.size(), 1U);
  EXPECT_EQ(checkered_images.count(checkerable_image.image()->uniqueID()), 1U);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);

  // Run pending task to indicate completion of decode request to the tracker.
  // This should send an impl-side invalidation request to the client. The
  // images must remain locked until the sync tree to which the invalidations
  // are added is activated.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invalidation_request_pending_);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);

  // Continue checkering the image until the set of images to invalidate is
  // pulled.
  draw_images.clear();
  draw_images.push_back(checkerable_image);
  checkered_images.clear();
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 0U);
  EXPECT_EQ(checkered_images.size(), 1U);
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);

  ImageIdFlatSet invalidated_images =
      checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  EXPECT_EQ(invalidated_images.size(), 1U);
  EXPECT_EQ(invalidated_images.count(checkerable_image.image()->uniqueID()),
            1U);

  // Use the same set of draw images to ensure that they are not checkered on
  // the pending tree now.
  draw_images.clear();
  draw_images.push_back(checkerable_image);
  draw_images.push_back(non_checkerable_image);
  checkered_images.clear();
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 2U);
  EXPECT_EQ(checkered_images.size(), 0U);

  // Use this set to make the same request from the active tree, we should
  // continue checkering this image on the active tree until activation.
  draw_images.clear();
  draw_images.push_back(checkerable_image);
  draw_images.push_back(non_checkerable_image);
  checkered_images.clear();
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::ACTIVE_TREE);
  EXPECT_EQ(draw_images.size(), 1U);
  EXPECT_EQ(draw_images[0].image(), non_checkerable_image.image());
  EXPECT_EQ(checkered_images.size(), 1U);
  EXPECT_EQ(checkered_images.count(checkerable_image.image()->uniqueID()), 1U);

  // Activate the sync tree. The images should be unlocked upon activation.
  EXPECT_EQ(image_controller_.num_of_locked_images(), 1);
  checker_image_tracker_->DidActivateSyncTree();
}

TEST_F(CheckerImageTrackerTest, NoConsecutiveCheckeringForImage) {
  // Ensures that if an image is decoded and invalidated once, it is not
  // checkered again in subsequent frames.
  SetUpTracker(true);

  DrawImage checkerable_image = CreateImage(ImageType::CHECKERABLE);
  DrawImage non_checkerable_image = CreateImage(ImageType::NON_CHECKERABLE);
  ImageIdFlatSet checkered_images;
  std::vector<DrawImage> draw_images;

  draw_images.clear();
  draw_images.push_back(checkerable_image);
  checkered_images.clear();
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 0U);
  EXPECT_EQ(checkered_images.size(), 1U);

  // Trigger decode completion, take images to invalidate and activate the sync
  // tree.
  base::RunLoop().RunUntilIdle();
  checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  checker_image_tracker_->DidActivateSyncTree();

  // Subsequent requests for this image should not be checkered.
  draw_images.clear();
  draw_images.push_back(checkerable_image);
  checkered_images.clear();
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 1U);
  EXPECT_EQ(checkered_images.size(), 0U);
}

TEST_F(CheckerImageTrackerTest,
       TracksCheckeredImagesSeperatelyInConsecutiveFrames) {
  // Ensures that the set of images being checkered on the pending tree, and the
  // active tree are tracked correctly.
  SetUpTracker(true);

  DrawImage checkerable_image1 = CreateImage(ImageType::CHECKERABLE);
  ImageIdFlatSet checkered_images;
  std::vector<DrawImage> draw_images;

  // First request to filter images on the pending and active tree.
  draw_images.push_back(checkerable_image1);
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 0U);
  EXPECT_EQ(checkered_images.size(), 1U);

  // The image is also checkered on the active tree while a decode request is
  // pending.
  draw_images.clear();
  checkered_images.clear();
  draw_images.push_back(checkerable_image1);
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::ACTIVE_TREE);
  EXPECT_EQ(draw_images.size(), 0U);
  EXPECT_EQ(checkered_images.size(), 1U);

  // Trigger decode completion and take images to invalidate on the sync tree.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(invalidation_request_pending_);
  ImageIdFlatSet invalidated_images =
      checker_image_tracker_->TakeImagesToInvalidateOnSyncTree();
  EXPECT_EQ(invalidated_images.size(), 1U);
  EXPECT_EQ(invalidated_images.count(checkerable_image1.image()->uniqueID()),
            1U);

  // Second request to filter the same image on the pending and active tree. It
  // should be checkered on the active tree, but not the pending tree.
  draw_images.clear();
  checkered_images.clear();
  draw_images.push_back(checkerable_image1);
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 1U);
  EXPECT_EQ(checkered_images.size(), 0U);

  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::ACTIVE_TREE);
  EXPECT_EQ(draw_images.size(), 0U);
  EXPECT_EQ(checkered_images.size(), 1U);

  // New checkerable image on the pending tree.
  DrawImage checkerable_image2 = CreateImage(ImageType::CHECKERABLE);
  draw_images.clear();
  checkered_images.clear();
  draw_images.push_back(checkerable_image2);
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::PENDING_TREE);
  EXPECT_EQ(draw_images.size(), 0U);
  EXPECT_EQ(checkered_images.size(), 1U);

  // Activate the sync tree. The initial image should no longer be checkered on
  // the active tree.
  checker_image_tracker_->DidActivateSyncTree();

  draw_images.clear();
  checkered_images.clear();
  draw_images.push_back(checkerable_image1);
  checker_image_tracker_->FilterImagesForCheckeringForTile(
      &draw_images, &checkered_images, WhichTree::ACTIVE_TREE);
  EXPECT_EQ(draw_images.size(), 1U);
  EXPECT_EQ(checkered_images.size(), 0U);
}

}  // namespace
}  // namespace cc
