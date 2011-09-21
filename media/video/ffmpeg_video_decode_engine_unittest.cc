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
using ::testing::SaveArg;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

static const gfx::Size kCodedSize(320, 240);
static const gfx::Rect kVisibleRect(320, 240);
static const gfx::Size kNaturalSize(522, 288);
static const AVRational kFrameRate = { 100, 1 };

ACTION_P2(DemuxComplete, engine, buffer) {
  engine->ConsumeVideoSample(buffer);
}

class FFmpegVideoDecodeEngineTest
    : public testing::Test,
      public VideoDecodeEngine::EventHandler {
 public:
  FFmpegVideoDecodeEngineTest()
      : config_(kCodecVP8, kCodedSize, kVisibleRect, kNaturalSize,
                kFrameRate.num, kFrameRate.den, NULL, 0) {
    CHECK(FFmpegGlue::GetInstance());

    // Setup FFmpeg structures.
    frame_buffer_.reset(new uint8[kCodedSize.GetArea()]);

    test_engine_.reset(new FFmpegVideoDecodeEngine());

    ReadTestDataFile("vp8-I-frame-320x240", &i_frame_buffer_);
    ReadTestDataFile("vp8-corrupt-I-frame", &corrupt_i_frame_buffer_);

    end_of_stream_buffer_ = new DataBuffer(0);
  }

  ~FFmpegVideoDecodeEngineTest() {
    test_engine_.reset();
  }

  void Initialize() {
    VideoCodecInfo info;
    EXPECT_CALL(*this, OnInitializeComplete(_))
        .WillOnce(SaveArg<0>(&info));
    test_engine_->Initialize(MessageLoop::current(), this, NULL, config_);
    EXPECT_TRUE(info.success);
  }

  // Decodes the single compressed frame in |buffer| and writes the
  // uncompressed output to |video_frame|. This method works with single
  // and multithreaded decoders. End of stream buffers are used to trigger
  // the frame to be returned in the multithreaded decoder case.
  void DecodeASingleFrame(const scoped_refptr<Buffer>& buffer,
                          scoped_refptr<VideoFrame>* video_frame) {
    EXPECT_CALL(*this, ProduceVideoSample(_))
        .WillOnce(DemuxComplete(test_engine_.get(), buffer))
        .WillRepeatedly(DemuxComplete(test_engine_.get(),
                                      end_of_stream_buffer_));

    EXPECT_CALL(*this, ConsumeVideoFrame(_, _))
        .WillOnce(SaveArg<0>(video_frame));
    CallProduceVideoFrame();
  }

  // Decodes |i_frame_buffer_| and then decodes the data contained in
  // the file named |test_file_name|. This function expects both buffers
  // to decode to frames that are the same size.
  void DecodeIFrameThenTestFile(const std::string& test_file_name) {
    Initialize();

    scoped_refptr<VideoFrame> video_frame_a;
    scoped_refptr<VideoFrame> video_frame_b;

    scoped_refptr<Buffer> buffer;
    ReadTestDataFile(test_file_name, &buffer);

    EXPECT_CALL(*this, ProduceVideoSample(_))
        .WillOnce(DemuxComplete(test_engine_.get(), i_frame_buffer_))
        .WillOnce(DemuxComplete(test_engine_.get(), buffer))
        .WillRepeatedly(DemuxComplete(test_engine_.get(),
                                      end_of_stream_buffer_));

    EXPECT_CALL(*this, ConsumeVideoFrame(_, _))
        .WillOnce(SaveArg<0>(&video_frame_a))
        .WillOnce(SaveArg<0>(&video_frame_b));
    CallProduceVideoFrame();
    CallProduceVideoFrame();

    size_t expected_width = static_cast<size_t>(kVisibleRect.width());
    size_t expected_height = static_cast<size_t>(kVisibleRect.height());

    EXPECT_EQ(expected_width, video_frame_a->width());
    EXPECT_EQ(expected_height, video_frame_a->height());
    EXPECT_EQ(expected_width, video_frame_b->width());
    EXPECT_EQ(expected_height, video_frame_b->height());
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

  void CallProduceVideoFrame() {
    test_engine_->ProduceVideoFrame(VideoFrame::CreateFrame(
        VideoFrame::YV12, kVisibleRect.width(), kVisibleRect.height(),
        kNoTimestamp, kNoTimestamp));
  }

 protected:
  VideoDecoderConfig config_;
  scoped_ptr<FFmpegVideoDecodeEngine> test_engine_;
  scoped_array<uint8_t> frame_buffer_;
  scoped_refptr<Buffer> i_frame_buffer_;
  scoped_refptr<Buffer> corrupt_i_frame_buffer_;
  scoped_refptr<Buffer> end_of_stream_buffer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(FFmpegVideoDecodeEngineTest);
};

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_Normal) {
  Initialize();
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_FindDecoderFails) {
  VideoDecoderConfig config(kUnknownVideoCodec, kCodedSize, kVisibleRect,
                            kNaturalSize, kFrameRate.num, kFrameRate.den,
                            NULL, 0);

  // Test avcodec_find_decoder() returning NULL.
  VideoCodecInfo info;
  EXPECT_CALL(*this, OnInitializeComplete(_))
     .WillOnce(SaveArg<0>(&info));
  test_engine_->Initialize(MessageLoop::current(), this, NULL, config);
  EXPECT_FALSE(info.success);
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_OpenDecoderFails) {
  // Specify Theora w/o extra data so that avcodec_open() fails.
  VideoDecoderConfig config(kCodecTheora, kCodedSize, kVisibleRect,
                            kNaturalSize, kFrameRate.num, kFrameRate.den,
                            NULL, 0);
  VideoCodecInfo info;
  EXPECT_CALL(*this, OnInitializeComplete(_))
     .WillOnce(SaveArg<0>(&info));
  test_engine_->Initialize(MessageLoop::current(), this, NULL, config);
  EXPECT_FALSE(info.success);
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_Normal) {
  Initialize();

  // We rely on FFmpeg for timestamp and duration reporting.
  const base::TimeDelta kTimestamp = base::TimeDelta::FromMicroseconds(0);
  const base::TimeDelta kDuration = base::TimeDelta::FromMicroseconds(10000);

  // Simulate decoding a single frame.
  scoped_refptr<VideoFrame> video_frame;
  DecodeASingleFrame(i_frame_buffer_, &video_frame);

  // |video_frame| timestamp is 0 because we set the timestamp based off
  // the buffer timestamp.
  ASSERT_TRUE(video_frame);
  EXPECT_EQ(0, video_frame->GetTimestamp().ToInternalValue());
  EXPECT_EQ(kDuration.ToInternalValue(),
            video_frame->GetDuration().ToInternalValue());
}


