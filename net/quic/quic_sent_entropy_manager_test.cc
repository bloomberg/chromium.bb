// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_sent_entropy_manager.h"

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

class QuicSentEntropyManagerTest : public ::testing::Test {
 protected:
  QuicSentEntropyManager entropy_manager_;
};

TEST_F(QuicSentEntropyManagerTest, SentEntropyHash) {
  EXPECT_EQ(0, entropy_manager_.GetCumulativeEntropy(0));

  QuicPacketEntropyHash entropies[4] = {12, 1, 33, 3};
  for (size_t i = 0; i < arraysize(entropies); ++i) {
    entropy_manager_.RecordPacketEntropyHash(i + 1, entropies[i]);
  }

  QuicPacketEntropyHash hash = 0;
  for (size_t i = 0; i < arraysize(entropies); ++i) {
    hash ^= entropies[i];
    EXPECT_EQ(hash, entropy_manager_.GetCumulativeEntropy(i + 1));
  }
}

TEST_F(QuicSentEntropyManagerTest, IsValidEntropy) {
  QuicPacketEntropyHash entropies[10] =
      {12, 1, 33, 3, 32, 100, 28, 42, 22, 255};
  for (size_t i = 0; i < arraysize(entropies); ++i) {
    entropy_manager_.RecordPacketEntropyHash(i + 1, entropies[i]);
  }

  SequenceNumberSet missing_packets;
  missing_packets.insert(1);
  missing_packets.insert(4);
  missing_packets.insert(7);
  missing_packets.insert(8);

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < arraysize(entropies); ++i) {
    if (missing_packets.find(i + 1) == missing_packets.end()) {
      entropy_hash ^= entropies[i];
    }
  }

  EXPECT_TRUE(entropy_manager_.IsValidEntropy(10, missing_packets,
                                              entropy_hash));
}

TEST_F(QuicSentEntropyManagerTest, ClearEntropiesBefore) {
  QuicPacketEntropyHash entropies[10] =
      {12, 1, 33, 3, 32, 100, 28, 42, 22, 255};

  for (size_t i = 0; i < arraysize(entropies); ++i) {
    entropy_manager_.RecordPacketEntropyHash(i + 1, entropies[i]);
  }

  // Discard the first 5 entropies and ensure IsValidEntropy and EntropyHash
  // still return correct results.
  entropy_manager_.ClearEntropyBefore(5);

  SequenceNumberSet missing_packets;
  missing_packets.insert(7);
  missing_packets.insert(8);

  QuicPacketEntropyHash entropy_hash = 0;
  for (size_t i = 0; i < arraysize(entropies); ++i) {
    if (missing_packets.find(i + 1) == missing_packets.end()) {
      entropy_hash ^= entropies[i];
    }
  }
  EXPECT_TRUE(entropy_manager_.IsValidEntropy(10, missing_packets,
                                              entropy_hash));

  entropy_hash = 0;
  for (size_t i = 0; i < arraysize(entropies); ++i) {
    entropy_hash ^= entropies[i];
  }
  EXPECT_EQ(entropy_hash, entropy_manager_.GetCumulativeEntropy(10));
}

}  // namespace
}  // namespace test
}  // namespace net
