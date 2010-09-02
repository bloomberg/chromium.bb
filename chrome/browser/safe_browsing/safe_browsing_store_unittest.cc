// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store.h"
#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(SafeBrowsingStoreTest, SBAddPrefixLess) {
  // chunk_id then prefix.
  EXPECT_TRUE(SBAddPrefixLess(SBAddPrefix(10, 1), SBAddPrefix(11, 1)));
  EXPECT_FALSE(SBAddPrefixLess(SBAddPrefix(11, 1), SBAddPrefix(10, 1)));
  EXPECT_TRUE(SBAddPrefixLess(SBAddPrefix(10, 1), SBAddPrefix(10, 2)));
  EXPECT_FALSE(SBAddPrefixLess(SBAddPrefix(10, 2), SBAddPrefix(10, 1)));

  // Equal is not less.
  EXPECT_FALSE(SBAddPrefixLess(SBAddPrefix(10, 1), SBAddPrefix(10, 1)));
}

TEST(SafeBrowsingStoreTest, SBAddPrefixHashLess) {
  // The first four bytes of SBFullHash can be read as an int32, which
  // means that byte-ordering issues can come up.  To test this, |one|
  // and |two| differ in the prefix, while |one| and |onetwo| have the
  // same prefix, but differ in the byte after the prefix.
  SBFullHash one, onetwo, two;
  memset(&one, 0, sizeof(one));
  memset(&onetwo, 0, sizeof(onetwo));
  memset(&two, 0, sizeof(two));
  one.prefix = 1;
  one.full_hash[sizeof(int32)] = 1;
  onetwo.prefix = 1;
  onetwo.full_hash[sizeof(int32)] = 2;
  two.prefix = 2;

  const base::Time now = base::Time::Now();

  // add_id dominates.
  EXPECT_TRUE(SBAddPrefixHashLess(SBAddFullHash(10, now, two),
                                  SBAddFullHash(11, now, one)));
  EXPECT_FALSE(SBAddPrefixHashLess(SBAddFullHash(11, now, two),
                                   SBAddFullHash(10, now, one)));

  // After add_id, prefix.
  EXPECT_TRUE(SBAddPrefixHashLess(SBAddFullHash(10, now, one),
                                  SBAddFullHash(10, now, two)));
  EXPECT_FALSE(SBAddPrefixHashLess(SBAddFullHash(10, now, two),
                                   SBAddFullHash(10, now, one)));

  // After prefix, full hash.
  EXPECT_TRUE(SBAddPrefixHashLess(SBAddFullHash(10, now, one),
                                  SBAddFullHash(10, now, onetwo)));
  EXPECT_FALSE(SBAddPrefixHashLess(SBAddFullHash(10, now, onetwo),
                                   SBAddFullHash(10, now, one)));

  // Equal is not less-than.
  EXPECT_FALSE(SBAddPrefixHashLess(SBAddFullHash(10, now, one),
                                   SBAddFullHash(10, now, one)));
}

TEST(SafeBrowsingStoreTest, SBSubPrefixLess) {
  // add_id dominates.
  EXPECT_TRUE(SBAddPrefixLess(SBSubPrefix(12, 10, 2), SBSubPrefix(9, 11, 1)));
  EXPECT_FALSE(SBAddPrefixLess(SBSubPrefix(12, 11, 2), SBSubPrefix(9, 10, 1)));

  // After add_id, prefix.
  EXPECT_TRUE(SBAddPrefixLess(SBSubPrefix(12, 10, 1), SBSubPrefix(9, 10, 2)));
  EXPECT_FALSE(SBAddPrefixLess(SBSubPrefix(12, 10, 2), SBSubPrefix(9, 10, 1)));

  // Equal is not less-than.
  EXPECT_FALSE(SBAddPrefixLess(SBSubPrefix(12, 10, 1), SBSubPrefix(12, 10, 1)));

  // chunk_id doesn't matter.
}

TEST(SafeBrowsingStoreTest, SBSubFullHashLess) {
  SBFullHash one, onetwo, two;
  memset(&one, 0, sizeof(one));
  memset(&onetwo, 0, sizeof(onetwo));
  memset(&two, 0, sizeof(two));
  one.prefix = 1;
  one.full_hash[sizeof(int32)] = 1;
  onetwo.prefix = 1;
  onetwo.full_hash[sizeof(int32)] = 2;
  two.prefix = 2;

  // add_id dominates.
  EXPECT_TRUE(SBAddPrefixHashLess(SBSubFullHash(12, 10, two),
                                  SBSubFullHash(9, 11, one)));
  EXPECT_FALSE(SBAddPrefixHashLess(SBSubFullHash(12, 11, two),
                                   SBSubFullHash(9, 10, one)));

  // After add_id, prefix.
  EXPECT_TRUE(SBAddPrefixHashLess(SBSubFullHash(12, 10, one),
                                  SBSubFullHash(9, 10, two)));
  EXPECT_FALSE(SBAddPrefixHashLess(SBSubFullHash(12, 10, two),
                                   SBSubFullHash(9, 10, one)));

  // After prefix, full_hash.
  EXPECT_TRUE(SBAddPrefixHashLess(SBSubFullHash(12, 10, one),
                                  SBSubFullHash(9, 10, onetwo)));
  EXPECT_FALSE(SBAddPrefixHashLess(SBSubFullHash(12, 10, onetwo),
                                   SBSubFullHash(9, 10, one)));

  // Equal is not less-than.
  EXPECT_FALSE(SBAddPrefixHashLess(SBSubFullHash(12, 10, one),
                                   SBSubFullHash(9, 10, one)));
}

