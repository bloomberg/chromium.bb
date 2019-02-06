// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/strings/stringprintf.h"
#include "media/base/test_data_util.h"
#include "media/gpu/test/video_player/frame_renderer_dummy.h"
#include "media/gpu/test/video_player/video.h"
#include "media/gpu/test/video_player/video_collection.h"
#include "media/gpu/test/video_player/video_decoder_client.h"
#include "media/gpu/test/video_player/video_player.h"
#include "media/gpu/test/video_player/video_player_test_environment.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace test {

// Default output folder used to store performance metrics.
constexpr const base::FilePath::CharType* kDefaultOutputFolder =
    FILE_PATH_LITERAL("perf_metrics");

// TODO(dstaessens@) Expand with more meaningful metrics.
struct PerformanceMetrics {
  // Total measurement duration.
  base::TimeDelta total_duration_;
  // The number of frames decoded.
  size_t frame_decoded_count_ = 0;
  // The overall number of frames decoded per second.
  double frames_per_second_ = 0.0;
  // The average time it took to decode a frame.
  double avg_frame_decode_time_ms_ = 0.0;
};

// The performance evaluator can be plugged into the video player to collect
// various performance metrics.
// TODO(dstaessens@) Check and post warning when CPU frequency scaling is
// enabled as this affects test results.
class PerformanceEvaluator : public VideoFrameProcessor {
 public:
  // Interface VideoFrameProcessor
  void ProcessVideoFrame(scoped_refptr<const VideoFrame> video_frame,
                         size_t frame_index) override;
  bool WaitUntilDone() override { return true; }

  // Start/Stop collecting performance metrics.
  void StartMeasuring();
  void StopMeasuring();

  // Write the collected performance metrics to file.
  void WriteMetricsToFile() const;

 private:
  // Start/end time of the measurement period.
  base::TimeTicks start_time_;
  base::TimeTicks end_time_;
  // Time at which the previous frame was decoded.
  base::TimeTicks prev_frame_decoded_time_;

  // List of all frame decode times.
  std::vector<double> frame_decode_times_;
  // Collection of various performance metrics.
  PerformanceMetrics perf_metrics_;
};

void PerformanceEvaluator::ProcessVideoFrame(
    scoped_refptr<const VideoFrame> video_frame,
    size_t frame_index) {
  base::TimeTicks now = base::TimeTicks::Now();
  frame_decode_times_.push_back(
      (now - prev_frame_decoded_time_).InMillisecondsF());
  prev_frame_decoded_time_ = now;
  perf_metrics_.frame_decoded_count_++;
}

void PerformanceEvaluator::StartMeasuring() {
  start_time_ = base::TimeTicks::Now();
  prev_frame_decoded_time_ = start_time_;
}

void PerformanceEvaluator::StopMeasuring() {
  end_time_ = base::TimeTicks::Now();
  perf_metrics_.total_duration_ = end_time_ - start_time_;
  perf_metrics_.frames_per_second_ = perf_metrics_.frame_decoded_count_ /
                                     perf_metrics_.total_duration_.InSecondsF();
  perf_metrics_.avg_frame_decode_time_ms_ =
      perf_metrics_.total_duration_.InMillisecondsF() /
      perf_metrics_.frame_decoded_count_;

  VLOG(0) << "Number of frames decoded: " << perf_metrics_.frame_decoded_count_;
  VLOG(0) << "Total duration:           "
          << perf_metrics_.total_duration_.InMillisecondsF() << "ms";
  VLOG(0) << "FPS:                      " << perf_metrics_.frames_per_second_;
  VLOG(0) << "Avg. frame decode time:   "
          << perf_metrics_.avg_frame_decode_time_ms_ << "ms";
}

void PerformanceEvaluator::WriteMetricsToFile() const {
  const ::testing::TestInfo* const test_info =
      ::testing::UnitTest::GetInstance()->current_test_info();
  std::string str = base::StringPrintf(
      "Number of frames decoded: %zu\nTotal duration: %fms\nFPS: %f\nAvg. "
      "frame decode time: %fms\n",
      perf_metrics_.frame_decoded_count_,
      perf_metrics_.total_duration_.InMillisecondsF(),
      perf_metrics_.frames_per_second_,
      perf_metrics_.avg_frame_decode_time_ms_);

  // Write performance metrics to file.
  base::FilePath output_folder_path = base::FilePath(kDefaultOutputFolder);
  if (!DirectoryExists(output_folder_path))
    base::CreateDirectory(output_folder_path);
  base::FilePath output_file_path = output_folder_path.Append(
      base::FilePath(test_info->name()).AddExtension(".txt"));
  base::File output_file(
      base::FilePath(output_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  output_file.WriteAtCurrentPos(str.data(), str.length());
  VLOG(0) << "Wrote performance metrics to: " << output_file_path;

  // Write frame decode times to file.
  base::FilePath decode_times_file_path =
      output_file_path.InsertBeforeExtension(".frame_times");
  base::File decode_times_output_file(
      base::FilePath(decode_times_file_path),
      base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
  for (double frame_decoded_time : frame_decode_times_) {
    std::string decode_time_str =
        base::StringPrintf("%f\n", frame_decoded_time);
    decode_times_output_file.WriteAtCurrentPos(decode_time_str.data(),
                                               decode_time_str.length());
  }
  VLOG(0) << "Wrote frame decode times to: " << decode_times_file_path;
}

namespace {

media::test::VideoPlayerTestEnvironment* g_env;

// Video decode test class. Performs setup and teardown for each single test.
class VideoDecoderTest : public ::testing::Test {
 public:
  std::unique_ptr<VideoPlayer> CreateVideoPlayer(const Video* video) {
    LOG_ASSERT(video);
    std::vector<std::unique_ptr<VideoFrameProcessor>> frame_processors;
    auto performance_evaluator = std::make_unique<PerformanceEvaluator>();
    performance_evaluator_ = performance_evaluator.get();
    frame_processors.push_back(std::move(performance_evaluator));
    return VideoPlayer::Create(video, FrameRendererDummy::Create(),
                               std::move(frame_processors),
                               VideoDecoderClientConfig());
  }

  PerformanceEvaluator* performance_evaluator_;
};

}  // namespace

// Play video from start to end while measuring performance.
// TODO(dstaessens@) Add a test to measure capped decode performance, measuring
// the number of frames dropped.
TEST_F(VideoDecoderTest, MeasureUncappedPerfomance) {
  auto tvp = CreateVideoPlayer(g_env->video_);

  performance_evaluator_->StartMeasuring();
  tvp->Play();
  EXPECT_TRUE(tvp->WaitForFlushDone());
  performance_evaluator_->StopMeasuring();
  performance_evaluator_->WriteMetricsToFile();

  EXPECT_EQ(tvp->GetFlushDoneCount(), 1u);
  EXPECT_EQ(tvp->GetFrameDecodedCount(), g_env->video_->NumFrames());
}

}  // namespace test
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);

  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  // Set the default test data path.
  media::test::Video::SetTestDataPath(media::GetTestDataPath());

  // Set up our test environment
  media::test::g_env = static_cast<media::test::VideoPlayerTestEnvironment*>(
      testing::AddGlobalTestEnvironment(
          new media::test::VideoPlayerTestEnvironment(
              &media::test::kDefaultTestVideoCollection[0])));

  return RUN_ALL_TESTS();
}
