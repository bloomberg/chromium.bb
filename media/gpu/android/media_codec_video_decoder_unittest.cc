// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/run_loop.h"
#include "media/base/android/media_codec_util.h"
#include "media/base/test_helpers.h"
#include "media/gpu/android/media_codec_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

#define SKIP_IF_MEDIA_CODEC_IS_BLACKLISTED()                                 \
  do {                                                                       \
    if (!MediaCodecUtil::IsMediaCodecAvailable()) {                          \
      DVLOG(0) << "Skipping test: MediaCodec is blacklisted on this device"; \
      return;                                                                \
    }                                                                        \
  } while (0)

namespace media {
namespace {

void InitCb(bool* result_out, bool result) {
  *result_out = result;
}

void OutputCb(const scoped_refptr<VideoFrame>& frame) {}

}  // namespace

class MediaCodecVideoDecoderTest : public testing::Test {
 public:
  ~MediaCodecVideoDecoderTest() override {}

  bool Initialize(const VideoDecoderConfig& config) {
    bool result = false;
    mcvd_.Initialize(config, false, nullptr, base::Bind(&InitCb, &result),
                     base::Bind(&OutputCb));
    base::RunLoop().RunUntilIdle();
    return result;
  }

 private:
  MediaCodecVideoDecoder mcvd_;
  base::MessageLoop message_loop_;
};

TEST_F(MediaCodecVideoDecoderTest, DestructWithoutInit) {
  // Do nothing.
}

TEST_F(MediaCodecVideoDecoderTest, UnknownCodecIsRejected) {
  SKIP_IF_MEDIA_CODEC_IS_BLACKLISTED();
  ASSERT_FALSE(Initialize(TestVideoConfig::Invalid()));
}

TEST_F(MediaCodecVideoDecoderTest, H264IsSupported) {
  SKIP_IF_MEDIA_CODEC_IS_BLACKLISTED();
  // H264 is always supported by MCVD.
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));
}

TEST_F(MediaCodecVideoDecoderTest, SmallVp8IsRejected) {
  SKIP_IF_MEDIA_CODEC_IS_BLACKLISTED();
  ASSERT_TRUE(Initialize(TestVideoConfig::NormalH264()));
}

}  // namespace media
