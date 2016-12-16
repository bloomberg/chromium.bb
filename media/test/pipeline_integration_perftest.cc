// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"
#include "media/media_features.h"
#include "media/test/pipeline_integration_test_base.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterationsAudio = 200;
static const int kBenchmarkIterationsVideo = 20;

static void RunPlaybackBenchmark(const std::string& filename,
                                 const std::string& name,
                                 int iterations,
                                 bool audio_only) {
  double time_seconds = 0.0;

  for (int i = 0; i < iterations; ++i) {
    PipelineIntegrationTestBase pipeline;

    ASSERT_EQ(
        PIPELINE_OK,
        pipeline.Start(filename, PipelineIntegrationTestBase::kClockless));

    base::TimeTicks start = base::TimeTicks::Now();
    pipeline.Play();

    ASSERT_TRUE(pipeline.WaitUntilOnEnded());

    // Call Stop() to ensure that the rendering is complete.
    pipeline.Stop();

    if (audio_only) {
      time_seconds += pipeline.GetAudioTime().InSecondsF();
    } else {
      time_seconds += (base::TimeTicks::Now() - start).InSecondsF();
    }
  }

  perf_test::PrintResult(name,
                         "",
                         filename,
                         iterations / time_seconds,
                         "runs/s",
                         true);
}

static void RunVideoPlaybackBenchmark(const std::string& filename,
                                      const std::string name) {
  RunPlaybackBenchmark(filename, name, kBenchmarkIterationsVideo, false);
}

static void RunAudioPlaybackBenchmark(const std::string& filename,
                                      const std::string& name) {
  RunPlaybackBenchmark(filename, name, kBenchmarkIterationsAudio, true);
}

TEST(PipelineIntegrationPerfTest, AudioPlaybackBenchmark) {
  RunAudioPlaybackBenchmark("sfx_f32le.wav", "clockless_playback");
  RunAudioPlaybackBenchmark("sfx_s24le.wav", "clockless_playback");
  RunAudioPlaybackBenchmark("sfx_s16le.wav", "clockless_playback");
  RunAudioPlaybackBenchmark("sfx_u8.wav", "clockless_playback");
#if BUILDFLAG(USE_PROPRIETARY_CODECS)
  RunAudioPlaybackBenchmark("sfx.mp3", "clockless_playback");
#endif
}

TEST(PipelineIntegrationPerfTest, VP8PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear_silent.webm", "clockless_video_playback_vp8");
}

TEST(PipelineIntegrationPerfTest, VP9PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear-vp9.webm", "clockless_video_playback_vp9");
}

// Android doesn't build Theora support.
#if !defined(OS_ANDROID)
TEST(PipelineIntegrationPerfTest, TheoraPlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear_silent.ogv",
                            "clockless_video_playback_theora");
}
#endif

// PipelineIntegrationTests can't play h264 content.
#if BUILDFLAG(USE_PROPRIETARY_CODECS) && !defined(OS_ANDROID)
TEST(PipelineIntegrationPerfTest, MP4PlaybackBenchmark) {
  RunVideoPlaybackBenchmark("bear_silent.mp4", "clockless_video_playback_mp4");
}
#endif

}  // namespace media
