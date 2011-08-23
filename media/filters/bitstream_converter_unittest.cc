// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/bitstream_converter.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

static const char kTestFilterName[] = "test_filter";
static uint8_t kFailData[] = { 3, 2, 1 };
static uint8_t kNewBufferData[] = { 2, 1 };
static uint8_t kInPlaceData[] = { 1 };
static const int kFailSize = 3;
static const int kNewBufferSize = 2;
static const int kInPlaceSize = 1;


// Test filter function that looks for specific input data and changes it's
// behavior based on what is passed to |buf| & |buf_size|. The three behaviors
// simulated are the following:
// - Create a new output buffer. Triggered by |buf| == |kNewBufferData|.
// - Use the existing output buffer. Triggered by |buf| == |kInPlaceData|.
// - Signal an error. Triggered by |buf| == |kFailData|.
static int DoFilter(AVBitStreamFilterContext* bsfc,
                    AVCodecContext* avctx,
                    const char* args,
                    uint8_t** poutbuf,
                    int* poutbuf_size,
                    const uint8_t* buf,
                    int buf_size,
                    int keyframe) {
  if (buf_size == kNewBufferSize &&
      !memcmp(buf, kNewBufferData, kNewBufferSize)) {
    *poutbuf_size = buf_size + 1;
    *poutbuf = static_cast<uint8*>(av_malloc(*poutbuf_size));
    *poutbuf[0] = 0;
    memcpy((*poutbuf) + 1, buf, buf_size);
    return 0;
  } else if (buf_size == kInPlaceSize &&
             !memcmp(buf, kInPlaceData, kInPlaceSize)) {
    return 0;
  }

  return -1;
}

static void DoClose(AVBitStreamFilterContext* bsfc) {}

static AVBitStreamFilter g_stream_filter = {
  kTestFilterName,
  0, // Private Data Size
  DoFilter,
  DoClose,
  0, // Next filter pointer.
};

class BitstreamConverterTest : public testing::Test {
 protected:
  BitstreamConverterTest() {
    memset(&test_stream_context_, 0, sizeof(test_stream_context_));
    memset(&test_packet_, 0, sizeof(test_packet_));
    test_packet_.data = kFailData;
    test_packet_.size = kFailSize;
  }

  virtual ~BitstreamConverterTest() {
    av_free_packet(&test_packet_);
  }

  static void SetUpTestCase() {
    // Register g_stream_filter if it isn't already registered.
    if (!g_stream_filter.next)
      av_register_bitstream_filter(&g_stream_filter);
  }

  AVCodecContext test_stream_context_;
  AVPacket test_packet_;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitstreamConverterTest);
};

TEST_F(BitstreamConverterTest, InitializeFailed) {
  FFmpegBitstreamConverter converter("BAD_FILTER_NAME", &test_stream_context_);

  EXPECT_FALSE(converter.Initialize());
}

TEST_F(BitstreamConverterTest, InitializeSuccess) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);
  EXPECT_TRUE(converter.Initialize());
}

TEST_F(BitstreamConverterTest, ConvertPacket_NotInitialized) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  EXPECT_FALSE(converter.ConvertPacket(&test_packet_));
}

TEST_F(BitstreamConverterTest, ConvertPacket_FailedFilter) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  EXPECT_TRUE(converter.Initialize());

  EXPECT_FALSE(converter.ConvertPacket(&test_packet_));
}

TEST_F(BitstreamConverterTest, ConvertPacket_Success) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  EXPECT_TRUE(converter.Initialize());

  // Ensure our packet doesn't already have a destructor.
  ASSERT_TRUE(test_packet_.destruct == NULL);

  test_packet_.data = kNewBufferData;
  test_packet_.size = kNewBufferSize;

  EXPECT_TRUE(converter.ConvertPacket(&test_packet_));
  EXPECT_NE(kNewBufferData, test_packet_.data);
  EXPECT_EQ(kNewBufferSize + 1, test_packet_.size);
  EXPECT_TRUE(test_packet_.destruct != NULL);
}

TEST_F(BitstreamConverterTest, ConvertPacket_SuccessInPlace) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  EXPECT_TRUE(converter.Initialize());

  // Ensure our packet is in a sane start state.
  ASSERT_TRUE(test_packet_.destruct == NULL);
  test_packet_.data = kInPlaceData;
  test_packet_.size = kInPlaceSize;

  EXPECT_TRUE(converter.ConvertPacket(&test_packet_));
  EXPECT_EQ(kInPlaceData, test_packet_.data);
  EXPECT_EQ(kInPlaceSize, test_packet_.size);
  EXPECT_TRUE(test_packet_.destruct == NULL);
}

}  // namespace media
