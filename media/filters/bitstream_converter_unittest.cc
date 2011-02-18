// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "media/base/mock_ffmpeg.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/bitstream_converter.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::DoAll;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrEq;
using ::testing::StrictMock;
using ::testing::_;

namespace media {

class BitstreamConverterTest : public testing::Test {
 protected:
  BitstreamConverterTest() {
    memset(&test_stream_context_, 0, sizeof(test_stream_context_));
    memset(&test_filter_, 0, sizeof(test_filter_));
    memset(&test_packet_, 0, sizeof(test_packet_));
    test_packet_.data = kData1;
    test_packet_.size = kTestSize1;
  }

  virtual ~BitstreamConverterTest() {}

  AVCodecContext test_stream_context_;
  AVBitStreamFilterContext test_filter_;
  AVPacket test_packet_;

  StrictMock<MockFFmpeg> mock_ffmpeg_;

  static const char kTestFilterName[];
  static uint8_t kData1[];
  static uint8_t kData2[];
  static const int kTestSize1;
  static const int kTestSize2;

 private:
  DISALLOW_COPY_AND_ASSIGN(BitstreamConverterTest);
};

const char BitstreamConverterTest::kTestFilterName[] = "test_filter";
uint8_t BitstreamConverterTest::kData1[] = { 1 };
uint8_t BitstreamConverterTest::kData2[] = { 2 };
const int BitstreamConverterTest::kTestSize1 = 1;
const int BitstreamConverterTest::kTestSize2 = 2;

TEST_F(BitstreamConverterTest, Initialize) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  // Test Initialize returns false on a bad initialization, and cleanup is not
  // done.
  EXPECT_CALL(mock_ffmpeg_, AVBitstreamFilterInit(StrEq(kTestFilterName)))
      .WillOnce(ReturnNull());
  EXPECT_FALSE(converter.Initialize());

  EXPECT_TRUE(Mock::VerifyAndClearExpectations(&mock_ffmpeg_));

  // Test Initialize returns true on successful initialization, and cleanup is
  // done. The cleanup will be activated when the converter object goes out of
  // scope.
  EXPECT_CALL(mock_ffmpeg_, AVBitstreamFilterInit(StrEq(kTestFilterName)))
      .WillOnce(Return(&test_filter_));
  EXPECT_CALL(mock_ffmpeg_, AVBitstreamFilterClose(&test_filter_));
  EXPECT_TRUE(converter.Initialize());
}

TEST_F(BitstreamConverterTest, ConvertPacket_NotInitialized) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  EXPECT_FALSE(converter.ConvertPacket(&test_packet_));
}

TEST_F(BitstreamConverterTest, ConvertPacket_FailedFilter) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  // Inject mock filter instance.
  converter.stream_filter_ = &test_filter_;

  // Simulate a successful filter call, that allocates a new data buffer.
  EXPECT_CALL(mock_ffmpeg_,
              AVBitstreamFilterFilter(&test_filter_, &test_stream_context_,
                                      NULL, _, _,
                                      test_packet_.data, test_packet_.size, _))
      .WillOnce(Return(AVERROR_UNKNOWN));

  EXPECT_FALSE(converter.ConvertPacket(&test_packet_));

  // Uninject mock filter instance to avoid cleanup code on destruction of
  // converter.
  converter.stream_filter_ = NULL;
}

TEST_F(BitstreamConverterTest, ConvertPacket_Success) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  // Inject mock filter instance.
  converter.stream_filter_ = &test_filter_;

  // Ensure our packet doesn't already have a destructor.
  ASSERT_TRUE(test_packet_.destruct == NULL);

  // Simulate a successful filter call, that allocates a new data buffer.
  EXPECT_CALL(mock_ffmpeg_,
              AVBitstreamFilterFilter(&test_filter_, &test_stream_context_,
                                      NULL, _, _,
                                      test_packet_.data, test_packet_.size, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(&kData2[0]),
                      SetArgumentPointee<4>(kTestSize2),
                      Return(0)));
  EXPECT_CALL(mock_ffmpeg_, AVFreePacket(&test_packet_));

  EXPECT_TRUE(converter.ConvertPacket(&test_packet_));
  EXPECT_EQ(kData2, test_packet_.data);
  EXPECT_EQ(kTestSize2, test_packet_.size);
  EXPECT_TRUE(test_packet_.destruct != NULL);

  // Uninject mock filter instance to avoid cleanup code on destruction of
  // converter.
  converter.stream_filter_ = NULL;
}

TEST_F(BitstreamConverterTest, ConvertPacket_SuccessInPlace) {
  FFmpegBitstreamConverter converter(kTestFilterName, &test_stream_context_);

  // Inject mock filter instance.
  converter.stream_filter_ = &test_filter_;

  // Ensure our packet is in a sane start state.
  ASSERT_TRUE(test_packet_.destruct == NULL);
  ASSERT_EQ(kData1, test_packet_.data);
  ASSERT_EQ(kTestSize1, test_packet_.size);

  // Simulate a successful filter call, that reuses the input buffer.  We should
  // not free the packet here or alter the packet's destructor.
  EXPECT_CALL(mock_ffmpeg_,
              AVBitstreamFilterFilter(&test_filter_, &test_stream_context_,
                                      NULL, _, _,
                                      test_packet_.data, test_packet_.size, _))
      .WillOnce(DoAll(SetArgumentPointee<3>(test_packet_.data),
                      SetArgumentPointee<4>(test_packet_.size),
                      Return(0)));

  EXPECT_TRUE(converter.ConvertPacket(&test_packet_));
  EXPECT_EQ(kData1, test_packet_.data);
  EXPECT_EQ(kTestSize1, test_packet_.size);
  EXPECT_TRUE(test_packet_.destruct == NULL);

  // Uninject mock filter instance to avoid cleanup code on destruction of
  // converter.
  converter.stream_filter_ = NULL;
}

}  // namespace media
