// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/synchronization/waitable_event.h"
#include "base/task_runner_util.h"
#include "base/threading/thread.h"
#include "content/renderer/media/rtc_video_decoder_factory_tv.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/webrtc/modules/video_coding/codecs/interface/mock/mock_video_codec_interface.h"
#include "ui/gfx/rect.h"

using ::testing::_;
using ::testing::Return;

namespace content {

class RTCVideoDecoderFactoryTvTest : public ::testing::Test {
 public:
  RTCVideoDecoderFactoryTvTest()
      : factory_(new RTCVideoDecoderFactoryTv),
        decoder_(NULL),
        is_demuxer_acquired_(false),
        video_stream_(NULL),
        size_(1280, 720),
        input_image_(&data_, sizeof(data_), sizeof(data_)),
        data_('a'),
        read_event_(false, false),
        decoder_thread_("Test decoder thread"),
        decoder_thread_event_(false, false) {
    memset(&codec_, 0, sizeof(codec_));
    message_loop_proxy_ = base::MessageLoopProxy::current();
    input_image_._frameType = webrtc::kKeyFrame;
    input_image_._encodedWidth = size_.width();
    input_image_._encodedHeight = size_.height();
    input_image_._completeFrame = true;
    decoder_thread_.Start();
  }

  virtual ~RTCVideoDecoderFactoryTvTest() {
    if (is_demuxer_acquired_) {
      factory_->ReleaseDemuxer();
      is_demuxer_acquired_ = false;
    }
    if (decoder_) {
      factory_->DestroyVideoDecoder(decoder_);
      decoder_ = NULL;
    }

    decoder_thread_.Stop();
  }

  void ReadCallback(media::DemuxerStream::Status status,
                    const scoped_refptr<media::DecoderBuffer>& decoder_buffer) {
    switch (status) {
      case media::DemuxerStream::kOk:
        EXPECT_TRUE(decoder_buffer);
        break;
      case media::DemuxerStream::kConfigChanged:
      case media::DemuxerStream::kAborted:
        EXPECT_FALSE(decoder_buffer);
        break;
    }
    last_decoder_buffer_ = decoder_buffer;
    read_event_.Signal();
  }

  void ExpectEqualsAndSignal(int32_t expected, int32_t actual) {
    EXPECT_EQ(expected, actual);
    decoder_thread_event_.Signal();
  }

  void ExpectNotEqualsAndSignal(int32_t unexpected, int32_t actual) {
    EXPECT_NE(unexpected, actual);
    decoder_thread_event_.Signal();
  }

 protected:
  base::Callback<void(int32_t)> BindExpectEquals(int32_t expected) {
    return base::Bind(&RTCVideoDecoderFactoryTvTest::ExpectEqualsAndSignal,
                      base::Unretained(this),
                      expected);
  }

  base::Callback<void(int32_t)> BindExpectNotEquals(int32_t unexpected) {
    return base::Bind(&RTCVideoDecoderFactoryTvTest::ExpectNotEqualsAndSignal,
                      base::Unretained(this),
                      unexpected);
  }

  base::Callback<int32_t(void)> BindInitDecode(const webrtc::VideoCodec* codec,
                                               int32_t num_cores) {
    return base::Bind(&webrtc::VideoDecoder::InitDecode,
                      base::Unretained(decoder_),
                      codec,
                      num_cores);
  }

  base::Callback<int32_t(void)> BindDecode(
      const webrtc::EncodedImage& input_image,
      bool missing_frames,
      const webrtc::RTPFragmentationHeader* fragmentation,
      const webrtc::CodecSpecificInfo* info,
      int64_t render_time_ms) {
    return base::Bind(&webrtc::VideoDecoder::Decode,
                      base::Unretained(decoder_),
                      input_image,
                      missing_frames,
                      fragmentation,
                      info,
                      render_time_ms);
  }

  void CreateDecoderAndAcquireDemuxer() {
    decoder_ = factory_->CreateVideoDecoder(webrtc::kVideoCodecVP8);
    ASSERT_TRUE(decoder_);
    ASSERT_TRUE(factory_->AcquireDemuxer());
    is_demuxer_acquired_ = true;
  }

  void InitDecode() {
    codec_.codecType = webrtc::kVideoCodecVP8;
    codec_.width = size_.width();
    codec_.height = size_.height();
    base::PostTaskAndReplyWithResult(decoder_thread_.message_loop_proxy(),
                                     FROM_HERE,
                                     BindInitDecode(&codec_, 1),
                                     BindExpectEquals(WEBRTC_VIDEO_CODEC_OK));
    decoder_thread_event_.Wait();
    base::PostTaskAndReplyWithResult(
        decoder_thread_.message_loop_proxy(),
        FROM_HERE,
        base::Bind(&webrtc::VideoDecoder::RegisterDecodeCompleteCallback,
                   base::Unretained(decoder_),
                   &decode_complete_callback_),
        BindExpectEquals(WEBRTC_VIDEO_CODEC_OK));
    decoder_thread_event_.Wait();
  }

