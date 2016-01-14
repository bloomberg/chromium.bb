// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/macros.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/test_helpers.h"
#include "media/base/video_frame.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

void CompareBytes(uint8_t* original_data, uint8_t* result_data, size_t length) {
  EXPECT_GT(length, 0u);
  EXPECT_EQ(memcmp(original_data, result_data, length), 0);
}

// Compare the actual video frame bytes (|rows| rows of |row|bytes| data),
// skipping any padding that may be in either frame.
void CompareRowBytes(uint8_t* original_data,
                     uint8_t* result_data,
                     size_t rows,
                     size_t row_bytes,
                     size_t original_stride,
                     size_t result_stride) {
  DCHECK_GE(original_stride, row_bytes);
  DCHECK_GE(result_stride, row_bytes);

  for (size_t i = 0; i < rows; ++i) {
    CompareBytes(original_data, result_data, row_bytes);
    original_data += original_stride;
    result_data += result_stride;
  }
}

void CompareAudioBuffers(SampleFormat sample_format,
                         const scoped_refptr<AudioBuffer>& original,
                         const scoped_refptr<AudioBuffer>& result) {
  EXPECT_EQ(original->frame_count(), result->frame_count());
  EXPECT_EQ(original->timestamp(), result->timestamp());
  EXPECT_EQ(original->duration(), result->duration());
  EXPECT_EQ(original->sample_rate(), result->sample_rate());
  EXPECT_EQ(original->channel_count(), result->channel_count());
  EXPECT_EQ(original->channel_layout(), result->channel_layout());
  EXPECT_EQ(original->end_of_stream(), result->end_of_stream());

  // Compare bytes in buffer.
  int bytes_per_channel =
      original->frame_count() * SampleFormatToBytesPerChannel(sample_format);
  if (IsPlanar(sample_format)) {
    for (int i = 0; i < original->channel_count(); ++i) {
      CompareBytes(original->channel_data()[i], result->channel_data()[i],
                   bytes_per_channel);
    }
    return;
  }

  DCHECK(IsInterleaved(sample_format)) << sample_format;
  CompareBytes(original->channel_data()[0], result->channel_data()[0],
               bytes_per_channel * original->channel_count());
}

void CompareVideoPlane(size_t plane,
                       const scoped_refptr<VideoFrame>& original,
                       const scoped_refptr<VideoFrame>& result) {
  EXPECT_EQ(original->stride(plane), result->stride(plane));
  EXPECT_EQ(original->row_bytes(plane), result->row_bytes(plane));
  EXPECT_EQ(original->rows(plane), result->rows(plane));
  CompareRowBytes(original->data(plane), result->data(plane),
                  original->rows(plane), original->row_bytes(plane),
                  original->stride(plane), result->stride(plane));
}

void CompareVideoFrames(const scoped_refptr<VideoFrame>& original,
                        const scoped_refptr<VideoFrame>& result) {
  if (original->metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM)) {
    EXPECT_TRUE(
        result->metadata()->IsTrue(media::VideoFrameMetadata::END_OF_STREAM));
    return;
  }

  EXPECT_EQ(original->format(), result->format());
  EXPECT_EQ(original->coded_size().height(), result->coded_size().height());
  EXPECT_EQ(original->coded_size().width(), result->coded_size().width());
  EXPECT_EQ(original->visible_rect().height(), result->visible_rect().height());
  EXPECT_EQ(original->visible_rect().width(), result->visible_rect().width());
  EXPECT_EQ(original->natural_size().height(), result->natural_size().height());
  EXPECT_EQ(original->natural_size().width(), result->natural_size().width());

  CompareVideoPlane(media::VideoFrame::kYPlane, original, result);
  CompareVideoPlane(media::VideoFrame::kUPlane, original, result);
  CompareVideoPlane(media::VideoFrame::kVPlane, original, result);
}

}  // namespace

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_Normal) {
  const uint8_t kData[] = "hello, world";
  const uint8_t kSideData[] = "sideshow bob";
  const size_t kDataSize = arraysize(kData);
  const size_t kSideDataSize = arraysize(kSideData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize,
      reinterpret_cast<const uint8_t*>(&kSideData), kSideDataSize));
  buffer->set_timestamp(base::TimeDelta::FromMilliseconds(123));
  buffer->set_duration(base::TimeDelta::FromMilliseconds(456));
  buffer->set_splice_timestamp(base::TimeDelta::FromMilliseconds(200));
  buffer->set_discard_padding(
      DecoderBuffer::DiscardPadding(base::TimeDelta::FromMilliseconds(5),
                                    base::TimeDelta::FromMilliseconds(6)));

  // Convert from and back.
  interfaces::DecoderBufferPtr ptr(interfaces::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_EQ(kSideDataSize, result->side_data_size());
  EXPECT_EQ(0, memcmp(result->side_data(), kSideData, kSideDataSize));
  EXPECT_EQ(buffer->timestamp(), result->timestamp());
  EXPECT_EQ(buffer->duration(), result->duration());
  EXPECT_EQ(buffer->is_key_frame(), result->is_key_frame());
  EXPECT_EQ(buffer->splice_timestamp(), result->splice_timestamp());
  EXPECT_EQ(buffer->discard_padding(), result->discard_padding());

  // Both |buffer| and |result| are not encrypted.
  EXPECT_FALSE(buffer->decrypt_config());
  EXPECT_FALSE(result->decrypt_config());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS) {
  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());

  // Convert from and back.
  interfaces::DecoderBufferPtr ptr(interfaces::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_KeyFrame) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_is_key_frame(true);
  EXPECT_TRUE(buffer->is_key_frame());

  // Convert from and back.
  interfaces::DecoderBufferPtr ptr(interfaces::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_TRUE(result->is_key_frame());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EncryptedBuffer) {
  const uint8_t kData[] = "hello, world";
  const size_t kDataSize = arraysize(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8_t*>(&kData), kDataSize));
  buffer->set_decrypt_config(
      make_scoped_ptr(new DecryptConfig(kKeyId, kIv, subsamples)));

  // Convert from and back.
  interfaces::DecoderBufferPtr ptr(interfaces::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));

  // Test empty IV. This is used for clear buffer in an encrypted stream.
  buffer->set_decrypt_config(make_scoped_ptr(
      new DecryptConfig(kKeyId, "", std::vector<SubsampleEntry>())));
  result = interfaces::DecoderBuffer::From(buffer)
               .To<scoped_refptr<DecoderBuffer>>();
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));
  EXPECT_TRUE(buffer->decrypt_config()->iv().empty());
}

