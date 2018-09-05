// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/android/video_frame_extractor.h"

#include <memory>

#include "base/run_loop.h"
#include "base/test/scoped_task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "media/base/test_data_util.h"
#include "media/filters/file_data_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
namespace {

struct ExtractVideoFrameResult {
  bool success = false;
  std::vector<uint8_t> encoded_frame;
  VideoDecoderConfig decoder_config;
};

void OnThumbnailGenerated(ExtractVideoFrameResult* result,
                          base::RepeatingClosure quit_closure,
                          bool success,
                          const std::vector<uint8_t>& encoded_frame,
                          const VideoDecoderConfig& decoder_config) {
  result->success = success;
  result->encoded_frame = encoded_frame;
  result->decoder_config = decoder_config;

  quit_closure.Run();
}

class VideoFrameExtractorTest : public testing::Test {
 public:
  VideoFrameExtractorTest() {}
  ~VideoFrameExtractorTest() override {}

 protected:
  void SetUp() override {
    data_source_ = std::make_unique<FileDataSource>();

    CHECK(data_source_->Initialize(GetTestDataFilePath("bear.mp4")));
    extractor_ = std::make_unique<VideoFrameExtractor>(data_source_.get());
  }

  void TearDown() override { extractor_.reset(); }

  VideoFrameExtractor* extractor() { return extractor_.get(); }

 private:
  std::unique_ptr<FileDataSource> data_source_;
  std::unique_ptr<VideoFrameExtractor> extractor_;
  base::test::ScopedTaskEnvironment scoped_task_environment_;

  DISALLOW_COPY_AND_ASSIGN(VideoFrameExtractorTest);
};

// Verifies the encoded video frame is extracted correctly.
TEST_F(VideoFrameExtractorTest, ExtractVideoFrame) {
  ExtractVideoFrameResult result;
  base::RunLoop loop;
  extractor()->Start(
      base::BindOnce(&OnThumbnailGenerated, &result, loop.QuitClosure()));
  loop.Run();
  EXPECT_TRUE(result.success);
  EXPECT_GT(result.encoded_frame.size(), 0u);
  EXPECT_EQ(result.decoder_config.codec(), VideoCodec::kCodecH264);
}

}  // namespace
}  // namespace media
