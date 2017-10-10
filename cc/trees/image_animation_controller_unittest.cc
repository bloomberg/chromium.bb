// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/trees/image_animation_controller.h"

#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/test/gtest_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cc {

class FakeAnimationDriver : public ImageAnimationController::AnimationDriver {
 public:
  FakeAnimationDriver() {}
  ~FakeAnimationDriver() override {}

  void set_should_animate(bool should_animate) {
    should_animate_ = should_animate;
  }

  // ImageAnimationController::AnimationDriver implementation.
  bool ShouldAnimate(PaintImage::Id paint_image_id) const override {
    return should_animate_;
  }

 private:
  bool should_animate_ = true;
};

class DelayTrackingTaskRunner : public base::SingleThreadTaskRunner {
 public:
  explicit DelayTrackingTaskRunner(base::SingleThreadTaskRunner* task_runner)
      : task_runner_(task_runner) {}

  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override {
    last_delay_.emplace(delay);
    return task_runner_->PostTask(from_here, std::move(task));
  }

  bool RunsTasksInCurrentSequence() const override {
    return task_runner_->RunsTasksInCurrentSequence();
  }

  bool PostNonNestableDelayedTask(const base::Location& from_here,
                                  base::OnceClosure task,
                                  base::TimeDelta delay) override {
    last_delay_.emplace(delay);
    return task_runner_->PostTask(from_here, std::move(task));
  }

  void VerifyDelay(base::TimeDelta expected) {
    DCHECK(last_delay_.has_value());
    EXPECT_EQ(last_delay_.value(), expected);
    last_delay_.reset();
  }

  bool has_delay() const { return last_delay_.has_value(); }

 private:
  ~DelayTrackingTaskRunner() override = default;

  base::Optional<base::TimeDelta> last_delay_;
  base::SingleThreadTaskRunner* task_runner_;
};

class ImageAnimationControllerTest : public testing::Test {
 public:
  void SetUp() override {
    task_runner_ =
        new DelayTrackingTaskRunner(base::ThreadTaskRunnerHandle::Get().get());
    base::Closure invalidation_callback =
        base::Bind(&ImageAnimationControllerTest::RequestInvalidation,
                   base::Unretained(this));
    controller_ = base::MakeUnique<ImageAnimationController>(
        task_runner_.get(), invalidation_callback);
    now_ += base::TimeDelta::FromSeconds(10);
  }

  void TearDown() override { controller_.reset(); }

  void LoopOnceNoDelay(PaintImage::Id paint_image_id,
                       const std::vector<FrameMetadata>& frames,
                       size_t num_of_frames_to_loop,
                       int repetitions_completed,
                       bool restarting = false) {
    DCHECK_LE(num_of_frames_to_loop, frames.size());

    invalidation_count_ = 0;
    for (size_t i = 0; i < num_of_frames_to_loop; ++i) {
      SCOPED_TRACE(i);

      // Run the pending invalidation.
      base::RunLoop().RunUntilIdle();
      EXPECT_EQ(invalidation_count_, static_cast<int>(i + 1));

      // Animate the image on the sync tree.
      auto animated_images = controller_->AnimateForSyncTree(now_);

      // No frames should have been skipped since we add no delay in advancing
      // the animation.
      EXPECT_EQ(
          controller_->GetLastNumOfFramesSkippedForTesting(paint_image_id), 0u);

      EXPECT_EQ(controller_->GetFrameIndexForImage(paint_image_id,
                                                   WhichTree::PENDING_TREE),
                i);
      if (i == 0u && !restarting) {
        // If we are displaying the first frame on the pending tree, then the
        // active tree has the first frame as well if this is the first loop,
        // otherwise it should be the last frame since we are starting a new
        // loop.
        size_t active_index = 0u;
        if (repetitions_completed != 0)
          active_index = frames.size() - 1;
        EXPECT_EQ(controller_->GetFrameIndexForImage(paint_image_id,
                                                     WhichTree::ACTIVE_TREE),
                  active_index);
      } else if (i != 0u) {
        EXPECT_EQ(controller_->GetFrameIndexForImage(paint_image_id,
                                                     WhichTree::ACTIVE_TREE),
                  i - 1);
      }

      if (i == 0u && repetitions_completed == 0 && !restarting) {
        // Starting the animation does not perform any invalidation.
        EXPECT_EQ(animated_images.size(), 0u);
      } else {
        EXPECT_EQ(animated_images.size(), 1u);
        EXPECT_EQ(animated_images.count(paint_image_id), 1u);
      }

      // Animating should schedule an invalidation for the next frame, until we
      // reach the last frame.
      if (i != num_of_frames_to_loop - 1)
        task_runner_->VerifyDelay(frames[i].duration);

      // Activate and advance time to the next frame.
      controller_->DidActivate();
      AdvanceNow(frames[i].duration);
    }
  }

