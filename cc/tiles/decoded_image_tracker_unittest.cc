// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "cc/tiles/decoded_image_tracker.h"
#include "cc/tiles/image_controller.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class TestImageController : public ImageController {
 public:
  TestImageController() : ImageController(nullptr, nullptr) {}

  void UnlockImageDecode(ImageDecodeRequestId id) override {
    auto it = std::find(locked_ids_.begin(), locked_ids_.end(), id);
    ASSERT_FALSE(it == locked_ids_.end());
    locked_ids_.erase(it);
  }

  ImageDecodeRequestId QueueImageDecode(
      sk_sp<const SkImage> image,
      const ImageDecodedCallback& callback) override {
    auto id = next_id_++;
    locked_ids_.push_back(id);
    callback.Run(id, ImageDecodeResult::SUCCESS);
    return id;
  }

  size_t num_locked_images() { return locked_ids_.size(); }

 private:
  ImageDecodeRequestId next_id_ = 1;
  std::vector<ImageDecodeRequestId> locked_ids_;
};

class DecodedImageTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    decoded_image_tracker_.set_image_controller(image_controller());
  }

  TestImageController* image_controller() { return &image_controller_; }
  DecodedImageTracker* decoded_image_tracker() {
    return &decoded_image_tracker_;
  }

 private:
  TestImageController image_controller_;
  DecodedImageTracker decoded_image_tracker_;
};

TEST_F(DecodedImageTrackerTest, QueueImageLocksImages) {
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      nullptr, base::Bind([](bool* locked, bool success) { *locked = true; },
                          base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());
}

TEST_F(DecodedImageTrackerTest, NotifyFrameFinishedUnlocksImages) {
  bool locked = false;
  decoded_image_tracker()->QueueImageDecode(
      nullptr, base::Bind([](bool* locked, bool success) { *locked = true; },
                          base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  decoded_image_tracker()->NotifyFrameFinished();
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  locked = false;
  decoded_image_tracker()->QueueImageDecode(
      nullptr, base::Bind([](bool* locked, bool success) { *locked = true; },
                          base::Unretained(&locked)));
  EXPECT_TRUE(locked);
  EXPECT_EQ(2u, image_controller()->num_locked_images());

  decoded_image_tracker()->NotifyFrameFinished();
  EXPECT_EQ(1u, image_controller()->num_locked_images());

  decoded_image_tracker()->NotifyFrameFinished();
  EXPECT_EQ(0u, image_controller()->num_locked_images());
}

}  // namespace cc
