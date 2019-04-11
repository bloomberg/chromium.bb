// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
#define MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_

#include <memory>

#include "base/at_exit.h"
#include "base/test/scoped_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(USE_OZONE)
namespace ui {
class OzoneGpuTestHelper;
}
#endif

namespace media {
namespace test {

class Video;

// Test environment for video decode tests. Performs setup and teardown once for
// the entire test run.
class VideoPlayerTestEnvironment : public ::testing::Environment {
 public:
  static VideoPlayerTestEnvironment* Create(
      const base::FilePath& video_path,
      const base::FilePath& video_metadata_path,
      bool enable_validator,
      bool output_frames,
      bool use_vd);
  ~VideoPlayerTestEnvironment() override;

  // Set up the video decode test environment, only called once.
  void SetUp() override;
  // Tear down the video decode test environment, only called once.
  void TearDown() override;

  // Get the video the tests will be ran on.
  const media::test::Video* Video() const;
  // Check whether frame validation is enabled.
  bool IsValidatorEnabled() const;
  // Check whether outputting frames is enabled.
  bool IsFramesOutputEnabled() const;
  // Check whether we should use VD-based video decoders instead of VDA-based.
  bool UseVD() const;

  // Get the name of the current test.
  base::FilePath::StringType GetTestName() const;

 private:
  VideoPlayerTestEnvironment(std::unique_ptr<media::test::Video> video,
                             bool enable_validator,
                             bool output_frames,
                             bool use_vd);

  std::unique_ptr<base::test::ScopedTaskEnvironment> task_environment_;
  const std::unique_ptr<media::test::Video> video_;
  const bool enable_validator_;
  const bool output_frames_;
  const bool use_vd_;

  // An exit manager is required to run callbacks on shutdown.
  base::AtExitManager at_exit_manager;

#if defined(USE_OZONE)
  std::unique_ptr<ui::OzoneGpuTestHelper> gpu_helper_;
#endif
};
}  // namespace test
}  // namespace media

#endif  // MEDIA_GPU_TEST_VIDEO_PLAYER_VIDEO_PLAYER_TEST_ENVIRONMENT_H_
