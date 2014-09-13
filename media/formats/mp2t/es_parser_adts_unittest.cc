// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "media/base/buffers.h"
#include "media/base/stream_parser_buffer.h"
#include "media/formats/mp2t/es_parser_adts.h"
#include "media/formats/mp2t/es_parser_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace media {
class AudioDecoderConfig;

namespace mp2t {

class EsParserAdtsTest : public EsParserTestBase,
                         public testing::Test {
 public:
  EsParserAdtsTest();
  virtual ~EsParserAdtsTest() {}

 protected:
  bool Process(const std::vector<Packet>& pes_packets, bool force_timing);

 private:
  DISALLOW_COPY_AND_ASSIGN(EsParserAdtsTest);
};

EsParserAdtsTest::EsParserAdtsTest() {
}

bool EsParserAdtsTest::Process(
    const std::vector<Packet>& pes_packets,
    bool force_timing) {
  EsParserAdts es_parser(
      base::Bind(&EsParserAdtsTest::NewAudioConfig, base::Unretained(this)),
      base::Bind(&EsParserAdtsTest::EmitBuffer, base::Unretained(this)),
      false);
  return ProcessPesPackets(&es_parser, pes_packets, force_timing);
}

TEST_F(EsParserAdtsTest, NoInitialPts) {
  LoadStream("bear.adts");
  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);
  EXPECT_FALSE(Process(pes_packets, false));
  EXPECT_EQ(0u, buffer_count_);
}

TEST_F(EsParserAdtsTest, SinglePts) {
  LoadStream("bear.adts");

  std::vector<Packet> pes_packets = GenerateFixedSizePesPacket(512);
  pes_packets.front().pts = base::TimeDelta::FromSeconds(10);

  EXPECT_TRUE(Process(pes_packets, false));
  EXPECT_EQ(1u, config_count_);
  EXPECT_EQ(45u, buffer_count_);
}

}  // namespace mp2t
}  // namespace media

