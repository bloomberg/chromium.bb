// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <deque>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/md5.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/sys_byteorder.h"
#include "build/build_config.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_bus.h"
#include "media/base/audio_hash.h"
#include "media/base/decoder_buffer.h"
#include "media/base/test_data_util.h"
#include "media/base/test_helpers.h"
#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/audio_file_reader.h"
#include "media/filters/ffmpeg_audio_decoder.h"
#include "media/filters/in_memory_url_protocol.h"
#include "media/filters/opus_audio_decoder.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// The number of packets to read and then decode from each file.
static const size_t kDecodeRuns = 3;
static const uint8_t kOpusExtraData[] = {
    0x4f, 0x70, 0x75, 0x73, 0x48, 0x65, 0x61, 0x64, 0x01, 0x02,
    // The next two bytes represent the codec delay.
    0x00, 0x00, 0x80, 0xbb, 0x00, 0x00, 0x00, 0x00, 0x00};

enum AudioDecoderType {
  FFMPEG,
  OPUS,
};

struct DecodedBufferExpectations {
  const int64 timestamp;
  const int64 duration;
  const char* hash;
};

struct DecoderTestData {
  const AudioDecoderType decoder_type;
  const AudioCodec codec;
  const char* filename;
  const DecodedBufferExpectations* expectations;
  const int first_packet_pts;
  const int samples_per_second;
  const ChannelLayout channel_layout;
};

// Tells gtest how to print our DecoderTestData structure.
std::ostream& operator<<(std::ostream& os, const DecoderTestData& data) {
  return os << data.filename;
}

// Marks negative timestamp buffers for discard or transfers FFmpeg's built in
// discard metadata in favor of setting DiscardPadding on the DecoderBuffer.
// Allows better testing of AudioDiscardHelper usage.
static void SetDiscardPadding(AVPacket* packet,
                              const scoped_refptr<DecoderBuffer> buffer,
                              double samples_per_second) {
  // Discard negative timestamps.
  if (buffer->timestamp() + buffer->duration() < base::TimeDelta()) {
    buffer->set_discard_padding(
        std::make_pair(kInfiniteDuration(), base::TimeDelta()));
    return;
  }
  if (buffer->timestamp() < base::TimeDelta()) {
    buffer->set_discard_padding(
        std::make_pair(-buffer->timestamp(), base::TimeDelta()));
    return;
  }

  // If the timestamp is positive, try to use FFmpeg's discard data.
  int skip_samples_size = 0;
  const uint32* skip_samples_ptr =
      reinterpret_cast<const uint32*>(av_packet_get_side_data(
          packet, AV_PKT_DATA_SKIP_SAMPLES, &skip_samples_size));
  if (skip_samples_size < 4)
    return;
  buffer->set_discard_padding(std::make_pair(
      base::TimeDelta::FromSecondsD(base::ByteSwapToLE32(*skip_samples_ptr) /
                                    samples_per_second),
      base::TimeDelta()));
}

class AudioDecoderTest : public testing::TestWithParam<DecoderTestData> {
 public:
  AudioDecoderTest()
      : pending_decode_(false),
        pending_reset_(false),
        last_decode_status_(AudioDecoder::kDecodeError) {
    switch (GetParam().decoder_type) {
      case FFMPEG:
        decoder_.reset(new FFmpegAudioDecoder(
            message_loop_.message_loop_proxy(), LogCB()));
        break;
      case OPUS:
        decoder_.reset(
            new OpusAudioDecoder(message_loop_.message_loop_proxy()));
        break;
    }
  }

  virtual ~AudioDecoderTest() {
    EXPECT_FALSE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
  }

