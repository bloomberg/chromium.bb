// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/test_data_util.h"
#include "media/filters/pipeline_integration_test_base.h"
#include "testing/perf/perf_test.h"

namespace media {

static const int kBenchmarkIterations = 200;

static void RunPlaybackBenchmark(const std::string& filename) {
  double time_seconds = 0.0;

  for (int i = 0; i < kBenchmarkIterations; ++i) {
    PipelineIntegrationTestBase pipeline;

    ASSERT_TRUE(pipeline.Start(GetTestDataFilePath(filename),
                               PIPELINE_OK,
                               PipelineIntegrationTestBase::kClockless));

    pipeline.Play();

    ASSERT_TRUE(pipeline.WaitUntilOnEnded());

    // Call Stop() to ensure that the rendering is complete.
    pipeline.Stop();
    time_seconds += pipeline.GetAudioTime().InSecondsF();
  }

  perf_test::PrintResult("clockless_playback",
                         "",
                         filename,
                         kBenchmarkIterations / time_seconds,
                         "runs/s",
                         true);
}

TEST(PipelineIntegrationPerfTest, AudioPlaybackBenchmark) {
  RunPlaybackBenchmark("sfx_f32le.wav");
  RunPlaybackBenchmark("sfx_s24le.wav");
  RunPlaybackBenchmark("sfx_s16le.wav");
  RunPlaybackBenchmark("sfx_u8.wav");
}

}  // namespace media
