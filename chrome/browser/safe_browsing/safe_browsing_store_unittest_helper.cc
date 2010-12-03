// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/safe_browsing_store_unittest_helper.h"

#include "base/file_util.h"

namespace {

const int kAddChunk1 = 1;
const int kAddChunk2 = 3;
const int kAddChunk3 = 5;
const int kAddChunk4 = 7;
// Disjoint chunk numbers for subs to flush out typos.
const int kSubChunk1 = 2;
const int kSubChunk2 = 4;
const int kSubChunk3 = 6;

const SBFullHash kHash1 = SBFullHashFromString("one");
const SBFullHash kHash2 = SBFullHashFromString("two");
const SBFullHash kHash3 = SBFullHashFromString("three");
const SBFullHash kHash4 = SBFullHashFromString("four");
const SBFullHash kHash5 = SBFullHashFromString("five");

}  // namespace

void SafeBrowsingStoreTestEmpty(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());

  std::vector<int> chunks;
  store->GetAddChunks(&chunks);
  EXPECT_TRUE(chunks.empty());
  store->GetSubChunks(&chunks);
  EXPECT_TRUE(chunks.empty());

  // Shouldn't see anything, but anything is a big set to test.
  EXPECT_FALSE(store->CheckAddChunk(0));
  EXPECT_FALSE(store->CheckAddChunk(1));
  EXPECT_FALSE(store->CheckAddChunk(-1));

  EXPECT_FALSE(store->CheckSubChunk(0));
  EXPECT_FALSE(store->CheckSubChunk(1));
  EXPECT_FALSE(store->CheckSubChunk(-1));

  std::vector<SBAddFullHash> pending_adds;
  std::set<SBPrefix> prefix_misses;
  std::vector<SBAddPrefix> add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));
  EXPECT_TRUE(add_prefixes_result.empty());
  EXPECT_TRUE(add_full_hashes_result.empty());
}

void SafeBrowsingStoreTestStorePrefix(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());

  const base::Time now = base::Time::Now();

  EXPECT_TRUE(store->BeginChunk());
  store->SetAddChunk(kAddChunk1);
  EXPECT_TRUE(store->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash1.prefix));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store->WriteAddHash(kAddChunk1, now, kHash2));

  store->SetSubChunk(kSubChunk1);
  EXPECT_TRUE(store->CheckSubChunk(kSubChunk1));
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store->WriteSubHash(kSubChunk1, kAddChunk3, kHash3));
  EXPECT_TRUE(store->FinishChunk());

  // Chunk numbers shouldn't leak over.
  EXPECT_FALSE(store->CheckAddChunk(kSubChunk1));
  EXPECT_FALSE(store->CheckAddChunk(kAddChunk3));
  EXPECT_FALSE(store->CheckSubChunk(kAddChunk1));

  std::vector<int> chunks;
  store->GetAddChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kAddChunk1, chunks[0]);

  store->GetSubChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kSubChunk1, chunks[0]);

  std::vector<SBAddFullHash> pending_adds;
  std::set<SBPrefix> prefix_misses;
  std::vector<SBAddPrefix> add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));

  ASSERT_EQ(2U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(kAddChunk1, add_prefixes_result[1].chunk_id);
  EXPECT_EQ(kHash2.prefix, add_prefixes_result[1].prefix);

  ASSERT_EQ(1U, add_full_hashes_result.size());
  EXPECT_EQ(kAddChunk1, add_full_hashes_result[0].chunk_id);
  // EXPECT_TRUE(add_full_hashes_result[0].received == now)?
  EXPECT_EQ(now.ToTimeT(), add_full_hashes_result[0].received);
  EXPECT_TRUE(SBFullHashEq(kHash2, add_full_hashes_result[0].full_hash));

  add_prefixes_result.clear();
  add_full_hashes_result.clear();

  EXPECT_TRUE(store->BeginUpdate());

  // Still has the chunks expected in the next update.
  store->GetAddChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kAddChunk1, chunks[0]);

  store->GetSubChunks(&chunks);
  ASSERT_EQ(1U, chunks.size());
  EXPECT_EQ(kSubChunk1, chunks[0]);

  EXPECT_TRUE(store->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store->CheckSubChunk(kSubChunk1));

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));

  // Still has the expected contents.
  ASSERT_EQ(2U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(kAddChunk1, add_prefixes_result[1].chunk_id);
  EXPECT_EQ(kHash2.prefix, add_prefixes_result[1].prefix);

  ASSERT_EQ(1U, add_full_hashes_result.size());
  EXPECT_EQ(kAddChunk1, add_full_hashes_result[0].chunk_id);
  EXPECT_EQ(now.ToTimeT(), add_full_hashes_result[0].received);
  EXPECT_TRUE(SBFullHashEq(kHash2, add_full_hashes_result[0].full_hash));
}

