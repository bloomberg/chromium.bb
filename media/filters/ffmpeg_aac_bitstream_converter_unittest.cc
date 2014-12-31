// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/ffmpeg/ffmpeg_common.h"
#include "media/filters/ffmpeg_aac_bitstream_converter.h"
#include "media/filters/ffmpeg_demuxer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {

// Class for testing the FFmpegAACBitstreamConverter.
class FFmpegAACBitstreamConverterTest : public testing::Test {
 protected:
  FFmpegAACBitstreamConverterTest() {
    // Minimal extra data header
    memset(context_header_, 0, sizeof(context_header_));

    // Set up reasonable aac context
    memset(&test_context_, 0, sizeof(AVCodecContext));
    test_context_.codec_id = CODEC_ID_AAC;
    test_context_.profile = FF_PROFILE_AAC_MAIN;
    test_context_.channels = 2;
    test_context_.extradata = context_header_;
    test_context_.extradata_size = sizeof(context_header_);
  }

  void CreatePacket(AVPacket* packet, const uint8* data, uint32 data_size) {
    // Create new packet sized of |data_size| from |data|.
    EXPECT_EQ(av_new_packet(packet, data_size), 0);
    memcpy(packet->data, data, data_size);
  }

  // Variable to hold valid dummy context for testing.
  AVCodecContext test_context_;

 private:
  uint8 context_header_[2];

  DISALLOW_COPY_AND_ASSIGN(FFmpegAACBitstreamConverterTest);
};

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_Success) {
  FFmpegAACBitstreamConverter converter(&test_context_);

  uint8 dummy_packet[1000];
  // Fill dummy packet with junk data. aac converter doesn't look into packet
  // data, just header, so can fill with whatever we want for test.
  for(size_t i = 0; i < sizeof(dummy_packet); i++) {
    dummy_packet[i] = i & 0xFF; // Repeated sequences of 0-255
  }

  ScopedAVPacket test_packet(new AVPacket());
  CreatePacket(test_packet.get(), dummy_packet,
               sizeof(dummy_packet));

  // Try out the actual conversion (should be successful and allocate new
  // packet and destroy the old one).
  EXPECT_TRUE(converter.ConvertPacket(test_packet.get()));

  // Check that a header was added and that packet data was preserved
  EXPECT_EQ(static_cast<long>(test_packet->size),
            static_cast<long>(sizeof(dummy_packet) +
                              FFmpegAACBitstreamConverter::kAdtsHeaderSize));
  EXPECT_EQ(memcmp(
      reinterpret_cast<void*>(test_packet->data +
                              FFmpegAACBitstreamConverter::kAdtsHeaderSize),
      reinterpret_cast<void*>(dummy_packet),
      sizeof(dummy_packet)), 0);
}

TEST_F(FFmpegAACBitstreamConverterTest, Conversion_FailureNullParams) {
  // Set up AVCConfigurationRecord to represent NULL data.
  AVCodecContext dummy_context;
  dummy_context.extradata = NULL;
  dummy_context.extradata_size = 0;
  FFmpegAACBitstreamConverter converter(&dummy_context);

  uint8 dummy_packet[1000] = {0};

  // Try out the actual conversion with NULL parameter.
  EXPECT_FALSE(converter.ConvertPacket(NULL));

  // Create new packet to test actual conversion.
  ScopedAVPacket test_packet(new AVPacket());
  CreatePacket(test_packet.get(), dummy_packet, sizeof(dummy_packet));

  // Try out the actual conversion. This should fail due to missing extradata.
  EXPECT_FALSE(converter.ConvertPacket(test_packet.get()));
}

}  // namespace media