 protected:
  void RequestInvalidation() { invalidation_count_++; }

  void AdvanceNow(base::TimeDelta delta) { now_ += delta; }

  base::TimeTicks now_;
  int invalidation_count_ = 0;
  std::unique_ptr<ImageAnimationController> controller_;
  scoped_refptr<DelayTrackingTaskRunner> task_runner_;
};

TEST_F(ImageAnimationControllerTest, AnimationWithDelays) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(5)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE, frames,
      kAnimationLoopInfinite, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Display 2 loops in the animation.
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 0);
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 1);

  // now_ is set to the time at which the first frame should be displayed for
  // the third iteration. Add a delay that causes us to skip the first frame.
  base::TimeDelta additional_delay = base::TimeDelta::FromMilliseconds(1);
  AdvanceNow(data.frames[0].duration + additional_delay);
  auto animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 1u);
  EXPECT_EQ(animated_images.count(data.paint_image_id), 1u);
  EXPECT_EQ(
      controller_->GetLastNumOfFramesSkippedForTesting(data.paint_image_id),
      1u);

  // The pending tree displays the second frame while the active tree has the
  // last frame.
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            1u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            3u);

  // Invalidation delay is based on the duration of the second frame and the
  // delay in creating this sync tree.
  task_runner_->VerifyDelay(frames[1].duration - additional_delay);
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);

  // Activate and animate with a delay that causes us to skip another 2 frames.
  controller_->DidActivate();
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            1u);
  AdvanceNow(data.frames[1].duration + data.frames[2].duration +
             data.frames[3].duration);
  animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 1u);
  EXPECT_EQ(animated_images.count(data.paint_image_id), 1u);
  EXPECT_EQ(
      controller_->GetLastNumOfFramesSkippedForTesting(data.paint_image_id),
      2u);

  // The pending tree displays the first frame, while the active tree has the
  // second frame.
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            1u);

  // Invalidation delay is based on the duration of the first frame and the
  // initial additionaly delay.
  task_runner_->VerifyDelay(frames[0].duration - additional_delay);
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, DriversControlAnimationTicking) {
  std::vector<FrameMetadata> first_image_frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};
  DiscardableImageMap::AnimatedImageMetadata first_data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE,
      first_image_frames, kAnimationLoopOnce, 0);
  controller_->UpdateAnimatedImage(first_data);
  FakeAnimationDriver first_driver;
  controller_->RegisterAnimationDriver(first_data.paint_image_id,
                                       &first_driver);

  std::vector<FrameMetadata> second_image_frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(5)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};
  DiscardableImageMap::AnimatedImageMetadata second_data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE,
      second_image_frames, kAnimationLoopOnce, 0);
  controller_->UpdateAnimatedImage(second_data);
  FakeAnimationDriver second_driver;
  controller_->RegisterAnimationDriver(second_data.paint_image_id,
                                       &second_driver);

  // Disable animating from all drivers, no invalidation request should be made.
  first_driver.set_should_animate(false);
  second_driver.set_should_animate(false);
  controller_->UpdateStateFromDrivers(now_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  // Enable animating from the first driver, which should schedule an
  // invalidation to advance this animation.
  first_driver.set_should_animate(true);
  controller_->UpdateStateFromDrivers(now_);
  task_runner_->VerifyDelay(base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);

  // Start animating the first image.
  auto animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 0u);

  // Invalidation should be scheduled for this image.
  task_runner_->VerifyDelay(first_image_frames[0].duration);

  // Now enable animating the second image instead.
  second_driver.set_should_animate(true);
  controller_->UpdateStateFromDrivers(now_);

  // Invalidation is triggered to start with no delay since the second image has
  // not started animating yet.
  task_runner_->VerifyDelay(base::TimeDelta());

  // Disable animating all images.
  first_driver.set_should_animate(false);
  second_driver.set_should_animate(false);
  controller_->UpdateStateFromDrivers(now_);

  // Any scheduled invalidation should be cancelled.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);

  controller_->UnregisterAnimationDriver(first_data.paint_image_id,
                                         &first_driver);
  controller_->UnregisterAnimationDriver(second_data.paint_image_id,
                                         &second_driver);
}