void SafeBrowsingStoreTestSubKnockout(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());

  const base::Time now = base::Time::Now();

  EXPECT_TRUE(store->BeginChunk());
  store->SetAddChunk(kAddChunk1);
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash1.prefix));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store->WriteAddHash(kAddChunk1, now, kHash2));

  store->SetSubChunk(kSubChunk1);
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk1, kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store->FinishChunk());

  std::vector<SBAddFullHash> pending_adds;
  std::set<SBPrefix> prefix_misses;
  std::vector<SBAddPrefix> add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));

  // Knocked out the chunk expected.
  ASSERT_EQ(1U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_TRUE(add_full_hashes_result.empty());

  add_prefixes_result.clear();

  EXPECT_TRUE(store->BeginUpdate());

  // This add should be knocked out by an existing sub.
  EXPECT_TRUE(store->BeginChunk());
  store->SetAddChunk(kAddChunk3);
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store->FinishChunk());

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));
  EXPECT_EQ(1U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_TRUE(add_full_hashes_result.empty());

  add_prefixes_result.clear();

  EXPECT_TRUE(store->BeginUpdate());

  // But by here the sub should be gone, so it should stick this time.
  EXPECT_TRUE(store->BeginChunk());
  store->SetAddChunk(kAddChunk3);
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store->FinishChunk());

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));
  ASSERT_EQ(2U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk1, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash1.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(kAddChunk3, add_prefixes_result[1].chunk_id);
  EXPECT_EQ(kHash3.prefix, add_prefixes_result[1].prefix);
  EXPECT_TRUE(add_full_hashes_result.empty());
}

