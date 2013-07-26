// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_received_entropy_manager.h"

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

class QuicReceivedEntropyManagerTest : public ::testing::Test {
 protected:
  QuicReceivedEntropyManager entropy_manager_;
};

TEST_F(QuicReceivedEntropyManagerTest, ReceivedPacketEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(7, 1));
  entropies.push_back(make_pair(2, 33));
  entropies.push_back(make_pair(5, 3));
  entropies.push_back(make_pair(8, 34));

  for (size_t i = 0; i < entropies.size(); ++i) {
    entropy_manager_.RecordPacketEntropyHash(entropies[i].first,
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
    EXPECT_EQ(hash, entropy_manager_.EntropyHash(i));
  }
}

TEST_F(QuicReceivedEntropyManagerTest, EntropyHashBelowLeastObserved) {
  EXPECT_EQ(0, entropy_manager_.EntropyHash(0));
  entropy_manager_.RecordPacketEntropyHash(4, 5);
  EXPECT_EQ(0, entropy_manager_.EntropyHash(3));
}

TEST_F(QuicReceivedEntropyManagerTest, EntropyHashAboveLargestObserved) {
  EXPECT_EQ(0, entropy_manager_.EntropyHash(0));
  entropy_manager_.RecordPacketEntropyHash(4, 5);
  EXPECT_EQ(0, entropy_manager_.EntropyHash(3));
}

TEST_F(QuicReceivedEntropyManagerTest, RecalculateEntropyHash) {
  vector<pair<QuicPacketSequenceNumber, QuicPacketEntropyHash> > entropies;
  entropies.push_back(make_pair(1, 12));
  entropies.push_back(make_pair(2, 1));
  entropies.push_back(make_pair(3, 33));
  entropies.push_back(make_pair(4, 3));
  entropies.push_back(make_pair(5, 34));
  entropies.push_back(make_pair(6, 29));

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < entropies.size(); ++i) {
    entropy_manager_.RecordPacketEntropyHash(entropies[i].first,
                                             entropies[i].second);
    entropy_hash ^= entropies[i].second;
  }
  EXPECT_EQ(entropy_hash, entropy_manager_.EntropyHash(6));

  // Now set the entropy hash up to 4 to be 100.
  entropy_hash ^= 100;
  for (size_t i = 0; i < 3; ++i) {
    entropy_hash ^= entropies[i].second;
  }
  entropy_manager_.RecalculateEntropyHash(4, 100);
  EXPECT_EQ(entropy_hash, entropy_manager_.EntropyHash(6));

  // Ensure it doesn't change with an old received sequence number or entropy.
  entropy_manager_.RecordPacketEntropyHash(1, 50);
  EXPECT_EQ(entropy_hash, entropy_manager_.EntropyHash(6));

  entropy_manager_.RecalculateEntropyHash(1, 50);
  EXPECT_EQ(entropy_hash, entropy_manager_.EntropyHash(6));
}

}  // namespace
}  // namespace test
}  // namespace net