TEST_F(ImageAnimationControllerTest, RepetitionsRequested) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE, frames,
      kAnimationLoopOnce, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Finish a single loop in the animation.
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 0);

  // No invalidation should be scheduled now, since the requested number of
  // loops have been completed.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);
  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);

  // Now with a repetition count of 5.
  data.paint_image_id = PaintImage::GetNextId();
  data.repetition_count = 5;
  controller_->UpdateAnimatedImage(data);
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);
  for (int i = 0; i < data.repetition_count; ++i) {
    LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), i);

    // Since we will be looping back to the first frame, the invalidation should
    // have the delay of the last frame. Until we reach the end of requested
    // iterations.
    if (i < data.repetition_count - 1)
      task_runner_->VerifyDelay(frames.back().duration);
    invalidation_count_ = 0;
  }

  // No invalidation should be scheduled now, since the requested number of
  // loops have been completed.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);
  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);

  // Now with kAnimationLoopInfinite.
  data.paint_image_id = PaintImage::GetNextId();
  data.repetition_count = kAnimationLoopInfinite;
  controller_->UpdateAnimatedImage(data);
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);
  for (int i = 0; i < 7; ++i) {
    LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), i);

    // Since we will be looping back to the first frame, the invalidation should
    // have the delay of the last frame. Until we reach the end of requested
    // iterations.
    if (i < data.repetition_count - 1)
      task_runner_->VerifyDelay(frames.back().duration);
    invalidation_count_ = 0;
  }

  // We still have an invalidation scheduled since the image will keep looping
  // till the drivers keep the animation active.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);
  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);

  // Now try with a kAnimationNone image, which should result in a DCHECK
  // failure.
  data.paint_image_id = PaintImage::GetNextId();
  data.repetition_count = kAnimationNone;
  EXPECT_DCHECK_DEATH(controller_->UpdateAnimatedImage(data));
}

TEST_F(ImageAnimationControllerTest, DisplayCompleteFrameOnly) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(false, base::TimeDelta::FromMilliseconds(4))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::PARTIALLY_DONE,
      frames, kAnimationLoopInfinite, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Advance until the second frame.
  LoopOnceNoDelay(data.paint_image_id, frames, 2, 0);

  // We have no invalidation scheduled since its not possible to animate the
  // image further until the second frame is completed.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  // Completely load the image but the frame is still incomplete. It should not
  // be advanced.
  data.completion_state = PaintImage::CompletionState::DONE;
  controller_->UpdateAnimatedImage(data);
  controller_->UpdateStateFromDrivers(now_);

  // No invalidation is scheduled since the last frame is still incomplete.
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, DontLoopPartiallyLoadedImages) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::PARTIALLY_DONE,
      frames, 2, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Finish the first loop.
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 0);

  // We shouldn't be looping back to the first frame until the image is known to
  // be completely loaded.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  // Now add another frame and mark the image complete. The animation should
  // advance and we should see another repetition. This verifies that we don't
  // mark loops complete on reaching the last frame until the image is
  // completely loaded and the frame count is known to be accurate.
  frames.push_back(FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)));
  data.completion_state = PaintImage::CompletionState::DONE;
  data.frames = frames;
  controller_->UpdateAnimatedImage(data);
  controller_->UpdateStateFromDrivers(now_);

  // The animation advances to the last frame. We don't have a delay since we
  // already advanced to the desired time in the loop above.
  task_runner_->VerifyDelay(base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);
  auto animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            2u);
  EXPECT_EQ(animated_images.size(), 1u);
  EXPECT_EQ(animated_images.count(data.paint_image_id), 1u);
  controller_->DidActivate();

  // Advancing the animation scheduled an invalidation for the next iteration.
  task_runner_->VerifyDelay(frames.back().duration);

  // Perform another loop in the animation.
  AdvanceNow(frames.back().duration);
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 1);

  // No invalidation should have been requested at the end of the second loop.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, DontAdvanceUntilDesiredTime) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE, frames,
      kAnimationLoopOnce, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Advance the first frame.
  task_runner_->VerifyDelay(base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);
  auto animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            0u);
  EXPECT_EQ(animated_images.size(), 0u);
  controller_->DidActivate();

  // We have an invalidation request for the second frame.
  task_runner_->VerifyDelay(frames[0].duration);

  // While there is still time for the second frame, we get a new sync tree. The
  // animation is not advanced.
  base::TimeDelta time_remaining = base::TimeDelta::FromMilliseconds(1);
  AdvanceNow(frames[0].duration - time_remaining);
  animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            0u);
  EXPECT_EQ(animated_images.size(), 0u);
  controller_->DidActivate();

  // We did not get another invalidation request because there is no change in
  // the desired time and the previous request is still pending.
  EXPECT_FALSE(task_runner_->has_delay());

  // We have a sync tree before the invalidation task could run.
  AdvanceNow(time_remaining);
  animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 1u);
  EXPECT_EQ(animated_images.count(data.paint_image_id), 1u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            1u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            0u);
  controller_->DidActivate();

  // We shouldn't have an invalidation because the animation was already
  // advanced to the last frame and the previous one should have been cancelled.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, RestartAfterSyncCutoff) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE, frames,
      kAnimationLoopOnce, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Advance the first frame.
  task_runner_->VerifyDelay(base::TimeDelta());
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);
  auto animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            0u);
  controller_->DidActivate();

  // Invalidation request for the second frame.
  task_runner_->VerifyDelay(frames[0].duration);

  // Advance the time by 10 min.
  AdvanceNow(base::TimeDelta::FromMinutes(10));

  // Animate again, it starts from the first frame. We don't see an
  // invalidation, because that's the frame we are already displaying.
  animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            0u);
  controller_->DidActivate();

  // New invalidation request since the desired invalidation time changed.
  task_runner_->VerifyDelay(frames[0].duration);
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, DontSkipLoopsToCatchUpAfterLoad) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(5))};

  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::PARTIALLY_DONE,
      frames, kAnimationLoopInfinite, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Perform the first loop while the image is partially loaded, until the third
  // frame.
  LoopOnceNoDelay(data.paint_image_id, frames, 3u, 0);

  // The invalidation has been scheduled with a delay for the third frame's
  // duration.
  task_runner_->VerifyDelay(frames[2].duration);

  // |now_| is set to the desired time for the fourth frame. Advance further so
  // we would reach the time for the second frame.
  AdvanceNow(frames[3].duration + frames[0].duration);

  // Finish the image load.
  data.completion_state = PaintImage::CompletionState::DONE;
  controller_->UpdateAnimatedImage(data);
  controller_->UpdateStateFromDrivers(now_);

  // Invalidation is scheduled immediately because we are way past the desired
  // time. We should start from the first frame after the image is loaded
  // instead of skipping frames.
  task_runner_->VerifyDelay(base::TimeDelta());
  auto animated_images = controller_->AnimateForSyncTree(now_);
  EXPECT_EQ(animated_images.size(), 1u);
  EXPECT_EQ(animated_images.count(data.paint_image_id), 1u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            2u);
  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, FinishRepetitionsDuringCatchUp) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4))};

  // The animation wants 3 loops.
  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE, frames, 3, 0);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Finish 2 loops.
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 0);
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 1);

  // now_ is set to the desired time for the first frame. Advance it so we would
  // reach way beyond the third repeition.
  AdvanceNow(base::TimeDelta::FromMinutes(1));

  // Advance the animation, we should see the last frame since the desired
  // repetition count will be reached during catch up.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 1);
  auto animated_images = controller_->AnimateForSyncTree(now_);
  // No invalidation since the active tree is already at the last frame.
  EXPECT_EQ(animated_images.size(), 0u);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::PENDING_TREE),
            frames.size() - 1);
  EXPECT_EQ(controller_->GetFrameIndexForImage(data.paint_image_id,
                                               WhichTree::ACTIVE_TREE),
            frames.size() - 1);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

