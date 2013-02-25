// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_entropy_manager.h"

#include <algorithm>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::make_pair;
using std::pair;
using std::vector;

namespace net {
namespace test {
namespace {

class QuicPacketEntropyManagerTest : public ::testing::Test {
 protected:
  QuicPacketEntropyManager entropy_manager_;
};

TEST_F(QuicPacketEntropyManagerTest, ReceivedPacketEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(7, 1));
  entropies.push_back(make_pair(2, 33));
  entropies.push_back(make_pair(5, 3));
  entropies.push_back(make_pair(8, 34));

  for (size_t i = 0; i < entropies.size(); ++i) {
    entropy_manager_.RecordReceivedPacketEntropyHash(entropies[i].first,
                                                     entropies[i].second);
  }

  sort(entropies.begin(), entropies.end());

  QuicPacketEntropyHash hash = 0;
  size_t index = 0;
  for (size_t i = 1; i <= (*entropies.rbegin()).first; ++i) {
    if (entropies[index].first == i) {
      hash ^= entropies[index].second;
      ++index;
    }
    EXPECT_EQ(hash, entropy_manager_.ReceivedEntropyHash(i));
  }
};

TEST_F(QuicPacketEntropyManagerTest, EntropyHashBelowLeastObserved) {
  EXPECT_EQ(0, entropy_manager_.ReceivedEntropyHash(0));
  EXPECT_EQ(0, entropy_manager_.ReceivedEntropyHash(9));
  entropy_manager_.RecordReceivedPacketEntropyHash(4, 5);
  EXPECT_EQ(0, entropy_manager_.ReceivedEntropyHash(3));
};

TEST_F(QuicPacketEntropyManagerTest, EntropyHashAboveLargesObserved) {
  EXPECT_EQ(0, entropy_manager_.ReceivedEntropyHash(0));
  EXPECT_EQ(0, entropy_manager_.ReceivedEntropyHash(9));
  entropy_manager_.RecordReceivedPacketEntropyHash(4, 5);
  EXPECT_EQ(0, entropy_manager_.ReceivedEntropyHash(3));
};

TEST_F(QuicPacketEntropyManagerTest, RecalculateReceivedEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(2, 1));
  entropies.push_back(make_pair(3, 33));
  entropies.push_back(make_pair(4, 3));
  entropies.push_back(make_pair(5, 34));
  entropies.push_back(make_pair(6, 29));

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < entropies.size(); ++i) {
    entropy_manager_.RecordReceivedPacketEntropyHash(entropies[i].first,
                                                     entropies[i].second);
    entropy_hash ^= entropies[i].second;
  }
  EXPECT_EQ(entropy_hash, entropy_manager_.ReceivedEntropyHash(6));

  // Now set the entropy hash up to 4 to be 100.
  entropy_hash ^= 100;
  for (size_t i = 0; i < 3; ++i) {
    entropy_hash ^= entropies[i].second;
  }
  entropy_manager_.RecalculateReceivedEntropyHash(4, 100);
  EXPECT_EQ(entropy_hash, entropy_manager_.ReceivedEntropyHash(6));
}

TEST_F(QuicPacketEntropyManagerTest, SentEntropyHash) {
  EXPECT_EQ(0, entropy_manager_.SentEntropyHash(0));

  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(2, 1));
  entropies.push_back(make_pair(3, 33));
  entropies.push_back(make_pair(4, 3));

  for (size_t i = 0; i < entropies.size(); ++i) {
    entropy_manager_.RecordSentPacketEntropyHash(entropies[i].first,
                                                 entropies[i].second);
  }

  QuicPacketEntropyHash hash = 0;
  for (size_t i = 0; i < entropies.size(); ++i) {
    hash ^= entropies[i].second;
    EXPECT_EQ(hash, entropy_manager_.SentEntropyHash(i + 1));
  }
}

TEST_F(QuicPacketEntropyManagerTest, IsValidEntropy) {
  QuicPacketEntropyHash entropies[10] =
      {12, 1, 33, 3, 32, 100, 28, 42, 22, 255};
  for (size_t i = 0; i < 10; ++i) {
    entropy_manager_.RecordSentPacketEntropyHash(i + 1, entropies[i]);
  }

  SequenceNumberSet missing_packets;
  missing_packets.insert(1);
  missing_packets.insert(4);
  missing_packets.insert(7);
  missing_packets.insert(8);

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < 10; ++i) {
    if (missing_packets.find(i + 1) == missing_packets.end()) {
      entropy_hash ^= entropies[i];
    }
  }

  EXPECT_TRUE(entropy_manager_.IsValidEntropy(10, missing_packets,
                                              entropy_hash));
}

}  // namespace
}  // namespace test
}  // namespace net
