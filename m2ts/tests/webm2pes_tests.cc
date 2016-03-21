// Copyright (c) 2016 The WebM project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.
#include "m2ts/webm2pes.h"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <vector>

#include "gtest/gtest.h"

#include "common/file_util.h"
#include "common/libwebm_util.h"
#include "m2ts/vpxpes_parser.h"
#include "testing/test_util.h"

namespace {

class Webm2PesTests : public ::testing::Test {
 public:
  // Constants for validating known values from input data.
  const std::uint8_t kMinVideoStreamId = 0xE0;
  const std::uint8_t kMaxVideoStreamId = 0xEF;
  const int kPesHeaderSize = 6;
  const int kPesOptionalHeaderStartOffset = kPesHeaderSize;
  const int kPesOptionalHeaderSize = 9;
  const int kPesOptionalHeaderMarkerValue = 0x2;
  const int kWebm2PesOptHeaderRemainingSize = 6;
  const int kBcmvHeaderSize = 10;

  Webm2PesTests() = default;
  ~Webm2PesTests() = default;

  void CreateAndLoadTestInput() {
    libwebm::Webm2Pes converter(input_file_name_, temp_file_name_.name());
    ASSERT_TRUE(converter.ConvertToFile());
    ASSERT_TRUE(parser_.Open(pes_file_name()));
  }

  bool VerifyPacketStartCode(const libwebm::VpxPesParser::PesHeader& header) {
    // PES packets all start with the byte sequence 0x0 0x0 0x1.
    if (header.start_code[0] != 0 || header.start_code[1] != 0 ||
        header.start_code[2] != 1) {
      return false;
    }
    return true;
  }

  const std::string& pes_file_name() const { return temp_file_name_.name(); }
  libwebm::VpxPesParser* parser() { return &parser_; }

 private:
  const libwebm::TempFileDeleter temp_file_name_;
  const std::string input_file_name_ =
      test::GetTestFilePath("bbb_480p_vp9_opus_1second.webm");
  libwebm::VpxPesParser parser_;
};

TEST_F(Webm2PesTests, CreatePesFile) { CreateAndLoadTestInput(); }

TEST_F(Webm2PesTests, CanParseFirstPacket) {
  CreateAndLoadTestInput();
  libwebm::VpxPesParser::PesHeader header;
  libwebm::VpxPesParser::VpxFrame frame;
  ASSERT_TRUE(parser()->ParseNextPacket(&header, &frame));
  EXPECT_TRUE(VerifyPacketStartCode(header));

  const std::size_t kPesOptionalHeaderLength = 9;
  const std::size_t kFirstFrameLength = 83;
  const std::size_t kPesPayloadLength =
      kPesOptionalHeaderLength + kFirstFrameLength;
  EXPECT_EQ(kPesPayloadLength, header.packet_length);

  EXPECT_GE(header.stream_id, kMinVideoStreamId);
  EXPECT_LE(header.stream_id, kMaxVideoStreamId);

  // Test PesOptionalHeader values.
  EXPECT_EQ(kPesOptionalHeaderMarkerValue, header.opt_header.marker);
  EXPECT_EQ(kWebm2PesOptHeaderRemainingSize, header.opt_header.remaining_size);
  EXPECT_EQ(0, header.opt_header.scrambling);
  EXPECT_EQ(0, header.opt_header.priority);
  EXPECT_EQ(0, header.opt_header.data_alignment);
  EXPECT_EQ(0, header.opt_header.copyright);
  EXPECT_EQ(0, header.opt_header.original);
  EXPECT_EQ(1, header.opt_header.has_pts);
  EXPECT_EQ(0, header.opt_header.has_dts);
  EXPECT_EQ(0, header.opt_header.unused_fields);

  // Test the BCMV header.
  const libwebm::VpxPesParser::BcmvHeader kFirstBcmvHeader(kFirstFrameLength);
  EXPECT_TRUE(header.bcmv_header.Valid());
  EXPECT_EQ(kFirstBcmvHeader, header.bcmv_header);

  // Parse the next packet to confirm correct parse and consumption of payload.
  EXPECT_TRUE(parser()->ParseNextPacket(&header, &frame));
}

}  // namespace

int main(int argc, char* argv[]) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