// Verify current behavior for 0 byte frames. FFmpeg simply ignores
// the 0 byte frames.
TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_0ByteFrame) {
  Initialize();

  scoped_refptr<DataBuffer> zero_byte_buffer = new DataBuffer(1);

  scoped_refptr<VideoFrame> video_frame_a;
  scoped_refptr<VideoFrame> video_frame_b;
  scoped_refptr<VideoFrame> video_frame_c;

  EXPECT_CALL(*this, ProduceVideoSample(_))
      .WillOnce(DemuxComplete(test_engine_.get(), i_frame_buffer_))
      .WillOnce(DemuxComplete(test_engine_.get(), zero_byte_buffer))
      .WillOnce(DemuxComplete(test_engine_.get(), i_frame_buffer_))
      .WillRepeatedly(DemuxComplete(test_engine_.get(),
                                    end_of_stream_buffer_));

  EXPECT_CALL(*this, ConsumeVideoFrame(_, _))
      .WillOnce(SaveArg<0>(&video_frame_a))
      .WillOnce(SaveArg<0>(&video_frame_b))
      .WillOnce(SaveArg<0>(&video_frame_c));
  CallProduceVideoFrame();
  CallProduceVideoFrame();
  CallProduceVideoFrame();

  EXPECT_TRUE(video_frame_a);
  EXPECT_TRUE(video_frame_b);
  EXPECT_FALSE(video_frame_c);
}


TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_DecodeError) {
  Initialize();

  EXPECT_CALL(*this, ProduceVideoSample(_))
      .WillOnce(DemuxComplete(test_engine_.get(), corrupt_i_frame_buffer_))
      .WillRepeatedly(DemuxComplete(test_engine_.get(), i_frame_buffer_));
  EXPECT_CALL(*this, OnError());

  CallProduceVideoFrame();
}

// Multi-threaded decoders have different behavior than single-threaded
// decoders at the end of the stream. Multithreaded decoders hide errors
// that happen on the last |codec_context_->thread_count| frames to avoid
// prematurely signalling EOS. This test just exposes that behavior so we can
// detect if it changes.
TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_DecodeErrorAtEndOfStream) {
  Initialize();

  EXPECT_CALL(*this, ProduceVideoSample(_))
      .WillOnce(DemuxComplete(test_engine_.get(), corrupt_i_frame_buffer_))
      .WillRepeatedly(DemuxComplete(test_engine_.get(), end_of_stream_buffer_));

  scoped_refptr<VideoFrame> video_frame;
  EXPECT_CALL(*this, ConsumeVideoFrame(_, _))
      .WillOnce(SaveArg<0>(&video_frame));
  CallProduceVideoFrame();

  EXPECT_FALSE(video_frame);
}

// Decode |i_frame_buffer_| and then a frame with a larger width and verify
// the output size didn't change.
// TODO(acolwell): Fix InvalidRead detected by Valgrind
//TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_LargerWidth) {
//  DecodeIFrameThenTestFile("vp8-I-frame-640x240");
//}

// Decode |i_frame_buffer_| and then a frame with a smaller width and verify
// the output size didn't change.
TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_SmallerWidth) {
  DecodeIFrameThenTestFile("vp8-I-frame-160x240");
}

// Decode |i_frame_buffer_| and then a frame with a larger height and verify
// the output size didn't change.
// TODO(acolwell): Fix InvalidRead detected by Valgrind
//TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_LargerHeight) {
//  DecodeIFrameThenTestFile("vp8-I-frame-320x480");
//}

// Decode |i_frame_buffer_| and then a frame with a smaller height and verify
// the output size didn't change.
TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_SmallerHeight) {
  DecodeIFrameThenTestFile("vp8-I-frame-320x120");
}

}  // namespace media
