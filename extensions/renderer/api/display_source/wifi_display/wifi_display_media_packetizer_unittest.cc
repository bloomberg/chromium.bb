// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"
#include "extensions/renderer/api/display_source/wifi_display/wifi_display_elementary_stream_packetizer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {

namespace {

namespace pes {
const unsigned kDtsFlag = 0x0040u;
const unsigned kMarkerFlag = 0x8000u;
const unsigned kPtsFlag = 0x0080u;
const size_t kUnitDataAlignment = sizeof(uint32_t);
}

uint64_t ParseTimeStamp(const uint8_t ts_bytes[5], uint8_t pts_dts_indicator) {
  EXPECT_EQ(pts_dts_indicator, (ts_bytes[0] & 0xF0u) >> 4);
  EXPECT_EQ(0x01u, ts_bytes[0] & 0x01u);
  EXPECT_EQ(0x01u, ts_bytes[2] & 0x01u);
  EXPECT_EQ(0x01u, ts_bytes[4] & 0x01u);
  uint64_t ts = 0u;
  ts = (ts_bytes[0] & 0x0Eu) >> 1;
  ts = (ts << 8) | ts_bytes[1];
  ts = (ts << 7) | ((ts_bytes[2] & 0xFEu) >> 1);
  ts = (ts << 8) | ts_bytes[3];
  ts = (ts << 7) | ((ts_bytes[4] & 0xFEu) >> 1);
  return ts;
}

class WiFiDisplayElementaryStreamUnitPacketizationTest
    : public testing::TestWithParam<
          testing::tuple<unsigned, base::TimeDelta, base::TimeDelta>> {
 protected:
  static base::TimeTicks SumOrNull(const base::TimeTicks& base,
                                   const base::TimeDelta& delta) {
    return delta.is_max() ? base::TimeTicks() : base + delta;
  }

  WiFiDisplayElementaryStreamUnitPacketizationTest()
      : unit_(testing::get<0>(GetParam())),
        now_(base::TimeTicks::Now()),
        dts_(SumOrNull(now_, testing::get<1>(GetParam()))),
        pts_(SumOrNull(now_, testing::get<2>(GetParam()))) {}

  void CheckElementaryStreamPacketHeader(
      const WiFiDisplayElementaryStreamPacket& packet,
      uint8_t stream_id) {
    base::BigEndianReader header_reader(
        reinterpret_cast<const char*>(packet.header().begin()),
        packet.header().size());
    uint8_t parsed_packet_start_code_prefix[3];
    EXPECT_TRUE(
        header_reader.ReadBytes(parsed_packet_start_code_prefix,
                                sizeof(parsed_packet_start_code_prefix)));
    EXPECT_EQ(0x00u, parsed_packet_start_code_prefix[0]);
    EXPECT_EQ(0x00u, parsed_packet_start_code_prefix[1]);
    EXPECT_EQ(0x01u, parsed_packet_start_code_prefix[2]);
    uint8_t parsed_stream_id;
    EXPECT_TRUE(header_reader.ReadU8(&parsed_stream_id));
    EXPECT_EQ(stream_id, parsed_stream_id);
    uint16_t parsed_packet_length;
    EXPECT_TRUE(header_reader.ReadU16(&parsed_packet_length));
    size_t packet_length = static_cast<size_t>(header_reader.remaining()) +
                           packet.unit_header().size() + packet.unit().size();
    if (packet_length >> 16)
      packet_length = 0u;
    EXPECT_EQ(packet_length, parsed_packet_length);
    uint16_t parsed_flags;
    EXPECT_TRUE(header_reader.ReadU16(&parsed_flags));
    EXPECT_EQ(
        0u, parsed_flags & ~(pes::kMarkerFlag | pes::kPtsFlag | pes::kDtsFlag));
    const bool parsed_pts_flag = (parsed_flags & pes::kPtsFlag) != 0u;
    const bool parsed_dts_flag = (parsed_flags & pes::kDtsFlag) != 0u;
    EXPECT_EQ(!pts_.is_null(), parsed_pts_flag);
    EXPECT_EQ(!pts_.is_null() && !dts_.is_null(), parsed_dts_flag);
    uint8_t parsed_header_length;
    EXPECT_TRUE(header_reader.ReadU8(&parsed_header_length));
    EXPECT_EQ(header_reader.remaining(), parsed_header_length);
    if (parsed_pts_flag) {
      uint8_t parsed_pts_bytes[5];
      EXPECT_TRUE(
          header_reader.ReadBytes(parsed_pts_bytes, sizeof(parsed_pts_bytes)));
      const uint64_t parsed_pts =
          ParseTimeStamp(parsed_pts_bytes, parsed_dts_flag ? 0x3u : 0x2u);
      if (parsed_dts_flag) {
        uint8_t parsed_dts_bytes[5];
        EXPECT_TRUE(header_reader.ReadBytes(parsed_dts_bytes,
                                            sizeof(parsed_dts_bytes)));
        const uint64_t parsed_dts = ParseTimeStamp(parsed_dts_bytes, 0x1u);
        EXPECT_EQ(
            static_cast<uint64_t>(90 * (pts_ - dts_).InMicroseconds() / 1000),
            (parsed_pts - parsed_dts) & UINT64_C(0x1FFFFFFFF));
      }
    }
    while (header_reader.remaining() > 0) {
      uint8_t parsed_stuffing_byte;
      EXPECT_TRUE(header_reader.ReadU8(&parsed_stuffing_byte));
      EXPECT_EQ(0xFFu, parsed_stuffing_byte);
    }
    EXPECT_EQ(0, header_reader.remaining());
  }

  void CheckElementaryStreamPacketUnitHeader(
      const WiFiDisplayElementaryStreamPacket& packet,
      const uint8_t* unit_header_data,
      size_t unit_header_size) {
    EXPECT_EQ(unit_header_data, packet.unit_header().begin());
    EXPECT_EQ(unit_header_size, packet.unit_header().size());
  }

  void CheckElementaryStreamPacketUnit(
      const WiFiDisplayElementaryStreamPacket& packet) {
    EXPECT_EQ(0u, (packet.header().size() + packet.unit_header().size()) %
                      pes::kUnitDataAlignment);
    EXPECT_EQ(unit_.data(), packet.unit().begin());
    EXPECT_EQ(unit_.size(), packet.unit().size());
  }

  enum { kVideoOnlyUnitSize = 0x8000u };  // Not exact. Be on the safe side.

  const std::vector<uint8_t> unit_;
  const base::TimeTicks now_;
  const base::TimeTicks dts_;
  const base::TimeTicks pts_;
};

TEST_P(WiFiDisplayElementaryStreamUnitPacketizationTest,
       EncodeToElementaryStreamPacket) {
  const size_t kMaxUnitHeaderSize = 4u;

  const uint8_t stream_id =
      unit_.size() >= kVideoOnlyUnitSize
          ? WiFiDisplayElementaryStreamPacketizer::kFirstVideoStreamId
          : WiFiDisplayElementaryStreamPacketizer::kFirstAudioStreamId;

  WiFiDisplayElementaryStreamPacketizer packetizer;
  uint8_t unit_header_data[kMaxUnitHeaderSize];
  for (size_t unit_header_size = 0u; unit_header_size <= kMaxUnitHeaderSize;
       ++unit_header_size) {
    WiFiDisplayElementaryStreamPacket packet =
        packetizer.EncodeElementaryStreamUnit(stream_id, unit_header_data,
                                              unit_header_size, unit_.data(),
                                              unit_.size(), pts_, dts_);
    CheckElementaryStreamPacketHeader(packet, stream_id);
    CheckElementaryStreamPacketUnitHeader(packet, unit_header_data,
                                          unit_header_size);
    CheckElementaryStreamPacketUnit(packet);
  }
}

INSTANTIATE_TEST_CASE_P(
    WiFiDisplayElementaryStreamUnitPacketizationTests,
    WiFiDisplayElementaryStreamUnitPacketizationTest,
    testing::Combine(testing::Values(123u, 180u, 0x10000u),
                     testing::Values(base::TimeDelta::Max(),
                                     base::TimeDelta::FromMicroseconds(0)),
                     testing::Values(base::TimeDelta::Max(),
                                     base::TimeDelta::FromMicroseconds(
                                         1000 * INT64_C(0x123456789) / 90))));

}  // namespace
}  // namespace extensions