TEST_F(ImageAnimationControllerTest, ResetAnimations) {
  std::vector<FrameMetadata> frames = {
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(2)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(3)),
      FrameMetadata(true, base::TimeDelta::FromMilliseconds(4))};
  DiscardableImageMap::AnimatedImageMetadata data(
      PaintImage::GetNextId(), PaintImage::CompletionState::DONE, frames, 3,
      0u);
  controller_->UpdateAnimatedImage(data);
  FakeAnimationDriver driver;
  controller_->RegisterAnimationDriver(data.paint_image_id, &driver);
  controller_->UpdateStateFromDrivers(now_);

  // Go uptill the second frame during the second iteration.
  LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), 0);
  LoopOnceNoDelay(data.paint_image_id, frames, 2u, 1);

  // Reset the animation.
  data.reset_animation_sequence_id++;
  controller_->UpdateAnimatedImage(data);
  controller_->UpdateStateFromDrivers(now_);

  // It should start again from the first frame and do 3 loops.
  for (int i = 0; i < 3; ++i) {
    bool restarting = i == 0;
    LoopOnceNoDelay(data.paint_image_id, frames, frames.size(), i, restarting);
  }

  // No invalidation should be pending.
  invalidation_count_ = 0;
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  // Same image used again in a recording. There shouldn't be an invalidation
  // since the reset sequence has already been synchronized.
  controller_->UpdateAnimatedImage(data);
  controller_->UpdateStateFromDrivers(now_);
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(invalidation_count_, 0);

  controller_->UnregisterAnimationDriver(data.paint_image_id, &driver);
}

}  // namespace cc
