// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/mock_filters.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/ffmpeg_glue.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::StrictMock;

namespace media {

class FFmpegAudioDecoderTest : public testing::Test {
 public:
  FFmpegAudioDecoderTest()
      : decoder_(new FFmpegAudioDecoder(message_loop_.message_loop_proxy())),
        pending_decode_(false),
        pending_reset_(false),
        pending_stop_(false) {
    FFmpegGlue::InitializeFFmpeg();

    vorbis_extradata_ = ReadTestDataFile("vorbis-extradata");

    // Refer to media/test/data/README for details on vorbis test data.
    for (int i = 0; i < 4; ++i) {
      scoped_refptr<DecoderBuffer> buffer =
          ReadTestDataFile(base::StringPrintf("vorbis-packet-%d", i));

      if (i < 3) {
        buffer->set_timestamp(base::TimeDelta());
      } else {
        buffer->set_timestamp(base::TimeDelta::FromMicroseconds(2902));
      }

      buffer->set_duration(base::TimeDelta());
      encoded_audio_.push_back(buffer);
    }

    // Push in an EOS buffer.
    encoded_audio_.push_back(DecoderBuffer::CreateEOSBuffer());

    Initialize();
  }

  virtual ~FFmpegAudioDecoderTest() {
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
    EXPECT_FALSE(pending_stop_);
  }

  void Initialize() {
    AudioDecoderConfig config(kCodecVorbis,
                              kSampleFormatPlanarF32,
                              CHANNEL_LAYOUT_STEREO,
                              44100,
                              vorbis_extradata_->data(),
                              vorbis_extradata_->data_size(),
                              false);  // Not encrypted.
    decoder_->Initialize(config,
                         NewExpectedStatusCB(PIPELINE_OK));
    base::RunLoop().RunUntilIdle();
  }

  void SatisfyPendingDecode() {
    base::RunLoop().RunUntilIdle();
  }

  void Decode() {
    pending_decode_ = true;
    scoped_refptr<DecoderBuffer> buffer(encoded_audio_.front());
    encoded_audio_.pop_front();
    decoder_->Decode(buffer,
                     base::Bind(&FFmpegAudioDecoderTest::DecodeFinished,
                                base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Reset() {
    pending_reset_ = true;
    decoder_->Reset(base::Bind(
        &FFmpegAudioDecoderTest::ResetFinished, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Stop() {
    pending_stop_ = true;
    decoder_->Stop(base::Bind(
        &FFmpegAudioDecoderTest::StopFinished, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void DecodeFinished(AudioDecoder::Status status,
                      const scoped_refptr<AudioBuffer>& buffer) {
    EXPECT_TRUE(pending_decode_);
    pending_decode_ = false;

    if (status == AudioDecoder::kNotEnoughData) {
      EXPECT_TRUE(buffer.get() == NULL);
      Decode();
      return;
    }

    decoded_audio_.push_back(buffer);

    // If we hit a NULL buffer or have a pending reset, we expect an abort.
    if (buffer.get() == NULL || pending_stop_ || pending_reset_) {
      EXPECT_TRUE(buffer.get() == NULL);
      EXPECT_EQ(status, AudioDecoder::kAborted);
      return;
    }

    EXPECT_EQ(status, AudioDecoder::kOk);
  }

  void StopFinished() {
    EXPECT_TRUE(pending_stop_);
    // Stop should always finish after Decode and Reset.
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);

    pending_stop_ = false;
  }

  void ResetFinished() {
    EXPECT_TRUE(pending_reset_);
    // Reset should always finish after Decode.
    EXPECT_FALSE(pending_decode_);

    pending_reset_ = false;
  }

  void ExpectDecodedAudio(size_t i, int64 timestamp, int64 duration) {
    EXPECT_LT(i, decoded_audio_.size());
    EXPECT_EQ(timestamp, decoded_audio_[i]->timestamp().InMicroseconds());
    EXPECT_EQ(duration, decoded_audio_[i]->duration().InMicroseconds());
    EXPECT_FALSE(decoded_audio_[i]->end_of_stream());
  }

  void ExpectEndOfStream(size_t i) {
    EXPECT_LT(i, decoded_audio_.size());
    EXPECT_TRUE(decoded_audio_[i]->end_of_stream());
  }

  base::MessageLoop message_loop_;
  scoped_ptr<FFmpegAudioDecoder> decoder_;
  bool pending_decode_;
  bool pending_reset_;
  bool pending_stop_;

  scoped_refptr<DecoderBuffer> vorbis_extradata_;

  std::deque<scoped_refptr<DecoderBuffer> > encoded_audio_;
  std::deque<scoped_refptr<AudioBuffer> > decoded_audio_;
};

TEST_F(FFmpegAudioDecoderTest, Initialize) {
  AudioDecoderConfig config(kCodecVorbis,
                            kSampleFormatPlanarF32,
                            CHANNEL_LAYOUT_STEREO,
                            44100,
                            vorbis_extradata_->data(),
                            vorbis_extradata_->data_size(),
                            false);  // Not encrypted.
  EXPECT_EQ(config.bits_per_channel(), decoder_->bits_per_channel());
  EXPECT_EQ(config.channel_layout(), decoder_->channel_layout());
  EXPECT_EQ(config.samples_per_second(), decoder_->samples_per_second());
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, ProduceAudioSamples) {
  // Vorbis requires N+1 packets to produce audio data for N packets.
  //
  // This will should result in the demuxer receiving three reads for two
  // requests to produce audio samples.
  Decode();
  Decode();
  Decode();

  ASSERT_EQ(3u, decoded_audio_.size());
  ExpectDecodedAudio(0, 0, 2902);
  ExpectDecodedAudio(1, 2902, 13061);
  ExpectDecodedAudio(2, 15963, 23220);

  // Call one more time to trigger EOS.
  Decode();
  ASSERT_EQ(4u, decoded_audio_.size());
  ExpectEndOfStream(3);
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, DecodeAbort) {
  encoded_audio_.clear();
  encoded_audio_.push_back(NULL);
  Decode();

  EXPECT_EQ(decoded_audio_.size(), 1u);
  EXPECT_TRUE(decoded_audio_[0].get() ==  NULL);
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, PendingDecode_Stop) {
  Decode();
  Stop();
  SatisfyPendingDecode();
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, PendingDecode_Reset) {
  Decode();
  Reset();
  SatisfyPendingDecode();
  Stop();
}

TEST_F(FFmpegAudioDecoderTest, PendingDecode_ResetStop) {
  Decode();
  Reset();
  Stop();
  SatisfyPendingDecode();
}

}  // namespace media
