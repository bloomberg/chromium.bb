// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_fec_group.h"

#include <algorithm>
#include <vector>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using base::StringPiece;

namespace net {

namespace {

// kData[] and kEntropyFlag[] are indexed by packet sequence numbers, which
// start at 1, so their first elements are dummy.
const char* kData[] = {
    "",  // dummy
    // kData[1] must be at least as long as every element of kData[], because
    // it is used to calculate kDataMaxLen.
    "abc12345678",
    "987defg",
    "ghi12345",
    "987jlkmno",
    "mno4567890",
    "789pqrstuvw",
};
// The maximum length of an element of kData.
const size_t kDataMaxLen = strlen(kData[1]);
// A suitable test data string, whose length is kDataMaxLen.
const char* kDataSingle = kData[1];

const bool kEntropyFlag[] = {
    false,  // dummy
    false,
    true,
    true,
    false,
    true,
    true,
};

}  // namespace

class QuicFecGroupTest : public ::testing::Test {
 protected:
  void RunTest(size_t num_packets, size_t lost_packet, bool out_of_order) {
    // kData[] and kEntropyFlag[] are indexed by packet sequence numbers, which
    // start at 1.
    DCHECK_GE(arraysize(kData), num_packets);
    scoped_ptr<char[]> redundancy(new char[kDataMaxLen]);
    for (size_t i = 0; i < kDataMaxLen; i++) {
      redundancy[i] = 0x00;
    }
    // XOR in the packets.
    for (size_t packet = 1; packet <= num_packets; ++packet) {
      for (size_t i = 0; i < kDataMaxLen; i++) {
        uint8 byte = i > strlen(kData[packet]) ? 0x00 : kData[packet][i];
        redundancy[i] = redundancy[i] ^ byte;
      }
    }

    QuicFecGroup group;

    // If we're out of order, send the FEC packet in the position of the
    // lost packet. Otherwise send all (non-missing) packets, then FEC.
    if (out_of_order) {
      // Update the FEC state for each non-lost packet.
      for (size_t packet = 1; packet <= num_packets; packet++) {
        if (packet == lost_packet) {
          ASSERT_FALSE(group.IsFinished());
          QuicFecData fec;
          fec.fec_group = 1u;
          fec.redundancy = StringPiece(redundancy.get(), kDataMaxLen);
          ASSERT_TRUE(
              group.UpdateFec(ENCRYPTION_FORWARD_SECURE, num_packets + 1, fec));
        } else {
          QuicPacketHeader header;
          header.packet_sequence_number = packet;
          header.entropy_flag = kEntropyFlag[packet];
          ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header,
                                   kData[packet]));
        }
        ASSERT_TRUE(group.CanRevive() == (packet == num_packets));
      }
    } else {
      // Update the FEC state for each non-lost packet.
      for (size_t packet = 1; packet <= num_packets; packet++) {
        if (packet == lost_packet) {
          continue;
        }

        QuicPacketHeader header;
        header.packet_sequence_number = packet;
        header.entropy_flag = kEntropyFlag[packet];
        ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header,
                                 kData[packet]));
        ASSERT_FALSE(group.CanRevive());
      }

      ASSERT_FALSE(group.IsFinished());
      // Attempt to revive the missing packet.
      QuicFecData fec;
      fec.fec_group = 1u;
      fec.redundancy = StringPiece(redundancy.get(), kDataMaxLen);

      ASSERT_TRUE(
          group.UpdateFec(ENCRYPTION_FORWARD_SECURE, num_packets + 1, fec));
    }
    QuicPacketHeader header;
    char recovered[kMaxPacketSize];
    ASSERT_TRUE(group.CanRevive());
    size_t len = group.Revive(&header, recovered, arraysize(recovered));
    ASSERT_NE(0u, len)
        << "Failed to revive packet " << lost_packet << " out of "
        << num_packets;
    EXPECT_EQ(lost_packet, header.packet_sequence_number)
        << "Failed to revive packet " << lost_packet << " out of "
        << num_packets;
    // Revived packets have an unknown entropy.
    EXPECT_FALSE(header.entropy_flag);
    ASSERT_GE(len, strlen(kData[lost_packet])) << "Incorrect length";
    for (size_t i = 0; i < strlen(kData[lost_packet]); i++) {
      EXPECT_EQ(kData[lost_packet][i], recovered[i]);
    }
    ASSERT_TRUE(group.IsFinished());
  }
};

TEST_F(QuicFecGroupTest, UpdateAndRevive) {
  RunTest(2, 1, false);
  RunTest(2, 2, false);

  RunTest(3, 1, false);
  RunTest(3, 2, false);
  RunTest(3, 3, false);
}