  void GetVideoStream() {
    video_stream_ = factory_->GetStream(media::DemuxerStream::VIDEO);
    ASSERT_TRUE(video_stream_);
    EXPECT_EQ(media::kCodecVP8, video_stream_->video_decoder_config().codec());
    EXPECT_EQ(size_, video_stream_->video_decoder_config().coded_size());
    EXPECT_EQ(gfx::Rect(size_),
              video_stream_->video_decoder_config().visible_rect());
    EXPECT_EQ(size_, video_stream_->video_decoder_config().natural_size());
  }

  void PostDecodeAndWait(int32_t expected,
                         const webrtc::EncodedImage& input_image,
                         bool missing_frames,
                         const webrtc::RTPFragmentationHeader* fragmentation,
                         const webrtc::CodecSpecificInfo* info,
                         int64_t render_time_ms) {
    base::PostTaskAndReplyWithResult(
        decoder_thread_.message_loop_proxy(),
        FROM_HERE,
        BindDecode(
            input_image, missing_frames, fragmentation, info, render_time_ms),
        BindExpectEquals(expected));
    decoder_thread_event_.Wait();
  }

  RTCVideoDecoderFactoryTv* factory_;
  webrtc::VideoDecoder* decoder_;
  bool is_demuxer_acquired_;
  base::MessageLoopProxy* message_loop_proxy_;
  media::DemuxerStream* video_stream_;
  webrtc::VideoCodec codec_;
  gfx::Size size_;
  webrtc::EncodedImage input_image_;
  unsigned char data_;
  webrtc::MockDecodedImageCallback decode_complete_callback_;
  base::WaitableEvent read_event_;
  base::Thread decoder_thread_;
  base::WaitableEvent decoder_thread_event_;
  scoped_refptr<media::DecoderBuffer> last_decoder_buffer_;
};

TEST_F(RTCVideoDecoderFactoryTvTest, CreateAndDestroyDecoder) {
  // Only VP8 decoder is supported.
  ASSERT_FALSE(factory_->CreateVideoDecoder(webrtc::kVideoCodecI420));
  decoder_ = factory_->CreateVideoDecoder(webrtc::kVideoCodecVP8);
  ASSERT_TRUE(decoder_);
  // Only one decoder at a time will be created.
  ASSERT_FALSE(factory_->CreateVideoDecoder(webrtc::kVideoCodecVP8));
  factory_->DestroyVideoDecoder(decoder_);
}

TEST_F(RTCVideoDecoderFactoryTvTest, AcquireDemuxerAfterCreateDecoder) {
  decoder_ = factory_->CreateVideoDecoder(webrtc::kVideoCodecVP8);
  ASSERT_TRUE(decoder_);
  ASSERT_TRUE(factory_->AcquireDemuxer());
  is_demuxer_acquired_ = true;
  // Demuxer can be acquired only once.
  ASSERT_FALSE(factory_->AcquireDemuxer());
}

TEST_F(RTCVideoDecoderFactoryTvTest, AcquireDemuxerBeforeCreateDecoder) {
  ASSERT_TRUE(factory_->AcquireDemuxer());
  is_demuxer_acquired_ = true;
  decoder_ = factory_->CreateVideoDecoder(webrtc::kVideoCodecVP8);
  ASSERT_TRUE(decoder_);
}

TEST_F(RTCVideoDecoderFactoryTvTest, InitDecodeReturnsErrorOnNonVP8Codec) {
  CreateDecoderAndAcquireDemuxer();
  codec_.codecType = webrtc::kVideoCodecI420;
  base::PostTaskAndReplyWithResult(decoder_thread_.message_loop_proxy(),
                                   FROM_HERE,
                                   BindInitDecode(&codec_, 1),
                                   BindExpectNotEquals(WEBRTC_VIDEO_CODEC_OK));
  decoder_thread_event_.Wait();
}

TEST_F(RTCVideoDecoderFactoryTvTest, InitDecodeReturnsErrorOnFeedbackMode) {
  CreateDecoderAndAcquireDemuxer();
  codec_.codecType = webrtc::kVideoCodecVP8;
  codec_.codecSpecific.VP8.feedbackModeOn = true;
  base::PostTaskAndReplyWithResult(decoder_thread_.message_loop_proxy(),
                                   FROM_HERE,
                                   BindInitDecode(&codec_, 1),
                                   BindExpectNotEquals(WEBRTC_VIDEO_CODEC_OK));
  decoder_thread_event_.Wait();
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodeReturnsErrorBeforeInitDecode) {
  CreateDecoderAndAcquireDemuxer();
  PostDecodeAndWait(
      WEBRTC_VIDEO_CODEC_UNINITIALIZED, input_image_, false, NULL, NULL, 0);
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodeReturnsErrorOnDamagedBitstream) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  input_image_._completeFrame = false;
  PostDecodeAndWait(
      WEBRTC_VIDEO_CODEC_ERROR, input_image_, false, NULL, NULL, 0);
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodeReturnsErrorOnMissingFrames) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  PostDecodeAndWait(
      WEBRTC_VIDEO_CODEC_ERROR, input_image_, true, NULL, NULL, 0);
}

