// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "media/base/data_buffer.h"
#include "media/base/filters.h"
#include "media/base/limits.h"
#include "media/base/mock_callback.h"
#include "media/base/mock_filter_host.h"
#include "media/base/mock_filters.h"
#include "media/base/mock_task.h"
#include "media/base/video_frame.h"
#include "media/filters/rtc_video_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::DoAll;
using ::testing::Message;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;
using ::testing::WithArg;
using ::testing::Invoke;

namespace media {

class RTCVideoDecoderTest : public testing::Test {
 protected:
  static const int kWidth;
  static const int kHeight;
  static const char* kUrl;
  static const PipelineStatistics kStatistics;

  RTCVideoDecoderTest() {
    MediaFormat media_format;
    decoder_ = new RTCVideoDecoder(&message_loop_, kUrl);
    renderer_ = new MockVideoRenderer();

    DCHECK(decoder_);

    // Inject mocks and prepare a demuxer stream.
    decoder_->set_host(&host_);

    EXPECT_CALL(stats_callback_object_, OnStatistics(_))
        .Times(AnyNumber());
  }

  virtual ~RTCVideoDecoderTest() {
    // Finish up any remaining tasks.
    message_loop_.RunAllPending();
  }

  void InitializeDecoderSuccessfully() {
    // Test successful initialization.
    decoder_->Initialize(NULL,
                         NewExpectedCallback(), NewStatisticsCallback());
    message_loop_.RunAllPending();
  }

  StatisticsCallback* NewStatisticsCallback() {
    return NewCallback(&stats_callback_object_,
                       &MockStatisticsCallback::OnStatistics);
  }

  // Fixture members.
  scoped_refptr<RTCVideoDecoder> decoder_;
  scoped_refptr<MockVideoRenderer> renderer_;
  MockStatisticsCallback stats_callback_object_;
  StrictMock<MockFilterHost> host_;
  MessageLoop message_loop_;

 private:
  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderTest);
};

const int RTCVideoDecoderTest::kWidth = 176;
const int RTCVideoDecoderTest::kHeight = 144;
const char* RTCVideoDecoderTest::kUrl = "media://remote/0";
const PipelineStatistics RTCVideoDecoderTest::kStatistics;

TEST_F(RTCVideoDecoderTest, Initialize_Successful) {
  InitializeDecoderSuccessfully();

  // Test that the output media format is an uncompressed video surface that
  // matches the dimensions specified by rtc.
  const MediaFormat& media_format = decoder_->media_format();
  int width = 0;
  int height = 0;
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kWidth, &width));
  EXPECT_EQ(kWidth, width);
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kHeight, &height));
  EXPECT_EQ(kHeight, height);
}

TEST_F(RTCVideoDecoderTest, DoSeek) {
  const base::TimeDelta kZero;

  InitializeDecoderSuccessfully();

  decoder_->set_consume_video_frame_callback(
      base::Bind(&MockVideoRenderer::ConsumeVideoFrame,
                 base::Unretained(renderer_.get())));

  // Expect Seek and verify the results.
  EXPECT_CALL(*renderer_.get(), ConsumeVideoFrame(_))
      .Times(Limits::kMaxVideoFrames);
  decoder_->Seek(kZero, NewExpectedStatusCB(PIPELINE_OK));

  message_loop_.RunAllPending();
  EXPECT_EQ(RTCVideoDecoder::kNormal, decoder_->state_);
}

TEST_F(RTCVideoDecoderTest, DoDeliverFrame) {
  const base::TimeDelta kZero;
  EXPECT_CALL(host_, GetTime()).WillRepeatedly(Return(base::TimeDelta()));

  InitializeDecoderSuccessfully();

  // Pass the frame back to decoder
  decoder_->set_consume_video_frame_callback(
      base::Bind(&RTCVideoDecoder::ProduceVideoFrame,
                 base::Unretained(decoder_.get())));
  decoder_->Seek(kZero, NewExpectedStatusCB(PIPELINE_OK));

  decoder_->set_consume_video_frame_callback(
      base::Bind(&MockVideoRenderer::ConsumeVideoFrame,
                 base::Unretained(renderer_.get())));
  EXPECT_CALL(*renderer_.get(), ConsumeVideoFrame(_))
      .Times(Limits::kMaxVideoFrames);

  unsigned int video_frame_size = decoder_->width_*decoder_->height_*3/2;
  unsigned char* video_frame = new unsigned char[video_frame_size];

  for (size_t i = 0; i < Limits::kMaxVideoFrames; ++i) {
    decoder_->DeliverFrame(video_frame, video_frame_size);
  }
  delete [] video_frame;

  message_loop_.RunAllPending();
  EXPECT_EQ(RTCVideoDecoder::kNormal, decoder_->state_);
}

TEST_F(RTCVideoDecoderTest, DoFrameSizeChange) {
  InitializeDecoderSuccessfully();

  int new_width = kWidth * 2;
  int new_height = kHeight * 2;
  int new_number_of_streams = 0;

  EXPECT_CALL(host_,
              SetVideoSize(new_width, new_height)).WillRepeatedly(Return());

  decoder_->FrameSizeChange(new_width, new_height, new_number_of_streams);

  const MediaFormat& media_format = decoder_->media_format();
  int width = 0;
  int height = 0;
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kWidth, &width));
  EXPECT_EQ(new_width, width);
  EXPECT_TRUE(media_format.GetAsInteger(MediaFormat::kHeight, &height));
  EXPECT_EQ(new_height, height);

  message_loop_.RunAllPending();
}


}  // namespace media
