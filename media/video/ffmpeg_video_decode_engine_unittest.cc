// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_task.h"
#include "media/base/pipeline.h"
#include "media/base/test_data_util.h"
#include "media/filters/ffmpeg_glue.h"
#include "media/video/ffmpeg_video_decode_engine.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

static const size_t kWidth = 320;
static const size_t kHeight = 240;
static const int kSurfaceWidth = 522;
static const int kSurfaceHeight = 288;
static const AVRational kFrameRate = { 100, 1 };

ACTION_P(DecodeComplete, decoder) {
  decoder->set_video_frame(arg0);
}

ACTION_P2(DemuxComplete, engine, buffer) {
  engine->ConsumeVideoSample(buffer);
}

ACTION_P(SaveInitializeResult, engine) {
  engine->set_video_codec_info(arg0);
}

class FFmpegVideoDecodeEngineTest
    : public testing::Test,
      public VideoDecodeEngine::EventHandler {
 public:
  FFmpegVideoDecodeEngineTest()
      : config_(kCodecVP8, kWidth, kHeight, kSurfaceWidth, kSurfaceHeight,
                kFrameRate.num, kFrameRate.den, NULL, 0) {
    CHECK(FFmpegGlue::GetInstance());

    // Setup FFmpeg structures.
    frame_buffer_.reset(new uint8[kWidth * kHeight]);

    test_engine_.reset(new FFmpegVideoDecodeEngine());

    video_frame_ = VideoFrame::CreateFrame(VideoFrame::YV12,
                                           kWidth,
                                           kHeight,
                                           kNoTimestamp,
                                           kNoTimestamp);

    ReadTestDataFile("vp8-I-frame-320x240", &i_frame_buffer_);
  }

  ~FFmpegVideoDecodeEngineTest() {
    test_engine_.reset();
  }

  void Initialize() {
    EXPECT_CALL(*this, OnInitializeComplete(_))
       .WillOnce(SaveInitializeResult(this));
    test_engine_->Initialize(MessageLoop::current(), this, NULL, config_);
    EXPECT_TRUE(info_.success);
  }

  void Decode(const scoped_refptr<Buffer>& buffer) {
    EXPECT_CALL(*this, ProduceVideoSample(_))
        .WillOnce(DemuxComplete(test_engine_.get(), buffer));
    EXPECT_CALL(*this, ConsumeVideoFrame(_, _))
        .WillOnce(DecodeComplete(this));
    test_engine_->ProduceVideoFrame(video_frame_);
  }

  // VideoDecodeEngine::EventHandler implementation.
  MOCK_METHOD2(ConsumeVideoFrame,
               void(scoped_refptr<VideoFrame> video_frame,
                    const PipelineStatistics& statistics));
  MOCK_METHOD1(ProduceVideoSample,
               void(scoped_refptr<Buffer> buffer));
  MOCK_METHOD1(OnInitializeComplete,
               void(const VideoCodecInfo& info));
  MOCK_METHOD0(OnUninitializeComplete, void());
  MOCK_METHOD0(OnFlushComplete, void());
  MOCK_METHOD0(OnSeekComplete, void());
  MOCK_METHOD0(OnError, void());

  // Used by gmock actions.
  void set_video_frame(scoped_refptr<VideoFrame> video_frame) {
    video_frame_ = video_frame;
  }

  void set_video_codec_info(const VideoCodecInfo& info) {
    info_ = info;
  }

 protected:
  VideoDecoderConfig config_;
  VideoCodecInfo info_;
  scoped_refptr<VideoFrame> video_frame_;
  scoped_ptr<FFmpegVideoDecodeEngine> test_engine_;
  scoped_array<uint8_t> frame_buffer_;
  scoped_refptr<Buffer> i_frame_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngineTest);
};

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_FindDecoderFails) {
  VideoDecoderConfig config(kUnknown, kWidth, kHeight, kSurfaceWidth,
                            kSurfaceHeight, kFrameRate.num, kFrameRate.den,
                            NULL, 0);
  // Test avcodec_find_decoder() returning NULL.
  EXPECT_CALL(*this, OnInitializeComplete(_))
     .WillOnce(SaveInitializeResult(this));
  test_engine_->Initialize(MessageLoop::current(), this, NULL, config);
  EXPECT_FALSE(info_.success);
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open() fails.
  VideoDecoderConfig config(kCodecTheora, kWidth, kHeight, kSurfaceWidth,
                            kSurfaceHeight, kFrameRate.num, kFrameRate.den,
                            NULL, 0);
  EXPECT_CALL(*this, OnInitializeComplete(_))
     .WillOnce(SaveInitializeResult(this));
  test_engine_->Initialize(MessageLoop::current(), this, NULL, config);
  EXPECT_FALSE(info_.success);
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_Normal) {
  Initialize();

  // We rely on FFmpeg for timestamp and duration reporting.
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(0);
  const base::TimeDelta kDuration = base::TimeDelta::FromMicroseconds(10000);

  // Simulate decoding a single frame.
  Decode(i_frame_buffer_);

  // |video_frame_| timestamp is 0 because we set the timestamp based off
  // the buffer timestamp.
  EXPECT_EQ(0, video_frame_->GetTimestamp().ToInternalValue());
  EXPECT_EQ(kDuration.ToInternalValue(),
            video_frame_->GetDuration().ToInternalValue());
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_0ByteFrame) {
  Initialize();

  scoped_refptr<Buffer> buffer_a = new DataBuffer(1);

  EXPECT_CALL(*this, ProduceVideoSample(_))
      .WillOnce(DemuxComplete(test_engine_.get(), buffer_a))
      .WillOnce(DemuxComplete(test_engine_.get(), i_frame_buffer_));
  EXPECT_CALL(*this, ConsumeVideoFrame(_, _))
      .WillOnce(DecodeComplete(this));
  test_engine_->ProduceVideoFrame(video_frame_);

  EXPECT_TRUE(video_frame_.get());
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_DecodeError) {
  Initialize();

  scoped_refptr<DataBuffer> buffer =
      new DataBuffer(i_frame_buffer_->GetDataSize());
  buffer->SetDataSize(i_frame_buffer_->GetDataSize());

  uint8* buf = buffer->GetWritableData();
  memcpy(buf, i_frame_buffer_->GetData(), buffer->GetDataSize());

  // Corrupt bytes by flipping bits w/ xor.
  for (size_t i = 0; i < buffer->GetDataSize(); i++)
    buf[i] ^= 0xA5;

  EXPECT_CALL(*this, ProduceVideoSample(_))
      .WillOnce(DemuxComplete(test_engine_.get(), buffer));
  EXPECT_CALL(*this, OnError());

  test_engine_->ProduceVideoFrame(video_frame_);
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_LargerWidth) {
  Initialize();

  // Decode a frame and verify the width.
  Decode(i_frame_buffer_);
  EXPECT_EQ(video_frame_->width(), kWidth);
  EXPECT_EQ(video_frame_->height(), kHeight);

  // Now decode a frame with a larger width and verify the output size didn't
  // change.
  scoped_refptr<Buffer> buffer;
  ReadTestDataFile("vp8-I-frame-640x240", &buffer);
  Decode(buffer);

  EXPECT_EQ(kWidth, video_frame_->width());
  EXPECT_EQ(kHeight, video_frame_->height());
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_SmallerWidth) {
  Initialize();

  // Decode a frame and verify the width.
  Decode(i_frame_buffer_);
  EXPECT_EQ(video_frame_->width(), kWidth);
  EXPECT_EQ(video_frame_->height(), kHeight);

  // Now decode a frame with a smaller width and verify the output size didn't
  // change.
  scoped_refptr<Buffer> buffer;
  ReadTestDataFile("vp8-I-frame-160x240", &buffer);
  Decode(buffer);
  EXPECT_EQ(video_frame_->width(), kWidth);
  EXPECT_EQ(video_frame_->height(), kHeight);
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_LargerHeight) {
  Initialize();

  // Decode a frame and verify the width.
  Decode(i_frame_buffer_);
  EXPECT_EQ(video_frame_->width(), kWidth);
  EXPECT_EQ(video_frame_->height(), kHeight);

  // Now decode a frame with a larger height and verify the output
  // size didn't change.
  scoped_refptr<Buffer> buffer;
  ReadTestDataFile("vp8-I-frame-320x480", &buffer);
  Decode(buffer);
  EXPECT_EQ(kWidth, video_frame_->width());
  EXPECT_EQ(kHeight, video_frame_->height());
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_SmallerHeight) {
  Initialize();

  // Decode a frame and verify the width.
  Decode(i_frame_buffer_);
  EXPECT_EQ(video_frame_->width(), kWidth);
  EXPECT_EQ(video_frame_->height(), kHeight);

  // Now decode a frame with a smaller height and verify the output size
  // didn't change.
  scoped_refptr<Buffer> buffer;
  ReadTestDataFile("vp8-I-frame-320x120", &buffer);
  Decode(buffer);
  EXPECT_EQ(kWidth, video_frame_->width());
  EXPECT_EQ(kHeight, video_frame_->height());
}

}  // namespace media
