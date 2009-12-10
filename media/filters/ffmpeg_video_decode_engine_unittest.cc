// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "media/base/data_buffer.h"
#include "media/base/mock_ffmpeg.h"
#include "media/base/mock_task.h"
#include "media/filters/ffmpeg_video_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::DoAll;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::SetArgumentPointee;
using ::testing::StrictMock;

namespace media {

class FFmpegVideoDecodeEngineTest : public testing::Test {
 protected:
  FFmpegVideoDecodeEngineTest() {
    // Setup FFmpeg structures.
    memset(&yuv_frame_, 0, sizeof(yuv_frame_));
    memset(&codec_context_, 0, sizeof(codec_context_));
    memset(&codec_, 0, sizeof(codec_));
    memset(&stream_, 0, sizeof(stream_));
    stream_.codec = &codec_context_;

    buffer_ = new DataBuffer(1);

    // Initialize MockFFmpeg.
    MockFFmpeg::set(&mock_ffmpeg_);

    test_engine_.reset(new FFmpegVideoDecodeEngine());
    test_engine_->SetCodecContextForTest(&codec_context_);
  }

  scoped_ptr<FFmpegVideoDecodeEngine> test_engine_;

  StrictMock<MockFFmpeg> mock_ffmpeg_;

  AVFrame yuv_frame_;
  AVCodecContext codec_context_;
  AVStream stream_;
  AVCodec codec_;
  scoped_refptr<DataBuffer> buffer_;
};

TEST_F(FFmpegVideoDecodeEngineTest, Construction) {
  FFmpegVideoDecodeEngine engine;
  EXPECT_FALSE(engine.codec_context());
  EXPECT_EQ(FFmpegVideoDecodeEngine::kCreated, engine.state());
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_Normal) {
  // Test avcodec_open() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(Return(&codec_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecThreadInit(&codec_context_, 2))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecOpen(&codec_context_, &codec_))
      .WillOnce(Return(0));

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  test_engine_->Initialize(&stream_, done_cb.CreateTask());
  EXPECT_EQ(VideoDecodeEngine::kNormal, test_engine_->state());
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_FindDecoderFails) {
  // Test avcodec_find_decoder() returning NULL.
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(ReturnNull());

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  test_engine_->Initialize(&stream_, done_cb.CreateTask());
  EXPECT_EQ(VideoDecodeEngine::kError, test_engine_->state());
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_InitThreadFails) {
  // Test avcodec_thread_init() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(Return(&codec_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecThreadInit(&codec_context_, 2))
      .WillOnce(Return(-1));

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  test_engine_->Initialize(&stream_, done_cb.CreateTask());
  EXPECT_EQ(VideoDecodeEngine::kError, test_engine_->state());
}

TEST_F(FFmpegVideoDecodeEngineTest, Initialize_OpenDecoderFails) {
  // Test avcodec_open() failing.
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecFindDecoder(CODEC_ID_NONE))
      .WillOnce(Return(&codec_));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecThreadInit(&codec_context_, 2))
      .WillOnce(Return(0));
  EXPECT_CALL(*MockFFmpeg::get(), AVCodecOpen(&codec_context_, &codec_))
      .WillOnce(Return(-1));

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  test_engine_->Initialize(&stream_, done_cb.CreateTask());
  EXPECT_EQ(VideoDecodeEngine::kError, test_engine_->state());
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_Normal) {
  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(1),  // Simulate 1 byte frame.
                      Return(0)));

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  bool got_result;
  test_engine_->DecodeFrame(*buffer_, &yuv_frame_, &got_result,
                            done_cb.CreateTask());
  EXPECT_TRUE(got_result);
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_0ByteFrame) {
  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(DoAll(SetArgumentPointee<2>(0),  // Simulate 0 byte frame.
                      Return(0)));

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  bool got_result;
  test_engine_->DecodeFrame(*buffer_, &yuv_frame_, &got_result,
                            done_cb.CreateTask());
  EXPECT_FALSE(got_result);
}

TEST_F(FFmpegVideoDecodeEngineTest, DecodeFrame_DecodeError) {
  // Expect a bunch of avcodec calls.
  EXPECT_CALL(mock_ffmpeg_, AVInitPacket(_));
  EXPECT_CALL(mock_ffmpeg_,
              AVCodecDecodeVideo2(&codec_context_, &yuv_frame_, _, _))
      .WillOnce(Return(-1));

  TaskMocker done_cb;
  EXPECT_CALL(done_cb, Run());

  bool got_result;
  test_engine_->DecodeFrame(*buffer_, &yuv_frame_, &got_result,
                            done_cb.CreateTask());
  EXPECT_FALSE(got_result);
}

TEST_F(FFmpegVideoDecodeEngineTest, GetSurfaceFormat) {
  // YV12 formats.
  codec_context_.pix_fmt = PIX_FMT_YUV420P;
  EXPECT_EQ(VideoSurface::YV12, test_engine_->GetSurfaceFormat());
  codec_context_.pix_fmt = PIX_FMT_YUVJ420P;
  EXPECT_EQ(VideoSurface::YV12, test_engine_->GetSurfaceFormat());

  // YV16 formats.
  codec_context_.pix_fmt = PIX_FMT_YUV422P;
  EXPECT_EQ(VideoSurface::YV16, test_engine_->GetSurfaceFormat());
  codec_context_.pix_fmt = PIX_FMT_YUVJ422P;
  EXPECT_EQ(VideoSurface::YV16, test_engine_->GetSurfaceFormat());

  // Invalid value.
  codec_context_.pix_fmt = PIX_FMT_NONE;
  EXPECT_EQ(VideoSurface::INVALID, test_engine_->GetSurfaceFormat());
}

}  // namespace media