 protected:
  void DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer) {
    ASSERT_FALSE(pending_decode_);
    pending_decode_ = true;
    last_decode_status_ = AudioDecoder::kDecodeError;
    decoder_->Decode(
        buffer,
        base::Bind(&AudioDecoderTest::DecodeFinished, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(pending_decode_);
  }

  void SendEndOfStream() {
    DecodeBuffer(DecoderBuffer::CreateEOSBuffer());
  }

  void Initialize() {
    // Load the test data file.
    data_ = ReadTestDataFile(GetParam().filename);
    protocol_.reset(
        new InMemoryUrlProtocol(data_->data(), data_->data_size(), false));
    reader_.reset(new AudioFileReader(protocol_.get()));
    ASSERT_TRUE(reader_->OpenDemuxerForTesting());

    // Load the first packet and check its timestamp.
    AVPacket packet;
    ASSERT_TRUE(reader_->ReadPacketForTesting(&packet));
    EXPECT_EQ(GetParam().first_packet_pts, packet.pts);
    start_timestamp_ = ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet.pts);
    av_free_packet(&packet);

    // Seek back to the beginning.
    ASSERT_TRUE(reader_->SeekForTesting(start_timestamp_));

    AudioDecoderConfig config;
    AVCodecContextToAudioDecoderConfig(
        reader_->codec_context_for_testing(), false, &config, false);

    EXPECT_EQ(GetParam().codec, config.codec());
    EXPECT_EQ(GetParam().samples_per_second, config.samples_per_second());
    EXPECT_EQ(GetParam().channel_layout, config.channel_layout());

    InitializeDecoder(config);
  }

  void InitializeDecoder(const AudioDecoderConfig& config) {
    InitializeDecoderWithStatus(config, PIPELINE_OK);
  }

  void InitializeDecoderWithStatus(const AudioDecoderConfig& config,
                                   PipelineStatus status) {
    decoder_->Initialize(
        config,
        NewExpectedStatusCB(status),
        base::Bind(&AudioDecoderTest::OnDecoderOutput, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
  }

  void Decode() {
    AVPacket packet;
    ASSERT_TRUE(reader_->ReadPacketForTesting(&packet));

    // Split out packet metadata before making a copy.
    av_packet_split_side_data(&packet);

    scoped_refptr<DecoderBuffer> buffer =
        DecoderBuffer::CopyFrom(packet.data, packet.size);
    buffer->set_timestamp(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet.pts));
    buffer->set_duration(ConvertFromTimeBase(
        reader_->GetAVStreamForTesting()->time_base, packet.duration));

    // Don't set discard padding for Opus, it already has discard behavior set
    // based on the codec delay in the AudioDecoderConfig.
    if (GetParam().decoder_type == FFMPEG)
      SetDiscardPadding(&packet, buffer, GetParam().samples_per_second);

    // DecodeBuffer() shouldn't need the original packet since it uses the copy.
    av_free_packet(&packet);
    DecodeBuffer(buffer);
  }

  void Reset() {
    ASSERT_FALSE(pending_reset_);
    pending_reset_ = true;
    decoder_->Reset(
        base::Bind(&AudioDecoderTest::ResetFinished, base::Unretained(this)));
    base::RunLoop().RunUntilIdle();
    ASSERT_FALSE(pending_reset_);
  }

  void Seek(base::TimeDelta seek_time) {
    Reset();
    decoded_audio_.clear();
    ASSERT_TRUE(reader_->SeekForTesting(seek_time));
  }

  void OnDecoderOutput(const scoped_refptr<AudioBuffer>& buffer) {
    EXPECT_FALSE(buffer->end_of_stream());
    decoded_audio_.push_back(buffer);
  }

  void DecodeFinished(AudioDecoder::Status status) {
    EXPECT_TRUE(pending_decode_);
    EXPECT_FALSE(pending_reset_);
    pending_decode_ = false;
    last_decode_status_ = status;
  }

  void ResetFinished() {
    EXPECT_TRUE(pending_reset_);
    EXPECT_FALSE(pending_decode_);
    pending_reset_ = false;
  }

  // Generates an MD5 hash of the audio signal.  Should not be used for checks
  // across platforms as audio varies slightly across platforms.
  std::string GetDecodedAudioMD5(size_t i) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    scoped_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    base::MD5Context context;
    base::MD5Init(&context);
    for (int ch = 0; ch < output->channels(); ++ch) {
      base::MD5Update(
          &context,
          base::StringPiece(reinterpret_cast<char*>(output->channel(ch)),
                            output->frames() * sizeof(*output->channel(ch))));
    }
    base::MD5Digest digest;
    base::MD5Final(&digest, &context);
    return base::MD5DigestToBase16(digest);
  }

  void ExpectDecodedAudio(size_t i, const std::string& exact_hash) {
    CHECK_LT(i, decoded_audio_.size());
    const scoped_refptr<AudioBuffer>& buffer = decoded_audio_[i];

    const DecodedBufferExpectations& sample_info = GetParam().expectations[i];
    EXPECT_EQ(sample_info.timestamp, buffer->timestamp().InMicroseconds());
    EXPECT_EQ(sample_info.duration, buffer->duration().InMicroseconds());
    EXPECT_FALSE(buffer->end_of_stream());

    scoped_ptr<AudioBus> output =
        AudioBus::Create(buffer->channel_count(), buffer->frame_count());
    buffer->ReadFrames(buffer->frame_count(), 0, 0, output.get());

    // Generate a lossy hash of the audio used for comparison across platforms.
    AudioHash audio_hash;
    audio_hash.Update(output.get(), output->frames());
    EXPECT_EQ(sample_info.hash, audio_hash.ToString());

    if (!exact_hash.empty()) {
      EXPECT_EQ(exact_hash, GetDecodedAudioMD5(i));

      // Verify different hashes are being generated.  None of our test data
      // files have audio that hashes out exactly the same.
      if (i > 0)
        EXPECT_NE(exact_hash, GetDecodedAudioMD5(i - 1));
    }
  }

  size_t decoded_audio_size() const { return decoded_audio_.size(); }
  base::TimeDelta start_timestamp() const { return start_timestamp_; }
  const scoped_refptr<AudioBuffer>& decoded_audio(size_t i) {
    return decoded_audio_[i];
  }
  AudioDecoder::Status last_decode_status() const {
    return last_decode_status_;
  }

 private:
  base::MessageLoop message_loop_;
  scoped_refptr<DecoderBuffer> data_;
  scoped_ptr<InMemoryUrlProtocol> protocol_;
  scoped_ptr<AudioFileReader> reader_;

  scoped_ptr<AudioDecoder> decoder_;
  bool pending_decode_;
  bool pending_reset_;
  AudioDecoder::Status last_decode_status_;

  std::deque<scoped_refptr<AudioBuffer> > decoded_audio_;
  base::TimeDelta start_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(AudioDecoderTest);
};

