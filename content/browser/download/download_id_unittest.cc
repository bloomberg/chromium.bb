// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/download/download_id.h"

#include <algorithm>
#include <map>
#include <set>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "content/browser/browser_thread_impl.h"
#include "content/browser/download/mock_download_manager.h"
#include "content/browser/download/mock_download_manager_delegate.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using content::BrowserThreadImpl;

class DownloadIdTest : public testing::Test {
 public:
  DownloadIdTest()
      : download_manager_delegate_(new MockDownloadManagerDelegate),
        ui_thread_(BrowserThread::UI, &message_loop_) {
    num_managers_ = ARRAYSIZE_UNSAFE(download_managers_);
    std::vector<MockDownloadManager*> managers;
    managers.resize(num_managers_);
    size_t i;
    // Create the download managers.
    for (i = 0; i < num_managers_; ++i) {
      managers[i] =
          new MockDownloadManager(download_manager_delegate_.get(), NULL, NULL);
    }
    // Sort by pointer value.
    std::sort(managers.begin(), managers.end());
    // Assign to |download_managers_|.
    for (i = 0; i < num_managers_; ++i) {
      download_managers_[i] = managers[i];
      managers[i] = NULL;
    }
  }

  ~DownloadIdTest() {
    for (size_t i = 0; i < num_managers_; ++i)
      download_managers_[i] = NULL;  // Releases & deletes.
  }

 protected:
  scoped_ptr<MockDownloadManagerDelegate> download_manager_delegate_;
  scoped_refptr<DownloadManager> download_managers_[2];
  MessageLoopForUI message_loop_;
  // Necessary to delete |DownloadManager|s.
  BrowserThreadImpl ui_thread_;
  size_t num_managers_;

  DISALLOW_COPY_AND_ASSIGN(DownloadIdTest);
};

TEST_F(DownloadIdTest, Local) {
  DownloadId id1(download_managers_[0], 23);
  DownloadId id2(download_managers_[0], 25);

  EXPECT_EQ(23, id1.local());
  EXPECT_EQ(25, id2.local());
}

TEST_F(DownloadIdTest, Valid) {
  DownloadId id1(download_managers_[0], 23);
  DownloadId id2(download_managers_[0], -1);
  DownloadId id3(NULL, 23);
  DownloadId id4(NULL, -3456);

  EXPECT_TRUE(id1.IsValid());
  EXPECT_FALSE(id2.IsValid());
  EXPECT_FALSE(id3.IsValid());
}

TEST_F(DownloadIdTest, Equals) {
  DownloadId id1(download_managers_[0], 23);
  DownloadId id2(download_managers_[0], 23);

  EXPECT_EQ(DownloadId::Invalid(), DownloadId::Invalid());
  EXPECT_EQ(id1, id2);
}

TEST_F(DownloadIdTest, NotEqualsIndex) {
  DownloadId id1(download_managers_[0], 23);
  DownloadId id2(download_managers_[0], 24);

  EXPECT_FALSE(id1 == id2);
  EXPECT_LT(id1, id2);
}

TEST_F(DownloadIdTest, NotEqualsManager) {
  // Because it's sorted above, &download_managers_[1] > &download_managers_[0].
  EXPECT_LT(download_managers_[0].get(), download_managers_[1].get());
  DownloadId id1(download_managers_[0], 23);
  DownloadId id2(download_managers_[1], 23);
  DownloadId id3(download_managers_[1], 22);

  EXPECT_LT(DownloadId::Invalid(), id1);
  EXPECT_LT(DownloadId::Invalid(), id2);
  EXPECT_LT(DownloadId::Invalid(), id3);

  EXPECT_FALSE(id1 == id2);
  EXPECT_LT(id1, id2);

  EXPECT_FALSE(id2 == id3);
  EXPECT_LT(id3, id2);

  EXPECT_FALSE(id1 == id3);
  EXPECT_LT(id1, id3);
}

TEST_F(DownloadIdTest, HashMap) {
  DownloadId id1(download_managers_[0], 23);
  DownloadId id2(download_managers_[0], 24);
  DownloadId id3(download_managers_[1], 23);

  const int kLocalId[] = { 95, 1234567, -29 };

  typedef base::hash_map<DownloadId, int> DownloadIdMap;
  DownloadIdMap map;
  map[id1] = kLocalId[0];
  EXPECT_EQ(1U, map.size());
  EXPECT_EQ(kLocalId[0], map[id1]);
  DownloadIdMap::iterator last = map.end();
  EXPECT_FALSE(last == map.find(id1));
  EXPECT_TRUE(last == map.find(id2));
  EXPECT_TRUE(last == map.find(id3));

  map[id2] = kLocalId[1];
  EXPECT_EQ(2U, map.size());
  EXPECT_EQ(kLocalId[0], map[id1]);
  EXPECT_EQ(kLocalId[1], map[id2]);
  last = map.end();
  EXPECT_FALSE(last == map.find(id1));
  EXPECT_FALSE(last == map.find(id2));
  EXPECT_TRUE(last == map.find(id3));

  map[id3] = kLocalId[2];
  EXPECT_EQ(3U, map.size());
  EXPECT_EQ(kLocalId[0], map[id1]);
  EXPECT_EQ(kLocalId[1], map[id2]);
  EXPECT_EQ(kLocalId[2], map[id3]);
  last = map.end();
  EXPECT_FALSE(last == map.find(id1));
  EXPECT_FALSE(last == map.find(id2));
  EXPECT_FALSE(last == map.find(id3));

  EXPECT_FALSE(id1 == id2);
  EXPECT_LT(id1, id2);
}

