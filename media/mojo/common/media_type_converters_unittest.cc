// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/common/media_type_converters.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "media/base/audio_buffer.h"
#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/encryption_scheme.h"
#include "media/base/media_util.h"
#include "media/base/sample_format.h"
#include "media/base/test_helpers.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

namespace {

void CompareBytes(uint8_t* original_data, uint8_t* result_data, size_t length) {
  EXPECT_GT(length, 0u);
  EXPECT_EQ(memcmp(original_data, result_data, length), 0);
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
  buffer->set_discard_padding(
      DecoderBuffer::DiscardPadding(base::TimeDelta::FromMilliseconds(5),
                                    base::TimeDelta::FromMilliseconds(6)));

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
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
  EXPECT_EQ(buffer->discard_padding(), result->discard_padding());

  // Both |buffer| and |result| are not encrypted.
  EXPECT_FALSE(buffer->decrypt_config());
  EXPECT_FALSE(result->decrypt_config());
}

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_EOS) {
  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CreateEOSBuffer());

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
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
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
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
      base::MakeUnique<DecryptConfig>(kKeyId, kIv, subsamples));

  // Convert from and back.
  mojom::DecoderBufferPtr ptr(mojom::DecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer>>());

  // Compare.
  // Note: We intentionally do not serialize the data section of the
  // DecoderBuffer; no need to check the data here.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));

  // Test empty IV. This is used for clear buffer in an encrypted stream.
  buffer->set_decrypt_config(base::MakeUnique<DecryptConfig>(
      kKeyId, "", std::vector<SubsampleEntry>()));
  result =
      mojom::DecoderBuffer::From(buffer).To<scoped_refptr<DecoderBuffer>>();
  EXPECT_TRUE(buffer->decrypt_config()->Matches(*result->decrypt_config()));
  EXPECT_TRUE(buffer->decrypt_config()->iv().empty());
}

TEST(MediaTypeConvertersTest, ConvertCdmConfig) {
  CdmConfig config;
  config.allow_distinctive_identifier = true;
  config.allow_persistent_state = false;
  config.use_hw_secure_codecs = true;

  mojom::CdmConfigPtr ptr(mojom::CdmConfig::From(config));
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
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(buffer));
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
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(buffer));
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
  mojom::AudioBufferPtr ptr(mojom::AudioBuffer::From(buffer));
  scoped_refptr<AudioBuffer> result(ptr.To<scoped_refptr<AudioBuffer>>());

  // Compare.
  CompareAudioBuffers(kSampleFormatPlanarF32, buffer, result);
}

}  // namespace media