TEST_F(QuicFecGroupTest, UpdateAndReviveOutOfOrder) {
  RunTest(2, 1, true);
  RunTest(2, 2, true);

  RunTest(3, 1, true);
  RunTest(3, 2, true);
  RunTest(3, 3, true);
}

TEST_F(QuicFecGroupTest, UpdateFecIfReceivedPacketIsNotCovered) {
  char data1[] = "abc123";
  char redundancy[arraysize(data1)];
  for (size_t i = 0; i < arraysize(data1); i++) {
    redundancy[i] = data1[i];
  }

  QuicFecGroup group;

  QuicPacketHeader header;
  header.packet_sequence_number = 3;
  group.Update(ENCRYPTION_FORWARD_SECURE, header, data1);

  QuicFecData fec;
  fec.fec_group = 1u;
  fec.redundancy = redundancy;

  header.packet_sequence_number = 2;
  ASSERT_FALSE(group.UpdateFec(ENCRYPTION_FORWARD_SECURE, 2, fec));
}

TEST_F(QuicFecGroupTest, ProtectsPacketsBefore) {
  QuicPacketHeader header;
  header.packet_sequence_number = 3;

  QuicFecGroup group;
  ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header, kDataSingle));

  EXPECT_FALSE(group.ProtectsPacketsBefore(1));
  EXPECT_FALSE(group.ProtectsPacketsBefore(2));
  EXPECT_FALSE(group.ProtectsPacketsBefore(3));
  EXPECT_TRUE(group.ProtectsPacketsBefore(4));
  EXPECT_TRUE(group.ProtectsPacketsBefore(5));
  EXPECT_TRUE(group.ProtectsPacketsBefore(50));
}

TEST_F(QuicFecGroupTest, ProtectsPacketsBeforeWithSeveralPackets) {
  QuicPacketHeader header;
  header.packet_sequence_number = 3;

  QuicFecGroup group;
  ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header, kDataSingle));

  header.packet_sequence_number = 7;
  ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header, kDataSingle));

  header.packet_sequence_number = 5;
  ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header, kDataSingle));

  EXPECT_FALSE(group.ProtectsPacketsBefore(1));
  EXPECT_FALSE(group.ProtectsPacketsBefore(2));
  EXPECT_FALSE(group.ProtectsPacketsBefore(3));
  EXPECT_TRUE(group.ProtectsPacketsBefore(4));
  EXPECT_TRUE(group.ProtectsPacketsBefore(5));
  EXPECT_TRUE(group.ProtectsPacketsBefore(6));
  EXPECT_TRUE(group.ProtectsPacketsBefore(7));
  EXPECT_TRUE(group.ProtectsPacketsBefore(8));
  EXPECT_TRUE(group.ProtectsPacketsBefore(9));
  EXPECT_TRUE(group.ProtectsPacketsBefore(50));
}

TEST_F(QuicFecGroupTest, ProtectsPacketsBeforeWithFecData) {
  QuicFecData fec;
  fec.fec_group = 2u;
  fec.redundancy = kDataSingle;

  QuicFecGroup group;
  ASSERT_TRUE(group.UpdateFec(ENCRYPTION_FORWARD_SECURE, 3, fec));

  EXPECT_FALSE(group.ProtectsPacketsBefore(1));
  EXPECT_FALSE(group.ProtectsPacketsBefore(2));
  EXPECT_TRUE(group.ProtectsPacketsBefore(3));
  EXPECT_TRUE(group.ProtectsPacketsBefore(4));
  EXPECT_TRUE(group.ProtectsPacketsBefore(5));
  EXPECT_TRUE(group.ProtectsPacketsBefore(50));
}

TEST_F(QuicFecGroupTest, EffectiveEncryptionLevel) {
  QuicFecGroup group;
  EXPECT_EQ(NUM_ENCRYPTION_LEVELS, group.effective_encryption_level());

  QuicPacketHeader header;
  header.packet_sequence_number = 5;
  ASSERT_TRUE(group.Update(ENCRYPTION_INITIAL, header, kDataSingle));
  EXPECT_EQ(ENCRYPTION_INITIAL, group.effective_encryption_level());

  QuicFecData fec;
  fec.fec_group = 1u;
  fec.redundancy = kDataSingle;
  ASSERT_TRUE(group.UpdateFec(ENCRYPTION_FORWARD_SECURE, 7, fec));
  EXPECT_EQ(ENCRYPTION_INITIAL, group.effective_encryption_level());

  header.packet_sequence_number = 3;
  ASSERT_TRUE(group.Update(ENCRYPTION_NONE, header, kDataSingle));
  EXPECT_EQ(ENCRYPTION_NONE, group.effective_encryption_level());
}

}  // namespace net
