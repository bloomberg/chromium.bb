// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "media/base/audio_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/opus_audio_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

class OpusAudioDecoderTest : public testing::Test {
 public:
  OpusAudioDecoderTest()
      : decoder_(new OpusAudioDecoder(message_loop_.message_loop_proxy())),
        pending_decode_(false),
        pending_reset_(false) {
    // Load the test data file.
    data_ = ReadTestDataFile("bear-opus.ogg");
    protocol_.reset(
        new InMemoryUrlProtocol(data_->data(), data_->data_size(), false));
    reader_.reset(new AudioFileReader(protocol_.get()));
    reader_->Open();

    AudioDecoderConfig config;
    AVCodecContextToAudioDecoderConfig(
        reader_->codec_context_for_testing(), false, &config, false);
    decoder_->Initialize(config, NewExpectedStatusCB(PIPELINE_OK));
    base::RunLoop().RunUntilIdle();
  }

  virtual ~OpusAudioDecoderTest() {
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
  }

 protected:
  void SatisfyPendingDecode() { base::RunLoop().RunUntilIdle(); }

  void SendEndOfStream() {
    pending_decode_ = true;
    decoder_->Decode(DecoderBuffer::CreateEOSBuffer(),
                     base::Bind(&OpusAudioDecoderTest::DecodeFinished,
                                base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Decode() {
    pending_decode_ = true;

    AVPacket packet;
    ASSERT_TRUE(reader_->ReadPacketForTesting(&packet));
    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(packet.data, packet.size);
    buffer->set_timestamp(ConvertFromTimeBase(
        reader_->codec_context_for_testing()->time_base, packet.pts));
    buffer->set_duration(ConvertFromTimeBase(
        reader_->codec_context_for_testing()->time_base, packet.duration));
    decoder_->Decode(buffer,
                     base::Bind(&OpusAudioDecoderTest::DecodeFinished,
                                base::Unretained(this)));
    av_free_packet(&packet);
    base::RunLoop().RunUntilIdle();
  }

  void Reset() {
    pending_reset_ = true;
    decoder_->Reset(base::Bind(&OpusAudioDecoderTest::ResetFinished,
                               base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Stop() {
    decoder_->Stop();
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
    if (buffer.get() == NULL || pending_reset_) {
      EXPECT_TRUE(buffer.get() == NULL);
      EXPECT_EQ(status, AudioDecoder::kAborted);
      return;
    }

    EXPECT_EQ(status, AudioDecoder::kOk);
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

  size_t decoded_audio_size() const {
    return decoded_audio_.size();
  }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<DecoderBuffer> data_;
  scoped_ptr<InMemoryUrlProtocol> protocol_;
  scoped_ptr<AudioFileReader> reader_;

  scoped_ptr<OpusAudioDecoder> decoder_;
  bool pending_decode_;
  bool pending_reset_;

  std::deque<scoped_refptr<AudioBuffer> > decoded_audio_;

  DISALLOW_COPY_AND_ASSIGN(OpusAudioDecoderTest);
};

TEST_F(OpusAudioDecoderTest, Initialize) {
  Stop();
}

TEST_F(OpusAudioDecoderTest, ProduceAudioSamples) {
  Decode();
  Decode();
  Decode();

  ASSERT_EQ(3u, decoded_audio_size());
  ExpectDecodedAudio(0, 0, 3500);
  ExpectDecodedAudio(1, 3500, 10000);
  ExpectDecodedAudio(2, 13500, 10000);

  // Call one more time to trigger EOS.
  SendEndOfStream();
  ASSERT_EQ(4u, decoded_audio_size());
  ExpectEndOfStream(3);
  Stop();
}

TEST_F(OpusAudioDecoderTest, DecodeAbort) {
  Decode();
  Stop();
}

TEST_F(OpusAudioDecoderTest, PendingDecode_Stop) {
  Decode();
  Stop();
  SatisfyPendingDecode();
}

TEST_F(OpusAudioDecoderTest, PendingDecode_Reset) {
  Decode();
  Reset();
  SatisfyPendingDecode();
  Stop();
}

TEST_F(OpusAudioDecoderTest, PendingDecode_ResetStop) {
  Decode();
  Reset();
  Stop();
  SatisfyPendingDecode();
}

}  // namespace media
