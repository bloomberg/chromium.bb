// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_frame_file_writer.h"
#include "media/gpu/test/video_frame_validator.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/frame_renderer_thumbnail.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_collection.h"
#include "media/gpu/test/video_player/video_decoder_client.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

namespace {
// Video decoder tests usage message.
constexpr const char* usage_msg =
    "usage: video_decode_accelerator_tests [--help] [--disable_validator]\n"
    "                                      [--output_frames] [<video>]\n";
// Video decoder tests help message.
constexpr const char* help_msg =
    "Run the video decode accelerator tests on the specified video. If no\n"
    "video is specified the default \"test-25fps.h264\" video will be used.\n"
    "\nThe following arguments are supported:\n"
    "  --disable_validator  disable frame validation, useful on old\n"
    "                       platforms that don't support import mode.\n"
    "  --output_frames      write all decoded video frames to the\n"
    "                       \"video_frames\" folder.\n"
    "  --help               display this help and exit.\n";

media::test::VideoPlayerTestEnvironment* g_env;

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  std::unique_ptr<VideoPlayer> CreateVideoPlayer(
      const Video* video,
      const VideoDecoderClientConfig& config = VideoDecoderClientConfig(),
      std::unique_ptr<FrameRenderer> frame_renderer =
          FrameRendererDummy::Create()) {
    LOG_ASSERT(video);
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;

    // Validate decoded video frames.
    if (g_env->enable_validator_) {
      frame_processors.push_back(
          media::test::VideoFrameValidator::Create(video->FrameChecksums()));
    }

    // Write decoded video frames to the 'video_frames/<test_name/>' folder.
    if (g_env->output_frames_) {
      const ::testing::TestInfo* const test_info =
          ::testing::UnitTest::GetInstance()->current_test_info();
      base::FilePath output_folder =
          base::FilePath("video_frames")
              .Append(base::FilePath(test_info->name()));
      frame_processors.push_back(VideoFrameFileWriter::Create(output_folder));
    }

    return VideoPlayer::Create(video, std::move(frame_renderer),
                               std::move(frame_processors), config);
  }
};

}  // namespace

// Play video from start to end. Wait for the kFlushDone event at the end of the
// stream, that notifies us all frames have been decoded.
TEST_F(VideoDecoderTest, FlushAtEndOfStream) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Flush the decoder immediately after initialization.
TEST_F(VideoDecoderTest, FlushAfterInitialize) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->Flush();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Flush the decoder immediately after doing a mid-stream reset, without waiting
// for a kResetDone event.
TEST_F(VideoDecoderTest, FlushBeforeResetDone) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFrameDecoded(g_env->video_->NumFrames() / 2));
  tvp->Reset();
  tvp->Flush();
  EXPECT_TRUE(tvp->WaitForResetDone());
  EXPECT_TRUE(tvp->WaitForFlushDone());

  // As flush doesn't cancel reset, we should have received a single kResetDone
  // and kFlushDone event. We didn't decode the entire video, but more frames
  // might be decoded by the time we called reset, so we can only check whether
  // the decoded frame count is <= the total number of frames.
  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_LE(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately after initialization.
TEST_F(VideoDecoderTest, ResetAfterInitialize) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the middle of the stream is reached.
TEST_F(VideoDecoderTest, ResetMidStream) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFrameDecoded(g_env->video_->NumFrames() / 2));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder when the end of the stream is reached.
TEST_F(VideoDecoderTest, ResetEndOfStream) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 2u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames() * 2);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when the end-of-stream flush starts, without
// waiting for a kFlushDone event.
TEST_F(VideoDecoderTest, ResetBeforeFlushDone) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  // Reset when a kFlushing event is received.
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());

  // Reset will cause the decoder to drop everything it's doing, including the
  // ongoing flush operation. However the flush might have been completed
  // already by the time reset is called. So depending on the timing of the
  // calls we should see 0 or 1 flushes, and the last few video frames might
  // have been dropped.
  EXPECT_LE(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_LE(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Reset the decoder immediately when encountering the first config info in a
// H.264 video stream. After resetting the video is played until the end.
TEST_F(VideoDecoderTest, ResetAfterFirstConfigInfo) {
  // This test is only relevant for H.264 video streams.
  if (g_env->video_->Profile() < H264PROFILE_MIN ||
      g_env->video_->Profile() > H264PROFILE_MAX)
    GTEST_SKIP();

  auto tvp = CreateVideoPlayer(g_env->video_);

  tvp->PlayUntil(VideoPlayerEvent::kConfigInfo);
  EXPECT_TRUE(tvp->WaitForEvent(VideoPlayerEvent::kConfigInfo));
  tvp->Reset();
  EXPECT_TRUE(tvp->WaitForResetDone());
  size_t numFramesDecoded = tvp->GetFrameDecodedCount();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetResetDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(),
            numFramesDecoded + g_env->video_->NumFrames());
  EXPECT_GE(tvp->GetEventCount(VideoPlayerEvent::kConfigInfo), 1u);
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play video from start to end. Multiple buffer decodes will be queued in the
// decoder, without waiting for the result of the previous decode requests.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleOutstandingDecodes) {
  VideoDecoderClientConfig config;
  config.max_outstanding_decode_requests = 5;
  auto tvp = CreateVideoPlayer(g_env->video_, config);

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
}

// Play multiple videos simultaneously from start to finish.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_MultipleConcurrentDecodes) {
  // The minimal number of concurrent decoders we expect to be supported.
  constexpr size_t kMinSupportedConcurrentDecoders = 3;

  std::vector<std::unique_ptr<VideoPlayer>> tvps(
      kMinSupportedConcurrentDecoders);
  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i] = CreateVideoPlayer(g_env->video_);

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i)
    tvps[i]->Play();

  for (size_t i = 0; i < kMinSupportedConcurrentDecoders; ++i) {
    EXPECT_TRUE(tvps[i]->WaitForFlushDone());
    EXPECT_EQ(tvps[i]->GetFlushDoneCount(), 1u);
    EXPECT_EQ(tvps[i]->GetFrameDecodedCount(), g_env->video_->NumFrames());
    EXPECT_TRUE(tvps[i]->WaitForFrameProcessors());
  }
}