// SBProcessSubs does a lot of iteration, run through empty just to
// make sure degenerate cases work.
TEST(SafeBrowsingStoreTest, SBProcessSubsEmpty) {
  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  std::vector<SBSubPrefix> sub_prefixes;
  std::vector<SBSubFullHash> sub_hashes;

  const base::hash_set<int32> no_deletions;
  SBProcessSubs(&add_prefixes, &sub_prefixes, &add_hashes, &sub_hashes,
                no_deletions, no_deletions);
  EXPECT_TRUE(add_prefixes.empty());
  EXPECT_TRUE(sub_prefixes.empty());
  EXPECT_TRUE(add_hashes.empty());
  EXPECT_TRUE(sub_hashes.empty());
}

// Test that subs knock out adds.
TEST(SafeBrowsingStoreTest, SBProcessSubsKnockout) {
  const base::Time kNow = base::Time::Now();
  const SBFullHash kHash1(SBFullHashFromString("one"));
  const SBFullHash kHash2(SBFullHashFromString("two"));
  const SBFullHash kHash3(SBFullHashFromString("three"));
  const int kAddChunk1 = 1;  // Use different chunk numbers just in case.
  const int kSubChunk1 = 2;

  // Construct some full hashes which share prefix with another.
  SBFullHash kHash1mod1 = kHash1;
  kHash1mod1.full_hash[sizeof(kHash1mod1.full_hash) - 1] ++;
  SBFullHash kHash1mod2 = kHash1mod1;
  kHash1mod2.full_hash[sizeof(kHash1mod2.full_hash) - 1] ++;
  SBFullHash kHash1mod3 = kHash1mod2;
  kHash1mod3.full_hash[sizeof(kHash1mod3.full_hash) - 1] ++;

  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  std::vector<SBSubPrefix> sub_prefixes;
  std::vector<SBSubFullHash> sub_hashes;

  // An add with prefix and a couple hashes, plus a sub for the prefix
  // and a couple sub hashes.  The sub should knock all of them out.
  add_prefixes.push_back(SBAddPrefix(kAddChunk1, kHash1.prefix));
  add_hashes.push_back(SBAddFullHash(kAddChunk1, kNow, kHash1));
  add_hashes.push_back(SBAddFullHash(kAddChunk1, kNow, kHash1mod1));
  sub_prefixes.push_back(SBSubPrefix(kSubChunk1, kAddChunk1, kHash1.prefix));
  sub_hashes.push_back(SBSubFullHash(kSubChunk1, kAddChunk1, kHash1mod2));
  sub_hashes.push_back(SBSubFullHash(kSubChunk1, kAddChunk1, kHash1mod3));

  // An add with no corresponding sub.  Both items should be retained.
  add_hashes.push_back(SBAddFullHash(kAddChunk1, kNow, kHash2));
  add_prefixes.push_back(SBAddPrefix(kAddChunk1, kHash2.prefix));

  // A sub with no corresponding add.  Both items should be retained.
  sub_hashes.push_back(SBSubFullHash(kSubChunk1, kAddChunk1, kHash3));
  sub_prefixes.push_back(SBSubPrefix(kSubChunk1, kAddChunk1, kHash3.prefix));

  const base::hash_set<int32> no_deletions;
  SBProcessSubs(&add_prefixes, &sub_prefixes, &add_hashes, &sub_hashes,
                no_deletions, no_deletions);

  EXPECT_EQ(1U, add_prefixes.size());
  EXPECT_EQ(kAddChunk1, add_prefixes[0].chunk_id);
  EXPECT_EQ(kHash2.prefix, add_prefixes[0].prefix);

  EXPECT_EQ(1U, add_hashes.size());
  EXPECT_EQ(kAddChunk1, add_hashes[0].chunk_id);
  EXPECT_TRUE(SBFullHashEq(kHash2, add_hashes[0].full_hash));

  EXPECT_EQ(1U, sub_prefixes.size());
  EXPECT_EQ(kSubChunk1, sub_prefixes[0].chunk_id);
  EXPECT_EQ(kAddChunk1, sub_prefixes[0].add_chunk_id);
  EXPECT_EQ(kHash3.prefix, sub_prefixes[0].add_prefix);

  EXPECT_EQ(1U, sub_hashes.size());
  EXPECT_EQ(kSubChunk1, sub_hashes[0].chunk_id);
  EXPECT_EQ(kAddChunk1, sub_hashes[0].add_chunk_id);
  EXPECT_TRUE(SBFullHashEq(kHash3, sub_hashes[0].full_hash));
}

