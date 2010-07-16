// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
#define CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_

#include <vector>

#include "chrome/browser/browser.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/test/bookmark_load_observer.h"
#include "chrome/test/live_sync/bookmark_model_verifier.h"
#include "chrome/test/live_sync/live_sync_test.h"
#include "chrome/test/ui_test_utils.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

class LiveBookmarksSyncTest : public LiveSyncTest {
 public:
  explicit LiveBookmarksSyncTest(TestType test_type)
      : LiveSyncTest(test_type) {}
  virtual ~LiveBookmarksSyncTest() {}

  // Sets up sync profiles and clients and initializes the verifier bookmark
  // model.
  virtual bool SetupClients() {
    if (!LiveSyncTest::SetupClients())
      return false;
    for (int i = 0; i < num_clients(); ++i) {
      ui_test_utils::WaitForBookmarkModelToLoad(
          GetProfile(i)->GetBookmarkModel());
    }
    verifier_helper_.reset(BookmarkModelVerifier::Create(
        GetVerifierBookmarkModel()));
    return (verifier_helper_.get() != NULL);
  }

  // Used to access the bookmarks within a particular sync profile.
  BookmarkModel* GetBookmarkModel(int index) {
    return GetProfile(index)->GetBookmarkModel();
  }

  // Used to access the bookmark bar within a particular sync profile.
  const BookmarkNode* GetBookmarkBarNode(int index) {
    return GetBookmarkModel(index)->GetBookmarkBarNode();
  }

  // Used to access the bookmarks within the verifier sync profile.
  BookmarkModel* GetVerifierBookmarkModel() {
    return verifier()->GetBookmarkModel();
  }

  // Used to access the service object within a particular sync client.
  ProfileSyncService* GetService(int index) {
    return GetClient(index)->service();
  }

  // Used to access the BookmarkModelVerifier helper object.
  BookmarkModelVerifier* verifier_helper() {
    return verifier_helper_.get();
  }

  // Helper to get a handle on a bookmark in |m| when the url is known to be
  // unique.
  static const BookmarkNode* GetByUniqueURL(BookmarkModel* m, const GURL& url) {
    std::vector<const BookmarkNode*> nodes;
    m->GetNodesByURL(url, &nodes);
    EXPECT_EQ(1U, nodes.size());
    if (nodes.empty())
      return NULL;
    return nodes[0];
  }

 private:
  // Helper object that has the functionality to verify changes made to the
  // bookmarks of individual profiles.
  scoped_ptr<BookmarkModelVerifier> verifier_helper_;

  DISALLOW_COPY_AND_ASSIGN(LiveBookmarksSyncTest);
};

class SingleClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  SingleClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(SINGLE_CLIENT) {}
  ~SingleClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientLiveBookmarksSyncTest);
};

class TwoClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  TwoClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(TWO_CLIENT) {}
  ~TwoClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientLiveBookmarksSyncTest);
};

class MultipleClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  MultipleClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(MULTIPLE_CLIENT) {}
  ~MultipleClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(MultipleClientLiveBookmarksSyncTest);
};

class ManyClientLiveBookmarksSyncTest : public LiveBookmarksSyncTest {
 public:
  ManyClientLiveBookmarksSyncTest()
      : LiveBookmarksSyncTest(MANY_CLIENT) {}
  ~ManyClientLiveBookmarksSyncTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(ManyClientLiveBookmarksSyncTest);
};

#endif  // CHROME_TEST_LIVE_SYNC_LIVE_BOOKMARKS_SYNC_TEST_H_
