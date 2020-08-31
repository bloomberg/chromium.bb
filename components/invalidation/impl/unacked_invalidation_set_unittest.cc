// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/unacked_invalidation_set.h"

#include <stddef.h>

#include <memory>

#include "base/json/json_string_value_serializer.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "components/invalidation/public/single_object_invalidation_set.h"
#include "components/invalidation/public/topic_invalidation_map.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

class UnackedInvalidationSetTest : public testing::Test {
 public:
  UnackedInvalidationSetTest() : unacked_invalidations_(kTopic) {}

  SingleObjectInvalidationSet GetStoredInvalidations() {
    TopicInvalidationMap map;
    unacked_invalidations_.ExportInvalidations(
        base::WeakPtr<AckHandler>(),
        scoped_refptr<base::SingleThreadTaskRunner>(),
        &map);
    if (map.Empty()) {
      return SingleObjectInvalidationSet();
    } else {
      return map.ForTopic(kTopic);
    }
  }

  const Topic kTopic = "ASDF";
  UnackedInvalidationSet unacked_invalidations_;
};

namespace {

// Test storage and retrieval of zero invalidations.
TEST_F(UnackedInvalidationSetTest, Empty) {
  EXPECT_EQ(0U, GetStoredInvalidations().GetSize());
}

// Test storage and retrieval of a single invalidation.
TEST_F(UnackedInvalidationSetTest, OneInvalidation) {
  Invalidation inv1 = Invalidation::Init(kTopic, 10, "payload");
  unacked_invalidations_.Add(inv1);

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  ASSERT_EQ(1U, set.GetSize());
  EXPECT_FALSE(set.StartsWithUnknownVersion());
}

// Test that calling Clear() returns us to the empty state.
TEST_F(UnackedInvalidationSetTest, Clear) {
  Invalidation inv1 = Invalidation::Init(kTopic, 10, "payload");
  unacked_invalidations_.Add(inv1);
  unacked_invalidations_.Clear();

  EXPECT_EQ(0U, GetStoredInvalidations().GetSize());
}

// Test that repeated unknown version invalidations are squashed together.
TEST_F(UnackedInvalidationSetTest, UnknownVersions) {
  Invalidation inv1 = Invalidation::Init(kTopic, 10, "payload");
  Invalidation inv2 = Invalidation::InitUnknownVersion(kTopic);
  Invalidation inv3 = Invalidation::InitUnknownVersion(kTopic);
  unacked_invalidations_.Add(inv1);
  unacked_invalidations_.Add(inv2);
  unacked_invalidations_.Add(inv3);

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  ASSERT_EQ(2U, set.GetSize());
  EXPECT_TRUE(set.StartsWithUnknownVersion());
}

// Tests that no truncation occurs while we're under the limit.
TEST_F(UnackedInvalidationSetTest, NoTruncation) {
  size_t kMax = UnackedInvalidationSet::kMaxBufferedInvalidations;

  for (size_t i = 0; i < kMax; ++i) {
    Invalidation inv = Invalidation::Init(kTopic, i, "payload");
    unacked_invalidations_.Add(inv);
  }

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  ASSERT_EQ(kMax, set.GetSize());
  EXPECT_FALSE(set.StartsWithUnknownVersion());
  EXPECT_EQ(0, set.begin()->version());
  EXPECT_EQ(kMax-1, static_cast<size_t>(set.rbegin()->version()));
}

// Test that truncation happens as we reach the limit.
TEST_F(UnackedInvalidationSetTest, Truncation) {
  size_t kMax = UnackedInvalidationSet::kMaxBufferedInvalidations;

  for (size_t i = 0; i < kMax + 1; ++i) {
    Invalidation inv = Invalidation::Init(kTopic, i, "payload");
    unacked_invalidations_.Add(inv);
  }

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  ASSERT_EQ(kMax, set.GetSize());
  EXPECT_TRUE(set.StartsWithUnknownVersion());
  EXPECT_TRUE(set.begin()->is_unknown_version());
  EXPECT_EQ(kMax, static_cast<size_t>(set.rbegin()->version()));
}

// Test that we don't truncate while a handler is registered.
TEST_F(UnackedInvalidationSetTest, RegistrationAndTruncation) {
  unacked_invalidations_.SetHandlerIsRegistered();

  size_t kMax = UnackedInvalidationSet::kMaxBufferedInvalidations;

  for (size_t i = 0; i < kMax + 1; ++i) {
    Invalidation inv = Invalidation::Init(kTopic, i, "payload");
    unacked_invalidations_.Add(inv);
  }

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  ASSERT_EQ(kMax+1, set.GetSize());
  EXPECT_FALSE(set.StartsWithUnknownVersion());
  EXPECT_EQ(0, set.begin()->version());
  EXPECT_EQ(kMax, static_cast<size_t>(set.rbegin()->version()));

  // Unregistering should re-enable truncation.
  unacked_invalidations_.SetHandlerIsUnregistered();
  SingleObjectInvalidationSet set2 = GetStoredInvalidations();
  ASSERT_EQ(kMax, set2.GetSize());
  EXPECT_TRUE(set2.StartsWithUnknownVersion());
  EXPECT_TRUE(set2.begin()->is_unknown_version());
  EXPECT_EQ(kMax, static_cast<size_t>(set2.rbegin()->version()));
}

// Test acknowledgement.
TEST_F(UnackedInvalidationSetTest, Acknowledge) {
  // inv2 is included in this test just to make sure invalidations that
  // are supposed to be unaffected by this operation will be unaffected.

  // We don't expect to be receiving acks or drops unless this flag is set.
  // Not that it makes much of a difference in behavior.
  unacked_invalidations_.SetHandlerIsRegistered();

  Invalidation inv1 = Invalidation::Init(kTopic, 10, "payload");
  Invalidation inv2 = Invalidation::InitUnknownVersion(kTopic);
  AckHandle inv1_handle = inv1.ack_handle();

  unacked_invalidations_.Add(inv1);
  unacked_invalidations_.Add(inv2);

  unacked_invalidations_.Acknowledge(inv1_handle);

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  EXPECT_EQ(1U, set.GetSize());
  EXPECT_TRUE(set.StartsWithUnknownVersion());
}

// Test drops.
TEST_F(UnackedInvalidationSetTest, Drop) {
  // inv2 is included in this test just to make sure invalidations that
  // are supposed to be unaffected by this operation will be unaffected.

  // We don't expect to be receiving acks or drops unless this flag is set.
  // Not that it makes much of a difference in behavior.
  unacked_invalidations_.SetHandlerIsRegistered();

  Invalidation inv1 = Invalidation::Init(kTopic, 10, "payload");
  Invalidation inv2 = Invalidation::Init(kTopic, 15, "payload");
  AckHandle inv1_handle = inv1.ack_handle();

  unacked_invalidations_.Add(inv1);
  unacked_invalidations_.Add(inv2);

  unacked_invalidations_.Drop(inv1_handle);

  SingleObjectInvalidationSet set = GetStoredInvalidations();
  ASSERT_EQ(2U, set.GetSize());
  EXPECT_TRUE(set.StartsWithUnknownVersion());
  EXPECT_EQ(15, set.rbegin()->version());
}

}  // namespace

}  // namespace syncer
