// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_

#include <memory>
#include "base/at_exit.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_timeouts.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video_player/video.h"
#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif
#include "testing/gtest/include/gtest/gtest.h"
#if defined(USE_OZONE)
#include "ui/ozone/public/ozone_gpu_test_helper.h"
#include "ui/ozone/public/ozone_platform.h"
#endif

namespace media {
namespace test {

// Test environment for video decode tests. Performs setup and teardown once for
// the entire test run.
class VideoPlayerTestEnvironment : public ::testing::Environment {
 public:
  explicit VideoPlayerTestEnvironment(const Video* video) : video_(video) {}

  // Set up the video decode test environment, only called once.
  void SetUp() override;
  // Tear down the video decode test environment, only called once.
  void TearDown() override;

  std::unique_ptr<base::test::ScopedTaskEnvironment> task_environment_;
  const Video* const video_;

  // An exit manager is required to run callbacks on shutdown.
  base::AtExitManager at_exit_manager;

#if defined(USE_OZONE)
  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper_;
#endif
};

void VideoPlayerTestEnvironment::SetUp() {
  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for the
  // test video player to function correctly.
  TestTimeouts::Initialize();
  task_environment_ = std::make_unique<base::test::ScopedTaskEnvironment>(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  // Perform all static initialization that is required when running video
  // decoders in a test environment.
#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

#if defined(USE_OZONE)
  // Initialize Ozone. This is necessary to gain access to the GPU for hardware
  // video decode acceleration.
  LOG(WARNING) << "Initializing Ozone Platform...\n"
                  "If this hangs indefinitely please call 'stop ui' first!";
  ui::OzonePlatform::InitParams params = {.single_process = false};
  ui::OzonePlatform::InitializeForUI(params);
  ui::OzonePlatform::InitializeForGPU(params);
  ui::OzonePlatform::GetInstance()->AfterSandboxEntry();

  // Initialize the Ozone GPU helper. If this is not done an error will occur:
  // "Check failed: drm. No devices available for buffer allocation."
  // Note: If a task environment is not set up initialization will hang
  // indefinitely here.
  gpu_helper_.reset(new ui::OzoneGpuTestHelper());
  gpu_helper_->Initialize(base::ThreadTaskRunnerHandle::Get());
#endif
}

void VideoPlayerTestEnvironment::TearDown() {
  task_environment_.reset();
}

}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
