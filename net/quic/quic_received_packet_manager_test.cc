// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_received_packet_manager.h"

#include <algorithm>
#include <vector>

#include "net/quic/quic_connection_stats.h"
#include "net/quic/test_tools/quic_received_packet_manager_peer.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::make_pair;
using std::pair;
using std::vector;

namespace net {
namespace test {
namespace {

class QuicReceivedPacketManagerTest : public ::testing::Test {
 protected:
  QuicReceivedPacketManagerTest() : received_manager_(kTCP, &stats_) { }

  void RecordPacketReceipt(QuicPacketSequenceNumber sequence_number,
                           QuicPacketEntropyHash entropy_hash) {
    RecordPacketReceipt(sequence_number, entropy_hash, QuicTime::Zero());
  }

  void RecordPacketReceipt(QuicPacketSequenceNumber sequence_number,
                           QuicPacketEntropyHash entropy_hash,
                           QuicTime receipt_time) {
    QuicPacketHeader header;
    header.packet_sequence_number = sequence_number;
    header.entropy_hash = entropy_hash;
    received_manager_.RecordPacketReceived(0u, header, receipt_time);
  }

  QuicConnectionStats stats_;
  QuicReceivedPacketManager received_manager_;
};

TEST_F(QuicReceivedPacketManagerTest, ReceivedPacketEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(7, 1));
  entropies.push_back(make_pair(2, 33));
  entropies.push_back(make_pair(5, 3));
  entropies.push_back(make_pair(8, 34));

  for (size_t i = 0; i < entropies.size(); ++i) {
    RecordPacketReceipt(entropies[i].first, entropies[i].second);
  }

  sort(entropies.begin(), entropies.end());

  QuicPacketEntropyHash hash = 0;
  size_t index = 0;
  for (size_t i = 1; i <= (*entropies.rbegin()).first; ++i) {
    if (entropies[index].first == i) {
      hash ^= entropies[index].second;
      ++index;
    }
    EXPECT_EQ(hash, received_manager_.EntropyHash(i));
  }
  // Reorder by 5 when 2 is received after 7.
  EXPECT_EQ(5u, stats_.max_sequence_reordering);
  EXPECT_EQ(0u, stats_.max_time_reordering_us);
  EXPECT_EQ(2u, stats_.packets_reordered);
}

TEST_F(QuicReceivedPacketManagerTest, EntropyHashBelowLeastObserved) {
  EXPECT_EQ(0, received_manager_.EntropyHash(0));
  RecordPacketReceipt(4, 5);
  EXPECT_EQ(0, received_manager_.EntropyHash(3));
}

TEST_F(QuicReceivedPacketManagerTest, EntropyHashAboveLargestObserved) {
  EXPECT_EQ(0, received_manager_.EntropyHash(0));
  RecordPacketReceipt(4, 5);
  EXPECT_EQ(0, received_manager_.EntropyHash(3));
}

TEST_F(QuicReceivedPacketManagerTest, RecalculateEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(2, 1));
  entropies.push_back(make_pair(3, 33));
  entropies.push_back(make_pair(4, 3));
  entropies.push_back(make_pair(5, 34));
  entropies.push_back(make_pair(6, 29));

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < entropies.size(); ++i) {
    RecordPacketReceipt(entropies[i].first, entropies[i].second);
    entropy_hash ^= entropies[i].second;
  }
  EXPECT_EQ(entropy_hash, received_manager_.EntropyHash(6));

  // Now set the entropy hash up to 4 to be 100.
  entropy_hash ^= 100;
  for (size_t i = 0; i < 3; ++i) {
    entropy_hash ^= entropies[i].second;
  }
  QuicReceivedPacketManagerPeer::RecalculateEntropyHash(
      &received_manager_, 4, 100);
  EXPECT_EQ(entropy_hash, received_manager_.EntropyHash(6));

  QuicReceivedPacketManagerPeer::RecalculateEntropyHash(
      &received_manager_, 1, 50);
  EXPECT_EQ(entropy_hash, received_manager_.EntropyHash(6));

  // No reordering.
  EXPECT_EQ(0u, stats_.max_sequence_reordering);
  EXPECT_EQ(0u, stats_.max_time_reordering_us);
  EXPECT_EQ(0u, stats_.packets_reordered);
}

TEST_F(QuicReceivedPacketManagerTest, DontWaitForPacketsBefore) {
  QuicPacketHeader header;
  header.packet_sequence_number = 2u;
  received_manager_.RecordPacketReceived(0u, header, QuicTime::Zero());
  header.packet_sequence_number = 7u;
  received_manager_.RecordPacketReceived(0u, header, QuicTime::Zero());
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(3u));
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(6u));
  EXPECT_TRUE(QuicReceivedPacketManagerPeer::DontWaitForPacketsBefore(
      &received_manager_, 4));
  EXPECT_FALSE(received_manager_.IsAwaitingPacket(3u));
  EXPECT_TRUE(received_manager_.IsAwaitingPacket(6u));
}

TEST_F(QuicReceivedPacketManagerTest, UpdateReceivedPacketInfo) {
  QuicPacketHeader header;
  header.packet_sequence_number = 2u;
  QuicTime two_ms = QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(2));
  received_manager_.RecordPacketReceived(0u, header, two_ms);

  ReceivedPacketInfo info;
  received_manager_.UpdateReceivedPacketInfo(&info, QuicTime::Zero());
  // When UpdateReceivedPacketInfo with a time earlier than the time of the
  // largest observed packet, make sure that the delta is 0, not negative.
  EXPECT_EQ(QuicTime::Delta::Zero(), info.delta_time_largest_observed);

  QuicTime four_ms = QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(4));
  received_manager_.UpdateReceivedPacketInfo(&info, four_ms);
  // When UpdateReceivedPacketInfo after not having received a new packet,
  // the delta should still be accurate.
  EXPECT_EQ(QuicTime::Delta::FromMilliseconds(2),
            info.delta_time_largest_observed);
}

TEST_F(QuicReceivedPacketManagerTest, UpdateReceivedConnectionStats) {
  RecordPacketReceipt(1, 0);
  RecordPacketReceipt(6, 0);
  RecordPacketReceipt(
      2, 0, QuicTime::Zero().Add(QuicTime::Delta::FromMilliseconds(1)));

  EXPECT_EQ(4u, stats_.max_sequence_reordering);
  EXPECT_EQ(1000u, stats_.max_time_reordering_us);
  EXPECT_EQ(1u, stats_.packets_reordered);
}

}  // namespace
}  // namespace test
}  // namespace net
