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
using std::string;

namespace net {

namespace {

// kData[] and kEntropyFlag[] are indexed by packet numbers, which
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
    // kData[] and kEntropyFlag[] are indexed by packet numbers, which
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
          header.packet_packet_number = packet;
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
        header.packet_packet_number = packet;
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
    EXPECT_EQ(lost_packet, header.packet_packet_number)
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
  header.packet_packet_number = 3;
  group.Update(ENCRYPTION_FORWARD_SECURE, header, data1);

  QuicFecData fec;
  fec.fec_group = 1u;
  fec.redundancy = redundancy;

  header.packet_packet_number = 2;
  ASSERT_FALSE(group.UpdateFec(ENCRYPTION_FORWARD_SECURE, 2, fec));
}

TEST_F(QuicFecGroupTest, ProtectsPacketsBefore) {
  QuicPacketHeader header;
  header.packet_packet_number = 3;

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
  header.packet_packet_number = 3;

  QuicFecGroup group;
  ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header, kDataSingle));

  header.packet_packet_number = 7;
  ASSERT_TRUE(group.Update(ENCRYPTION_FORWARD_SECURE, header, kDataSingle));

  header.packet_packet_number = 5;
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
  EXPECT_EQ(NUM_ENCRYPTION_LEVELS, group.EffectiveEncryptionLevel());

  QuicPacketHeader header;
  header.packet_packet_number = 5;
  ASSERT_TRUE(group.Update(ENCRYPTION_INITIAL, header, kDataSingle));
  EXPECT_EQ(ENCRYPTION_INITIAL, group.EffectiveEncryptionLevel());

  QuicFecData fec;
  fec.fec_group = 1u;
  fec.redundancy = kDataSingle;
  ASSERT_TRUE(group.UpdateFec(ENCRYPTION_FORWARD_SECURE, 7, fec));
  EXPECT_EQ(ENCRYPTION_INITIAL, group.EffectiveEncryptionLevel());

  header.packet_packet_number = 3;
  ASSERT_TRUE(group.Update(ENCRYPTION_NONE, header, kDataSingle));
  EXPECT_EQ(ENCRYPTION_NONE, group.EffectiveEncryptionLevel());
}

// Test the code assuming it is going to be operating in 128-bit chunks (which
// is something that can happen if it is compiled with full vectorization).
const QuicByteCount kWordSize = 128 / 8;

// A buffer which stores the data with the specified offset with respect to word
// alignment boundary.
class MisalignedBuffer {
 public:
  MisalignedBuffer(const string& original, size_t offset);

  char* buffer() { return buffer_; }
  size_t size() { return size_; }

  StringPiece AsStringPiece() { return StringPiece(buffer_, size_); }

 private:
  char* buffer_;
  size_t size_;

  scoped_ptr<char[]> allocation_;
};

MisalignedBuffer::MisalignedBuffer(const string& original, size_t offset) {
  CHECK_LT(offset, kWordSize);
  size_ = original.size();

  // Allocate aligned buffer two words larger than needed.
  const size_t aligned_buffer_size = size_ + 2 * kWordSize;
  allocation_.reset(new char[aligned_buffer_size]);
  char* aligned_buffer =
      allocation_.get() +
      (kWordSize - reinterpret_cast<uintptr_t>(allocation_.get()) % kWordSize);
  CHECK_EQ(0u, reinterpret_cast<uintptr_t>(aligned_buffer) % kWordSize);

  buffer_ = aligned_buffer + offset;
  CHECK_EQ(offset, reinterpret_cast<uintptr_t>(buffer_) % kWordSize);
  memcpy(buffer_, original.data(), size_);
}

// Checks whether XorBuffers works correctly with buffers aligned in various
// ways.
TEST(XorBuffersTest, XorBuffers) {
  const string longer_data =
      "Having to care about memory alignment can be incredibly frustrating.";
  const string shorter_data = "strict aliasing";

  // Compute the reference XOR using simpler slow way.
  string output_reference;
  for (size_t i = 0; i < longer_data.size(); i++) {
    char shorter_byte = i < shorter_data.size() ? shorter_data[i] : 0;
    output_reference.push_back(longer_data[i] ^ shorter_byte);
  }

  // Check whether XorBuffers works correctly for all possible misalignments.
  for (size_t offset_shorter = 0; offset_shorter < kWordSize;
       offset_shorter++) {
    for (size_t offset_longer = 0; offset_longer < kWordSize; offset_longer++) {
      // Prepare the misaligned buffer.
      MisalignedBuffer longer(longer_data, offset_longer);
      MisalignedBuffer shorter(shorter_data, offset_shorter);

      // XOR the buffers and compare the result with the reference.
      QuicFecGroup::XorBuffers(shorter.buffer(), shorter.size(),
                               longer.buffer());
      EXPECT_EQ(output_reference, longer.AsStringPiece());
    }
  }
}

}  // namespace net