class OpusAudioDecoderBehavioralTest : public AudioDecoderTest {};
class FFmpegAudioDecoderBehavioralTest : public AudioDecoderTest {};

TEST_P(AudioDecoderTest, Initialize) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
}

// Verifies decode audio as well as the Decode() -> Reset() sequence.
TEST_P(AudioDecoderTest, ProduceAudioSamples) {
  ASSERT_NO_FATAL_FAILURE(Initialize());

  // Run the test multiple times with a seek back to the beginning in between.
  std::vector<std::string> decoded_audio_md5_hashes;
  for (int i = 0; i < 2; ++i) {
    for (size_t j = 0; j < kDecodeRuns; ++j) {
      do {
        Decode();
        ASSERT_EQ(last_decode_status(), AudioDecoder::kOk);
        // Some codecs have a multiple buffer delay and require an extra
        // Decode() step to extract the desired number of output buffers.
      } while (j == 0 && decoded_audio_size() == 0);

      // On the first pass record the exact MD5 hash for each decoded buffer.
      if (i == 0)
        decoded_audio_md5_hashes.push_back(GetDecodedAudioMD5(j));
    }

    ASSERT_EQ(kDecodeRuns, decoded_audio_size());

    // On the first pass verify the basic audio hash and sample info.  On the
    // second, verify the exact MD5 sum for each packet.  It shouldn't change.
    for (size_t j = 0; j < kDecodeRuns; ++j) {
      SCOPED_TRACE(base::StringPrintf("i = %d, j = %" PRIuS, i, j));
      ExpectDecodedAudio(j, i == 0 ? "" : decoded_audio_md5_hashes[j]);
    }

    SendEndOfStream();
    ASSERT_EQ(kDecodeRuns, decoded_audio_size());

    // Seek back to the beginning.  Calls Reset() on the decoder.
    Seek(start_timestamp());
  }
}

TEST_P(AudioDecoderTest, Decode) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Decode();
  EXPECT_EQ(AudioDecoder::kOk, last_decode_status());
}

TEST_P(AudioDecoderTest, Reset) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  Reset();
}

