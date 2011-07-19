// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "chrome/browser/sync/sessions/ordered_commit_set.h"
#include "chrome/test/sync/engine/test_id_factory.h"

using std::vector;

class OrderedCommitSetTest : public testing::Test {
 public:
  OrderedCommitSetTest() {
    routes_[syncable::BOOKMARKS] = browser_sync::GROUP_UI;
    routes_[syncable::PREFERENCES] = browser_sync::GROUP_UI;
    routes_[syncable::AUTOFILL] = browser_sync::GROUP_DB;
    routes_[syncable::TOP_LEVEL_FOLDER] = browser_sync::GROUP_PASSIVE;
  }
 protected:
  browser_sync::TestIdFactory ids_;
  browser_sync::ModelSafeRoutingInfo routes_;
};

namespace browser_sync {
namespace sessions {

TEST_F(OrderedCommitSetTest, Projections) {
  vector<syncable::Id> expected;
  for (int i = 0; i < 8; i++)
    expected.push_back(ids_.NewLocalId());

  OrderedCommitSet commit_set1(routes_), commit_set2(routes_);
  commit_set1.AddCommitItem(0, expected[0], syncable::BOOKMARKS);
  commit_set1.AddCommitItem(1, expected[1], syncable::BOOKMARKS);
  commit_set1.AddCommitItem(2, expected[2], syncable::PREFERENCES);
  // Duplicates should be dropped.
  commit_set1.AddCommitItem(2, expected[2], syncable::PREFERENCES);
  commit_set1.AddCommitItem(3, expected[3], syncable::TOP_LEVEL_FOLDER);
  commit_set1.AddCommitItem(4, expected[4], syncable::TOP_LEVEL_FOLDER);
  commit_set2.AddCommitItem(7, expected[7], syncable::AUTOFILL);
  commit_set2.AddCommitItem(6, expected[6], syncable::AUTOFILL);
  commit_set2.AddCommitItem(5, expected[5], syncable::AUTOFILL);
  // Add something in set1 to set2, which should get dropped by AppendReverse.
  commit_set2.AddCommitItem(0, expected[0], syncable::BOOKMARKS);
  commit_set1.AppendReverse(commit_set2);

  // First, we should verify the projections are correct. Second, we want to
  // do the same verification after truncating by 1. Next, try truncating
  // the set to a size of 4, so that the DB projection is wiped out and
  // PASSIVE has one element removed.  Finally, truncate to 1 so only UI is
  // remaining.
  int j = 0;
  do {
    SCOPED_TRACE(::testing::Message("Iteration j = ") << j);
    vector<syncable::Id> all_ids = commit_set1.GetAllCommitIds();
    EXPECT_EQ(expected.size(), all_ids.size());
    for (size_t i = 0; i < expected.size(); i++) {
      SCOPED_TRACE(::testing::Message("CommitSet mismatch at iteration i = ")
                   << i);
      EXPECT_TRUE(expected[i] == all_ids[i]);
      EXPECT_TRUE(expected[i] == commit_set1.GetCommitIdAt(i));
    }

    OrderedCommitSet::Projection p1, p2, p3;
    p1 = commit_set1.GetCommitIdProjection(GROUP_UI);
    p2 = commit_set1.GetCommitIdProjection(GROUP_PASSIVE);
    p3 = commit_set1.GetCommitIdProjection(GROUP_DB);
    EXPECT_TRUE(p1.size() + p2.size() + p3.size() == expected.size()) << "Sum"
        << "of sizes of projections should equal full expected size!";

    for (size_t i = 0; i < p1.size(); i++) {
      SCOPED_TRACE(::testing::Message("UI projection mismatch at i = ") << i);
      EXPECT_TRUE(expected[p1[i]] == commit_set1.GetCommitIdAt(p1[i]))
          << "expected[p1[i]] = " << expected[p1[i]]
          << ", commit_set1[p1[i]] = " << commit_set1.GetCommitIdAt(p1[i]);
    }
    for (size_t i = 0; i < p2.size(); i++) {
      SCOPED_TRACE(::testing::Message("PASSIVE projection mismatch at i = ")
                   << i);
      EXPECT_TRUE(expected[p2[i]] == commit_set1.GetCommitIdAt(p2[i]))
          << "expected[p2[i]] = " << expected[p2[i]]
          << ", commit_set1[p2[i]] = " << commit_set1.GetCommitIdAt(p2[i]);
    }
    for (size_t i = 0; i < p3.size(); i++) {
      SCOPED_TRACE(::testing::Message("DB projection mismatch at i = ") << i);
      EXPECT_TRUE(expected[p3[i]] == commit_set1.GetCommitIdAt(p3[i]))
          << "expected[p3[i]] = " << expected[p3[i]]
          << ", commit_set1[p3[i]] = " << commit_set1.GetCommitIdAt(p3[i]);
    }

    int cut_to_size = 7 - 3 * j++;
    if (cut_to_size < 0)
      break;

    expected.resize(cut_to_size);
    commit_set1.Truncate(cut_to_size);
  } while (true);
}

TEST_F(OrderedCommitSetTest, HasBookmarkCommitId) {
  OrderedCommitSet commit_set(routes_);

  commit_set.AddCommitItem(0, ids_.NewLocalId(), syncable::AUTOFILL);
  commit_set.AddCommitItem(1, ids_.NewLocalId(), syncable::TOP_LEVEL_FOLDER);
  EXPECT_FALSE(commit_set.HasBookmarkCommitId());

  commit_set.AddCommitItem(2, ids_.NewLocalId(), syncable::PREFERENCES);
  commit_set.AddCommitItem(3, ids_.NewLocalId(), syncable::PREFERENCES);
  EXPECT_FALSE(commit_set.HasBookmarkCommitId());

  commit_set.AddCommitItem(4, ids_.NewLocalId(), syncable::BOOKMARKS);
  EXPECT_TRUE(commit_set.HasBookmarkCommitId());

  commit_set.Truncate(4);
  EXPECT_FALSE(commit_set.HasBookmarkCommitId());
}

}  // namespace sessions
}  // namespace browser_sync

