// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/internal/entry_utils.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "components/download/internal/test/entry_utils.h"
#include "components/download/public/clients.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

TEST(DownloadServiceEntryUtilsTest, TestGetNumberOfEntriesForClient_NoEntries) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();

  std::vector<Entry*> entries = {&entry1, &entry2, &entry3};

  EXPECT_EQ(
      0U, util::GetNumberOfEntriesForClient(DownloadClient::INVALID, entries));
  EXPECT_EQ(3U,
            util::GetNumberOfEntriesForClient(DownloadClient::TEST, entries));
}

TEST(DownloadServiceEntryUtilsTest, MapEntriesToClients) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();

  std::vector<Entry*> entries = {&entry1, &entry2, &entry3};
  std::vector<std::string> expected_list = {entry1.guid, entry2.guid,
                                            entry3.guid};

  // If DownloadClient::TEST isn't a valid Client, all of the associated entries
  // should move to the DownloadClient::INVALID bucket.
  auto mapped1 = util::MapEntriesToClients(std::set<DownloadClient>(), entries);
  EXPECT_EQ(1U, mapped1.size());
  EXPECT_NE(mapped1.end(), mapped1.find(DownloadClient::INVALID));
  EXPECT_EQ(mapped1.end(), mapped1.find(DownloadClient::TEST));

  auto list1 = mapped1.find(DownloadClient::INVALID)->second;
  EXPECT_EQ(3U, list1.size());
  EXPECT_TRUE(
      std::equal(expected_list.begin(), expected_list.end(), list1.begin()));

  // If DownloadClient::TEST is a valid Client, it should have the associated
  // entries.
  std::set<DownloadClient> clients = {DownloadClient::TEST};
  auto mapped2 = util::MapEntriesToClients(clients, entries);
  EXPECT_EQ(1U, mapped2.size());
  EXPECT_NE(mapped2.end(), mapped2.find(DownloadClient::TEST));
  EXPECT_EQ(mapped2.end(), mapped2.find(DownloadClient::INVALID));

  auto list2 = mapped2.find(DownloadClient::TEST)->second;
  EXPECT_EQ(3U, list2.size());
  EXPECT_TRUE(
      std::equal(expected_list.begin(), expected_list.end(), list2.begin()));
}

}  // namespace download