void SafeBrowsingStoreTestDeleteChunks(SafeBrowsingStore* store) {
  EXPECT_TRUE(store->BeginUpdate());

  const base::Time now = base::Time::Now();

  // A chunk which will be deleted.
  EXPECT_FALSE(store->CheckAddChunk(kAddChunk1));
  store->SetAddChunk(kAddChunk1);
  EXPECT_TRUE(store->BeginChunk());
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash1.prefix));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash2.prefix));
  EXPECT_TRUE(store->WriteAddHash(kAddChunk1, now, kHash2));
  EXPECT_TRUE(store->FinishChunk());

  // Another which won't.
  EXPECT_FALSE(store->CheckAddChunk(kAddChunk2));
  store->SetAddChunk(kAddChunk2);
  EXPECT_TRUE(store->BeginChunk());
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk2, kHash3.prefix));
  EXPECT_TRUE(store->WriteAddHash(kAddChunk2, now, kHash3));
  EXPECT_TRUE(store->FinishChunk());

  // A sub chunk to delete.
  EXPECT_FALSE(store->CheckSubChunk(kSubChunk1));
  store->SetSubChunk(kSubChunk1);
  EXPECT_TRUE(store->BeginChunk());
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash4.prefix));
  EXPECT_TRUE(store->WriteSubHash(kSubChunk1, kAddChunk3, kHash4));
  EXPECT_TRUE(store->FinishChunk());

  // A sub chunk to keep.
  EXPECT_FALSE(store->CheckSubChunk(kSubChunk2));
  store->SetSubChunk(kSubChunk2);
  EXPECT_TRUE(store->BeginChunk());
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk2, kAddChunk4, kHash5.prefix));
  EXPECT_TRUE(store->WriteSubHash(kSubChunk2, kAddChunk4, kHash5));
  EXPECT_TRUE(store->FinishChunk());

  store->DeleteAddChunk(kAddChunk1);
  store->DeleteSubChunk(kSubChunk1);

  // Not actually deleted until finish.
  EXPECT_TRUE(store->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store->CheckAddChunk(kAddChunk2));
  EXPECT_TRUE(store->CheckSubChunk(kSubChunk1));
  EXPECT_TRUE(store->CheckSubChunk(kSubChunk2));

  std::vector<SBAddFullHash> pending_adds;
  std::set<SBPrefix> prefix_misses;
  std::vector<SBAddPrefix> add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));

  EXPECT_EQ(1U, add_prefixes_result.size());
  EXPECT_EQ(kAddChunk2, add_prefixes_result[0].chunk_id);
  EXPECT_EQ(kHash3.prefix, add_prefixes_result[0].prefix);
  EXPECT_EQ(1U, add_full_hashes_result.size());
  EXPECT_EQ(kAddChunk2, add_full_hashes_result[0].chunk_id);
  EXPECT_EQ(now.ToTimeT(), add_full_hashes_result[0].received);
  EXPECT_TRUE(SBFullHashEq(kHash3, add_full_hashes_result[0].full_hash));

  // Expected chunks are there in another update.
  EXPECT_TRUE(store->BeginUpdate());
  EXPECT_FALSE(store->CheckAddChunk(kAddChunk1));
  EXPECT_TRUE(store->CheckAddChunk(kAddChunk2));
  EXPECT_FALSE(store->CheckSubChunk(kSubChunk1));
  EXPECT_TRUE(store->CheckSubChunk(kSubChunk2));

  // Delete them, too.
  store->DeleteAddChunk(kAddChunk2);
  store->DeleteSubChunk(kSubChunk2);

  add_prefixes_result.clear();
  add_full_hashes_result.clear();
  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));

  // Expect no more chunks.
  EXPECT_TRUE(store->BeginUpdate());
  EXPECT_FALSE(store->CheckAddChunk(kAddChunk1));
  EXPECT_FALSE(store->CheckAddChunk(kAddChunk2));
  EXPECT_FALSE(store->CheckSubChunk(kSubChunk1));
  EXPECT_FALSE(store->CheckSubChunk(kSubChunk2));
  add_prefixes_result.clear();
  add_full_hashes_result.clear();
  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));
  EXPECT_TRUE(add_prefixes_result.empty());
  EXPECT_TRUE(add_full_hashes_result.empty());
}

void SafeBrowsingStoreTestDelete(SafeBrowsingStore* store,
                                 const FilePath& filename) {
  // Delete should work if the file wasn't there in the first place.
  EXPECT_FALSE(file_util::PathExists(filename));
  EXPECT_TRUE(store->Delete());

  // Create a store file.
  EXPECT_TRUE(store->BeginUpdate());

  EXPECT_TRUE(store->BeginChunk());
  store->SetAddChunk(kAddChunk1);
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash1.prefix));
  EXPECT_TRUE(store->WriteAddPrefix(kAddChunk1, kHash2.prefix));

  store->SetSubChunk(kSubChunk1);
  EXPECT_TRUE(store->WriteSubPrefix(kSubChunk1, kAddChunk3, kHash3.prefix));
  EXPECT_TRUE(store->FinishChunk());

  std::vector<SBAddFullHash> pending_adds;
  std::set<SBPrefix> prefix_misses;
  std::vector<SBAddPrefix> add_prefixes_result;
  std::vector<SBAddFullHash> add_full_hashes_result;

  EXPECT_TRUE(store->FinishUpdate(pending_adds,
                                  prefix_misses,
                                  &add_prefixes_result,
                                  &add_full_hashes_result));

  EXPECT_TRUE(file_util::PathExists(filename));
  EXPECT_TRUE(store->Delete());
  EXPECT_FALSE(file_util::PathExists(filename));
}