// Play a video from start to finish. Thumbnails of the decoded frames will be
// rendered into a image, whose checksum is compared to a golden value. This
// test is only needed on older platforms that don't support the video frame
// validator, which requires direct access to the video frame's memory. This
// test is only ran when --disable_validator is specified, and will be
// deprecated in the future.
TEST_F(VideoDecoderTest, FlushAtEndOfStream_RenderThumbnails) {
  if (g_env->enable_validator_)
    GTEST_SKIP();

  VideoDecoderClientConfig config;
  config.allocation_mode = AllocationMode::kAllocate;
  auto tvp = CreateVideoPlayer(
      g_env->video_, config,
      FrameRendererThumbnail::Create(g_env->video_->FilePath()));

  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
  EXPECT_TRUE(tvp->WaitForFrameProcessors());
  EXPECT_TRUE(static_cast<FrameRendererThumbnail*>(tvp->GetFrameRenderer())
                  ->ValidateThumbnail());
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);
  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();

  // Print the help message if requested. This needs to be done before
  // initializing gtest, to overwrite the default gtest help message.
  LOG_ASSERT(cmd_line);
  if (cmd_line->HasSwitch("help")) {
    std::cout << media::test::usage_msg << media::test::help_msg;
    return 0;
  }

  testing::InitGoogleTest(&argc, argv);

  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

  // Check if a video was specified on the command line.
  std::unique_ptr<media::test::Video> video;
  base::CommandLine::StringVector args = cmd_line->GetArgs();
  if (args.size() >= 1) {
    video = std::make_unique<media::test::Video>(base::FilePath(args[0]));
    if (!video->Load()) {
      LOG(ERROR) << "Failed to load " << args[0];
      return 0;
    }
  }

  // Set up our test environment.
  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new media::test::VideoPlayerTestEnvironment(
              video ? video.get()
                    : &media::test::kDefaultTestVideoCollection[0])));

  // Parse command line arguments.
  base::CommandLine::SwitchMap switches = cmd_line->GetSwitches();
  for (base::CommandLine::SwitchMap::const_iterator it = switches.begin();
       it != switches.end(); ++it) {
    if (it->first == "disable_validator") {
      media::test::g_env->enable_validator_ = false;
    } else if (it->first == "output_frames") {
      media::test::g_env->output_frames_ = true;
    } else if (it->first.find("gtest_") == 0 || it->first == "v" ||
               it->first == "vmodule") {
      // Ignore
    } else {
      std::cout << "unknown option: --" << it->first << "\n"
                << media::test::usage_msg;
      return 0;
    }
  }

  return RUN_ALL_TESTS();
}