TEST_P(AudioDecoderTest, NoTimestamp) {
  ASSERT_NO_FATAL_FAILURE(Initialize());
  scoped_refptr<DecoderBuffer> buffer(new DecoderBuffer(0));
  buffer->set_timestamp(kNoTimestamp());
  DecodeBuffer(buffer);
  EXPECT_EQ(AudioDecoder::kDecodeError, last_decode_status());
}

TEST_P(OpusAudioDecoderBehavioralTest, InitializeWithNoCodecDelay) {
  ASSERT_EQ(GetParam().decoder_type, OPUS);
  AudioDecoderConfig decoder_config;
  decoder_config.Initialize(kCodecOpus,
                            kSampleFormatF32,
                            CHANNEL_LAYOUT_STEREO,
                            48000,
                            kOpusExtraData,
                            ARRAYSIZE_UNSAFE(kOpusExtraData),
                            false,
                            false,
                            base::TimeDelta::FromMilliseconds(80),
                            0);
  InitializeDecoder(decoder_config);
}

TEST_P(OpusAudioDecoderBehavioralTest, InitializeWithBadCodecDelay) {
  ASSERT_EQ(GetParam().decoder_type, OPUS);
  AudioDecoderConfig decoder_config;
  decoder_config.Initialize(
      kCodecOpus,
      kSampleFormatF32,
      CHANNEL_LAYOUT_STEREO,
      48000,
      kOpusExtraData,
      ARRAYSIZE_UNSAFE(kOpusExtraData),
      false,
      false,
      base::TimeDelta::FromMilliseconds(80),
      // Use a different codec delay than in the extradata.
      100);
  InitializeDecoderWithStatus(decoder_config, DECODER_ERROR_NOT_SUPPORTED);
}

TEST_P(FFmpegAudioDecoderBehavioralTest, InitializeWithBadConfig) {
  const AudioDecoderConfig decoder_config(kCodecVorbis,
                                          kSampleFormatF32,
                                          CHANNEL_LAYOUT_STEREO,
                                          // Invalid sample rate of zero.
                                          0,
                                          NULL,
                                          0,
                                          false);
  InitializeDecoderWithStatus(decoder_config, DECODER_ERROR_NOT_SUPPORTED);
}

const DecodedBufferExpectations kSfxOpusExpectations[] = {
    {0, 13500, "-2.70,-1.41,-0.78,-1.27,-2.56,-3.73,"},
    {13500, 20000, "5.48,5.93,6.04,5.83,5.54,5.45,"},
    {33500, 20000, "-3.45,-3.35,-3.57,-4.12,-4.74,-5.14,"},
};

const DecodedBufferExpectations kBearOpusExpectations[] = {
    {500, 3500, "-0.26,0.87,1.36,0.84,-0.30,-1.22,"},
    {4000, 10000, "0.09,0.23,0.21,0.03,-0.17,-0.24,"},
    {14000, 10000, "0.10,0.24,0.23,0.04,-0.14,-0.23,"},
};

const DecoderTestData kOpusTests[] = {
    {OPUS, kCodecOpus, "sfx-opus.ogg", kSfxOpusExpectations, -312, 48000,
     CHANNEL_LAYOUT_MONO},
    {OPUS, kCodecOpus, "bear-opus.ogg", kBearOpusExpectations, 24, 48000,
     CHANNEL_LAYOUT_STEREO},
};

// Dummy data for behavioral tests.
const DecoderTestData kOpusBehavioralTest[] = {
    {OPUS, kUnknownAudioCodec, "", NULL, 0, 0, CHANNEL_LAYOUT_NONE},
};

INSTANTIATE_TEST_CASE_P(OpusAudioDecoderTest,
                        AudioDecoderTest,
                        testing::ValuesIn(kOpusTests));
INSTANTIATE_TEST_CASE_P(OpusAudioDecoderBehavioralTest,
                        OpusAudioDecoderBehavioralTest,
                        testing::ValuesIn(kOpusBehavioralTest));

#if defined(USE_PROPRIETARY_CODECS)
const DecodedBufferExpectations kSfxMp3Expectations[] = {
    {0, 1065, "2.81,3.99,4.53,4.10,3.08,2.46,"},
    {1065, 26122, "-3.81,-4.14,-3.90,-3.36,-3.03,-3.23,"},
    {27188, 26122, "4.24,3.95,4.22,4.78,5.13,4.93,"},
};

