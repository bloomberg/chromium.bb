// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/media_type_converters.h"

#include "media/base/audio_decoder_config.h"
#include "media/base/decoder_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

using media::DecoderBuffer;

namespace mojo {
namespace test {

TEST(MediaTypeConvertersTest, ConvertDecoderBuffer) {
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
  buffer->set_discard_padding(media::DecoderBuffer::DiscardPadding(
      base::TimeDelta::FromMilliseconds(5),
      base::TimeDelta::FromMilliseconds(6)));

  // Convert from and back.
  MediaDecoderBufferPtr ptr(MediaDecoderBuffer::From(buffer));
  scoped_refptr<DecoderBuffer> result(ptr.To<scoped_refptr<DecoderBuffer> >());

  // Compare.
  EXPECT_EQ(kDataSize, result->data_size());
  EXPECT_EQ(0, memcmp(result->data(), kData, kDataSize));
  EXPECT_EQ(kSideDataSize, result->side_data_size());
  EXPECT_EQ(0, memcmp(result->side_data(), kSideData, kSideDataSize));
  EXPECT_EQ(buffer->timestamp(), result->timestamp());
  EXPECT_EQ(buffer->duration(), result->duration());
  EXPECT_EQ(buffer->splice_timestamp(), result->splice_timestamp());
  EXPECT_EQ(buffer->discard_padding(), result->discard_padding());
}

// TODO(tim): Handle EOS, check other properties.

TEST(MediaTypeConvertersTest, ConvertAudioDecoderConfig) {
  const uint8 kExtraData[] = "config extra data";
  const int kExtraDataSize = arraysize(kExtraData);
  media::AudioDecoderConfig config;
  config.Initialize(media::kCodecAAC,
                    media::kSampleFormatU8,
                    media::CHANNEL_LAYOUT_SURROUND,
                    48000,
                    reinterpret_cast<const uint8*>(&kExtraData),
                    kExtraDataSize,
                    false,
                    false,
                    base::TimeDelta(),
                    0);
  AudioDecoderConfigPtr ptr(AudioDecoderConfig::From(config));
  media::AudioDecoderConfig result(ptr.To<media::AudioDecoderConfig>());
  EXPECT_TRUE(result.Matches(config));
}

}  // namespace test
}  // namespace mojo
