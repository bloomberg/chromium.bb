// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/at_exit.h"
#include "base/command_line.h"
#include "base/test/scoped_task_environment.h"

#include "media/base/test_data_util.h"
#include "media/gpu/buildflags.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_collection.h"
#include "media/gpu/test/video_player/video_player.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_VAAPI)
#include "media/gpu/vaapi/vaapi_wrapper.h"
#endif

namespace media {
namespace test {

// TODO(dstaessens@)
// * Use a fixture here (TEST_F) with a testing environment that contains all
//   required setup code.
// * Fetch the expected number of frames from the video file's metadata.
TEST(VideoDecodeAcceleratorTest, BasicPlayTest) {
  auto renderer = FrameRendererDummy::Create();
  CHECK(renderer) << "Failed to create renderer";
  const Video* video = &kDefaultTestVideoCollection[0];
  auto tvp = VideoPlayer::Create(video, renderer.get());
  CHECK(tvp) << "Failed to create video player";

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForEvent(VideoPlayerEvent::kFlushDone));

  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kFlushDone), 1u);
  EXPECT_EQ(tvp->GetEventCount(VideoPlayerEvent::kFrameDecoded), 250u);
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);
  base::AtExitManager exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  // Setting up a task environment will create a task runner for the current
  // thread and allow posting tasks to other threads. This is required for the
  // test video player to function.
  base::test::ScopedTaskEnvironment scoped_task_environment(
      base::test::ScopedTaskEnvironment::MainThreadType::UI);

  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

#if BUILDFLAG(USE_VAAPI)
  media::VaapiWrapper::PreSandboxInitialization();
#endif

  return RUN_ALL_TESTS();
}
