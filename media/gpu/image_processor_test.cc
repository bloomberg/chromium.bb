// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <tuple>

#include "base/files/file_path.h"
#include "base/md5.h"
#include "base/test/launcher/unit_test_launcher.h"
#include "base/test/test_suite.h"
#include "build/build_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "media/gpu/image_processor.h"
#include "media/gpu/test/image_processor/image_processor_client.h"
#include "media/gpu/test/video_image_info.h"
#include "mojo/core/embedder/embedder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/geometry/size.h"

namespace media {
namespace {

// I420 formatted 320x192 video frame. (bear)
// TODO(crbug.com/917951): Dynamically load this info from json file.
constexpr test::VideoImageInfo kI420Image(
    FILE_PATH_LITERAL("bear_320x192.i420.yuv"),
    "962820755c74b28f9385fd67219cc04a",
    PIXEL_FORMAT_I420,
    gfx::Size(320, 192));

// NV12 formatted 320x192 video frame. (bear)
// TODO(crbug.com/917951): Dynamically load this info from json file.
constexpr test::VideoImageInfo kNV12Image(
    FILE_PATH_LITERAL("bear_320x192.i420.nv12.yuv"),
    "ce21986434743d3671056719136d46ff",
    PIXEL_FORMAT_NV12,
    gfx::Size(320, 192));

class ImageProcessorSimpleParamTest
    : public ::testing::Test,
      public ::testing::WithParamInterface<
          std::tuple<test::VideoImageInfo, test::VideoImageInfo>> {
 public:
  // TODO(crbug.com/917951): Initialize Ozone once.
  void SetUp() override {}
  void TearDown() override {}

  std::unique_ptr<test::ImageProcessorClient> CreateImageProcessorClient(
      const test::VideoImageInfo& input_image_info,
      const test::VideoImageInfo& output_image_info) {
    // TODO(crbug.com/917951): Pass VideoFrameProcessor.
    auto input_config_layout = input_image_info.VideoFrameLayout();
    auto output_config_layout = output_image_info.VideoFrameLayout();
    LOG_ASSERT(input_config_layout);
    LOG_ASSERT(output_config_layout);
    ImageProcessor::PortConfig input_config(*input_config_layout,
                                            input_image_info.visible_size,
                                            {VideoFrame::STORAGE_OWNED_MEMORY});
    ImageProcessor::PortConfig output_config(
        *output_config_layout, output_image_info.visible_size,
        {VideoFrame::STORAGE_OWNED_MEMORY});
    // TODO(crbug.com/917951): Select more appropriate number of buffers.
    constexpr size_t kNumBuffers = 1;
    auto ip_client = test::ImageProcessorClient::Create(
        input_config, output_config, kNumBuffers, true);
    LOG_ASSERT(ip_client) << "Failed to create ImageProcessorClient";
    return ip_client;
  }
};

TEST_P(ImageProcessorSimpleParamTest, ConvertOneTimeFromMemToMem) {
  test::VideoImageInfo input_image_info = std::get<0>(GetParam());
  test::VideoImageInfo output_image_info = std::get<1>(GetParam());
  auto ip_client =
      CreateImageProcessorClient(input_image_info, output_image_info);

  ip_client->Process(input_image_info, output_image_info);
  EXPECT_TRUE(ip_client->WaitUntilNumImageProcessed(1u));
  EXPECT_EQ(ip_client->GetErrorCount(), 0u);
  EXPECT_EQ(ip_client->GetNumOfProcessedImages(), 1u);

  // TODO(crbug.com/917951): Replace this checker with VideoFrameProcessor
  // interface and get results by ImageProcessorClient function like
  // ImageProcessorClient::GetProcessResults().
  const auto output_frames = ip_client->GetProcessedImages();
  ASSERT_EQ(output_frames.size(), 1u);
  auto processed_frame = output_frames[0];
  ASSERT_TRUE(processed_frame->IsMappable());
  base::MD5Context context;
  base::MD5Init(&context);
  VideoFrame::HashFrameForTesting(&context, processed_frame);
  base::MD5Digest digest;
  base::MD5Final(&digest, &context);
  std::string expected_md5 = output_image_info.md5sum;
  std::string computed_md5 = MD5DigestToBase16(digest);
  EXPECT_EQ(expected_md5, computed_md5);
};

// I420->NV12
INSTANTIATE_TEST_CASE_P(ConvertI420ToNV12,
                        ImageProcessorSimpleParamTest,
                        ::testing::Values(std::make_tuple(kI420Image,
                                                          kNV12Image)));

#if defined(OS_CHROMEOS)
// TODO(hiroh): Add more tests.
// MEM->DMABUF (V4L2VideoEncodeAccelerator),
// DMABUF->DMABUF (GpuArcVideoEncodeAccelerator),
#endif

}  // namespace
}  // namespace media

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  base::CommandLine::Init(argc, argv);
  // Using shared memory requires mojo to be initialized (crbug.com/849207).
  mojo::core::Init();
  base::ShadowingAtExitManager at_exit_manager;

  // Needed to enable DVLOG through --vmodule.
  logging::LoggingSettings settings;
  settings.logging_dest = logging::LOG_TO_SYSTEM_DEBUG_LOG;
  LOG_ASSERT(logging::InitLogging(settings));

  return RUN_ALL_TESTS();
}
