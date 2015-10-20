// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include "media/base/audio_decoder_config.h"
#include "media/base/cdm_config.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer_Normal) {
  const uint8 kData[] = "hello, world";
  const uint8 kSideData[] = "sideshow bob";
  const int kDataSize = arraysize(kData);
  const int kSideDataSize = arraysize(kSideData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize,
      reinterpret_cast<const uint8*>(&kSideData), kSideDataSize));
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
  const uint8 kData[] = "hello, world";
  const int kDataSize = arraysize(kData);

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize));
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
  const uint8 kData[] = "hello, world";
  const int kDataSize = arraysize(kData);
  const char kKeyId[] = "00112233445566778899aabbccddeeff";
  const char kIv[] = "0123456789abcdef";

  std::vector<SubsampleEntry> subsamples;
  subsamples.push_back(SubsampleEntry(10, 20));
  subsamples.push_back(SubsampleEntry(30, 40));
  subsamples.push_back(SubsampleEntry(50, 60));

  // Original.
  scoped_refptr<DecoderBuffer> buffer(DecoderBuffer::CopyFrom(
      reinterpret_cast<const uint8*>(&kData), kDataSize));
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

}  // namespace media