// TODO(tim): Check other properties.

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig_Normal) {
  const uint8_t kExtraData[] = "config extra data";
  const std::vector<uint8_t> kExtraDataVector(
      &kExtraData[0], &kExtraData[0] + arraysize(kExtraData));

  AudioDecoderConfig config;
  config.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                    kExtraDataVector, false, base::TimeDelta(), 0);
  interfaces::AudioDecoderConfigPtr ptr(
      interfaces::AudioDecoderConfig::From(config));
  AudioDecoderConfig result(ptr.To<AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig_EmptyExtraData) {
  AudioDecoderConfig config;
  config.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                    EmptyExtraData(), false, base::TimeDelta(), 0);
  interfaces::AudioDecoderConfigPtr ptr(
      interfaces::AudioDecoderConfig::From(config));
  AudioDecoderConfig result(ptr.To<AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig_Encrypted) {
  AudioDecoderConfig config;
  config.Initialize(kCodecAAC, kSampleFormatU8, CHANNEL_LAYOUT_SURROUND, 48000,
                    EmptyExtraData(),
                    true,  // Is encrypted.
                    base::TimeDelta(), 0);
  interfaces::AudioDecoderConfigPtr ptr(
      interfaces::AudioDecoderConfig::From(config));
  AudioDecoderConfig result(ptr.To<AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

TEST(MediaTypeConvertersTest, ConvertCdmConfig) {
  CdmConfig config;
  config.allow_distinctive_identifier = true;
  config.allow_persistent_state = false;
  config.use_hw_secure_codecs = true;

  interfaces::CdmConfigPtr ptr(interfaces::CdmConfig::From(config));
  CdmConfig result(ptr.To<CdmConfig>());

  EXPECT_EQ(config.allow_distinctive_identifier,
            result.allow_distinctive_identifier);
  EXPECT_EQ(config.allow_persistent_state, result.allow_persistent_state);
  EXPECT_EQ(config.use_hw_secure_codecs, result.use_hw_secure_codecs);
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_EOS) {
  // Original.
  scoped_refptr<AudioBuffer> buffer(AudioBuffer::CreateEOSBuffer());

  // Convert to and back.
  interfaces::AudioBufferPtr ptr(interfaces::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  EXPECT_TRUE(result->end_of_stream());
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_MONO) {
  // Original.
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_MONO;
  const int kSampleRate = 48000;
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<uint8_t>(
      kSampleFormatU8, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 1, 1,
      kSampleRate / 100, base::TimeDelta());

  // Convert to and back.
  interfaces::AudioBufferPtr ptr(interfaces::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatU8, buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertAudioBuffer_FLOAT) {
  // Original.
  const ChannelLayout kChannelLayout = CHANNEL_LAYOUT_4_0;
  const int kSampleRate = 48000;
  const base::TimeDelta start_time = base::TimeDelta::FromSecondsD(1000.0);
  scoped_refptr<AudioBuffer> buffer = MakeAudioBuffer<float>(
      kSampleFormatPlanarF32, kChannelLayout,
      ChannelLayoutToChannelCount(kChannelLayout), kSampleRate, 0.0f, 1.0f,
      kSampleRate / 10, start_time);
  // Convert to and back.
  interfaces::AudioBufferPtr ptr(interfaces::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatPlanarF32, buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertVideoFrame_EOS) {
  // Original.
  scoped_refptr<VideoFrame> buffer(VideoFrame::CreateEOSFrame());

  // Convert to and back.
  interfaces::VideoFramePtr ptr(interfaces::VideoFrame::From(buffer));
  scoped_refptr<VideoFrame> result(ptr.To<scoped_refptr<VideoFrame>>());

  // Compare.
  CompareVideoFrames(buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertVideoFrame_BlackFrame) {
  // Original.
  scoped_refptr<VideoFrame> buffer(
      VideoFrame::CreateBlackFrame(gfx::Size(100, 100)));

  // Convert to and back.
  interfaces::VideoFramePtr ptr(interfaces::VideoFrame::From(buffer));
  scoped_refptr<VideoFrame> result(ptr.To<scoped_refptr<VideoFrame>>());

  // Compare.
  CompareVideoFrames(buffer, result);
}

TEST(MediaTypeConvertersTest, ConvertVideoFrame_ColorFrame) {
  // Original.
  scoped_refptr<VideoFrame> buffer(VideoFrame::CreateColorFrame(
      gfx::Size(50, 100), 255, 128, 128, base::TimeDelta::FromSeconds(26)));

  // Convert to and back.
  interfaces::VideoFramePtr ptr(interfaces::VideoFrame::From(buffer));
  scoped_refptr<VideoFrame> result(ptr.To<scoped_refptr<VideoFrame>>());

  // Compare.
  CompareVideoFrames(buffer, result);
}

}  // namespace media