const DecodedBufferExpectations kSfxAdtsExpectations[] = {
    {0, 23219, "-1.90,-1.53,-0.15,1.28,1.23,-0.33,"},
    {23219, 23219, "0.54,0.88,2.19,3.54,3.24,1.63,"},
    {46439, 23219, "1.42,1.69,2.95,4.23,4.02,2.36,"},
};
#endif

#if defined(OS_CHROMEOS)
const DecodedBufferExpectations kSfxFlacExpectations[] = {
    {0, 104489, "-2.42,-1.12,0.71,1.70,1.09,-0.68,"},
    {104489, 104489, "-1.99,-0.67,1.18,2.19,1.60,-0.16,"},
    {208979, 79433, "2.84,2.70,3.23,4.06,4.59,4.44,"},
};
#endif

const DecodedBufferExpectations kSfxWaveExpectations[] = {
    {0, 23219, "-1.23,-0.87,0.47,1.85,1.88,0.29,"},
    {23219, 23219, "0.75,1.10,2.43,3.78,3.53,1.93,"},
    {46439, 23219, "1.27,1.56,2.83,4.13,3.87,2.23,"},
};

const DecodedBufferExpectations kFourChannelWaveExpectations[] = {
    {0, 11609, "-1.68,1.68,0.89,-3.45,1.52,1.15,"},
    {11609, 11609, "43.26,9.06,18.27,35.98,19.45,7.46,"},
    {23219, 11609, "36.37,9.45,16.04,27.67,18.81,10.15,"},
};

const DecodedBufferExpectations kSfxOggExpectations[] = {
    {0, 13061, "-0.33,1.25,2.86,3.26,2.09,0.14,"},
    {13061, 23219, "-2.79,-2.42,-1.06,0.33,0.93,-0.64,"},
    {36281, 23219, "-1.19,-0.80,0.57,1.97,2.08,0.51,"},
};

const DecodedBufferExpectations kBearOgvExpectations[] = {
    {0, 13061, "-1.25,0.10,2.11,2.29,1.50,-0.68,"},
    {13061, 23219, "-1.80,-1.41,-0.13,1.30,1.65,0.01,"},
    {36281, 23219, "-1.43,-1.25,0.11,1.29,1.86,0.14,"},
};

const DecoderTestData kFFmpegTests[] = {
#if defined(USE_PROPRIETARY_CODECS)
    {FFMPEG, kCodecMP3, "sfx.mp3", kSfxMp3Expectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {FFMPEG, kCodecAAC, "sfx.adts", kSfxAdtsExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
#endif
#if defined(OS_CHROMEOS)
    {FFMPEG, kCodecFLAC, "sfx.flac", kSfxFlacExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
#endif
    {FFMPEG, kCodecPCM, "sfx_f32le.wav", kSfxWaveExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    {FFMPEG, kCodecPCM, "4ch.wav", kFourChannelWaveExpectations, 0, 44100,
     CHANNEL_LAYOUT_QUAD},
    {FFMPEG, kCodecVorbis, "sfx.ogg", kSfxOggExpectations, 0, 44100,
     CHANNEL_LAYOUT_MONO},
    // Note: bear.ogv is incorrectly muxed such that valid samples are given
    // negative timestamps, this marks them for discard per the ogg vorbis spec.
    {FFMPEG, kCodecVorbis, "bear.ogv", kBearOgvExpectations, -704, 44100,
     CHANNEL_LAYOUT_STEREO},
};

// Dummy data for behavioral tests.
const DecoderTestData kFFmpegBehavioralTest[] = {
    {FFMPEG, kUnknownAudioCodec, "", NULL, 0, 0, CHANNEL_LAYOUT_NONE},
};

INSTANTIATE_TEST_CASE_P(FFmpegAudioDecoderTest,
                        AudioDecoderTest,
                        testing::ValuesIn(kFFmpegTests));
INSTANTIATE_TEST_CASE_P(FFmpegAudioDecoderBehavioralTest,
                        FFmpegAudioDecoderBehavioralTest,
                        testing::ValuesIn(kFFmpegBehavioralTest));

}  // namespace media