TEST_F(RTCVideoDecoderFactoryTvTest, GetNonVideoStreamFails) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  EXPECT_FALSE(factory_->GetStream(media::DemuxerStream::AUDIO));
  EXPECT_FALSE(factory_->GetStream(media::DemuxerStream::UNKNOWN));
}

TEST_F(RTCVideoDecoderFactoryTvTest, GetVideoStreamSucceeds) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  GetVideoStream();
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodeReturnsErrorOnNonKeyFrameAtFirst) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  GetVideoStream();
  input_image_._frameType = webrtc::kDeltaFrame;
  PostDecodeAndWait(
      WEBRTC_VIDEO_CODEC_ERROR, input_image_, false, NULL, NULL, 0);
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodeUpdatesVideoSizeOnKeyFrame) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  GetVideoStream();
  gfx::Size new_size(320, 240);
  input_image_._encodedWidth = new_size.width();
  input_image_._encodedHeight = new_size.height();
  PostDecodeAndWait(WEBRTC_VIDEO_CODEC_OK, input_image_, false, NULL, NULL, 0);
  EXPECT_EQ(new_size, video_stream_->video_decoder_config().coded_size());
  EXPECT_EQ(gfx::Rect(new_size),
            video_stream_->video_decoder_config().visible_rect());
  EXPECT_EQ(new_size, video_stream_->video_decoder_config().natural_size());
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodeAdjustsTimestampFromZero) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  GetVideoStream();
  PostDecodeAndWait(
      WEBRTC_VIDEO_CODEC_OK, input_image_, false, NULL, NULL, 10000);
  video_stream_->Read(base::Bind(&RTCVideoDecoderFactoryTvTest::ReadCallback,
                                 base::Unretained(this)));
  read_event_.Wait();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(0),
            last_decoder_buffer_->GetTimestamp());
  PostDecodeAndWait(
      WEBRTC_VIDEO_CODEC_OK, input_image_, false, NULL, NULL, 10033);
  video_stream_->Read(base::Bind(&RTCVideoDecoderFactoryTvTest::ReadCallback,
                                 base::Unretained(this)));
  read_event_.Wait();
  EXPECT_EQ(base::TimeDelta::FromMilliseconds(33),
            last_decoder_buffer_->GetTimestamp());
}

TEST_F(RTCVideoDecoderFactoryTvTest, DecodePassesDataCorrectly) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  GetVideoStream();
  video_stream_->Read(base::Bind(&RTCVideoDecoderFactoryTvTest::ReadCallback,
                                 base::Unretained(this)));
  PostDecodeAndWait(WEBRTC_VIDEO_CODEC_OK, input_image_, false, NULL, NULL, 0);
  read_event_.Wait();
  EXPECT_EQ(static_cast<int>(sizeof(data_)),
            last_decoder_buffer_->GetDataSize());
  EXPECT_EQ(data_, last_decoder_buffer_->GetData()[0]);
}

TEST_F(RTCVideoDecoderFactoryTvTest, NextReadTriggersDecodeCompleteCallback) {
  EXPECT_CALL(decode_complete_callback_, Decoded(_))
      .WillOnce(Return(WEBRTC_VIDEO_CODEC_OK));

  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  GetVideoStream();
  video_stream_->Read(base::Bind(&RTCVideoDecoderFactoryTvTest::ReadCallback,
                                 base::Unretained(this)));
  PostDecodeAndWait(WEBRTC_VIDEO_CODEC_OK, input_image_, false, NULL, NULL, 0);
  read_event_.Wait();
  video_stream_->Read(base::Bind(&RTCVideoDecoderFactoryTvTest::ReadCallback,
                                 base::Unretained(this)));
}

TEST_F(RTCVideoDecoderFactoryTvTest, ResetReturnsOk) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Reset());
}

TEST_F(RTCVideoDecoderFactoryTvTest, ReleaseReturnsOk) {
  CreateDecoderAndAcquireDemuxer();
  InitDecode();
  EXPECT_EQ(WEBRTC_VIDEO_CODEC_OK, decoder_->Release());
}

}  // content