// Test chunk deletions, and ordering of deletions WRT subs knocking
// out adds.
TEST(SafeBrowsingStoreTest, SBProcessSubsDeleteChunk) {
  const base::Time kNow = base::Time::Now();
  const SBFullHash kHash1(SBFullHashFromString("one"));
  const SBFullHash kHash2(SBFullHashFromString("two"));
  const SBFullHash kHash3(SBFullHashFromString("three"));
  const int kAddChunk1 = 1;  // Use different chunk numbers just in case.
  const int kSubChunk1 = 2;

  // Construct some full hashes which share prefix with another.
  SBFullHash kHash1mod1 = kHash1;
  kHash1mod1.full_hash[sizeof(kHash1mod1.full_hash) - 1] ++;
  SBFullHash kHash1mod2 = kHash1mod1;
  kHash1mod2.full_hash[sizeof(kHash1mod2.full_hash) - 1] ++;
  SBFullHash kHash1mod3 = kHash1mod2;
  kHash1mod3.full_hash[sizeof(kHash1mod3.full_hash) - 1] ++;

  std::vector<SBAddPrefix> add_prefixes;
  std::vector<SBAddFullHash> add_hashes;
  std::vector<SBSubPrefix> sub_prefixes;
  std::vector<SBSubFullHash> sub_hashes;

  // An add with prefix and a couple hashes, plus a sub for the prefix
  // and a couple sub hashes.  The sub should knock all of them out.
  add_prefixes.push_back(SBAddPrefix(kAddChunk1, kHash1.prefix));
  add_hashes.push_back(SBAddFullHash(kAddChunk1, kNow, kHash1));
  add_hashes.push_back(SBAddFullHash(kAddChunk1, kNow, kHash1mod1));
  sub_prefixes.push_back(SBSubPrefix(kSubChunk1, kAddChunk1, kHash1.prefix));
  sub_hashes.push_back(SBSubFullHash(kSubChunk1, kAddChunk1, kHash1mod2));
  sub_hashes.push_back(SBSubFullHash(kSubChunk1, kAddChunk1, kHash1mod3));

  // An add with no corresponding sub.  Both items should be retained.
  add_hashes.push_back(SBAddFullHash(kAddChunk1, kNow, kHash2));
  add_prefixes.push_back(SBAddPrefix(kAddChunk1, kHash2.prefix));

  // A sub with no corresponding add.  Both items should be retained.
  sub_hashes.push_back(SBSubFullHash(kSubChunk1, kAddChunk1, kHash3));
  sub_prefixes.push_back(SBSubPrefix(kSubChunk1, kAddChunk1, kHash3.prefix));

  const base::hash_set<int32> no_deletions;
  base::hash_set<int32> add_deletions;
  add_deletions.insert(kAddChunk1);
  SBProcessSubs(&add_prefixes, &sub_prefixes, &add_hashes, &sub_hashes,
                add_deletions, no_deletions);

  EXPECT_TRUE(add_prefixes.empty());
  EXPECT_TRUE(add_hashes.empty());

  EXPECT_EQ(1U, sub_prefixes.size());
  EXPECT_EQ(kSubChunk1, sub_prefixes[0].chunk_id);
  EXPECT_EQ(kAddChunk1, sub_prefixes[0].add_chunk_id);
  EXPECT_EQ(kHash3.prefix, sub_prefixes[0].add_prefix);

  EXPECT_EQ(1U, sub_hashes.size());
  EXPECT_EQ(kSubChunk1, sub_hashes[0].chunk_id);
  EXPECT_EQ(kAddChunk1, sub_hashes[0].add_chunk_id);
  EXPECT_TRUE(SBFullHashEq(kHash3, sub_hashes[0].full_hash));

  base::hash_set<int32> sub_deletions;
  sub_deletions.insert(kSubChunk1);
  SBProcessSubs(&add_prefixes, &sub_prefixes, &add_hashes, &sub_hashes,
                no_deletions, sub_deletions);

  EXPECT_TRUE(add_prefixes.empty());
  EXPECT_TRUE(add_hashes.empty());
  EXPECT_TRUE(sub_prefixes.empty());
  EXPECT_TRUE(sub_hashes.empty());
}

TEST(SafeBrowsingStoreTest, Y2K38) {
  const base::Time now = base::Time::Now();
  const base::Time future = now + base::TimeDelta::FromDays(3*365);

  // TODO: Fix file format before 2035.
  EXPECT_GT(static_cast<int32>(future.ToTimeT()), 0)
    << " (int32)time_t is running out.";
}

}  // namespace
