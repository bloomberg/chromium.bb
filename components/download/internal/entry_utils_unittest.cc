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
  Entry entry4 = test::BuildBasicEntry(Entry::State::COMPLETE);
  Entry entry5 = test::BuildBasicEntry(Entry::State::AVAILABLE);

  std::vector<Entry*> entries = {&entry1, &entry2, &entry3, &entry4, &entry5};
  std::vector<std::string> expected_list = {
      entry1.guid, entry2.guid, entry3.guid, entry4.guid, entry5.guid};
  std::vector<std::string> expected_pruned_list = {entry1.guid, entry2.guid,
                                                   entry3.guid, entry5.guid};
  // If DownloadClient::TEST isn't a valid Client, all of the associated entries
  // should move to the DownloadClient::INVALID bucket.
  auto mapped1 = util::MapEntriesToClients(std::set<DownloadClient>(), entries,
                                           std::set<Entry::State>());
  EXPECT_EQ(1U, mapped1.size());
  EXPECT_NE(mapped1.end(), mapped1.find(DownloadClient::INVALID));
  EXPECT_EQ(mapped1.end(), mapped1.find(DownloadClient::TEST));

  auto list1 = mapped1.find(DownloadClient::INVALID)->second;
  EXPECT_EQ(5U, list1.size());
  EXPECT_TRUE(
      std::equal(expected_list.begin(), expected_list.end(), list1.begin()));

  // If DownloadClient::TEST is a valid Client, it should have the associated
  // entries.
  std::set<DownloadClient> clients = {DownloadClient::TEST};
  auto mapped2 =
      util::MapEntriesToClients(clients, entries, std::set<Entry::State>());
  EXPECT_EQ(1U, mapped2.size());
  EXPECT_NE(mapped2.end(), mapped2.find(DownloadClient::TEST));
  EXPECT_EQ(mapped2.end(), mapped2.find(DownloadClient::INVALID));

  auto list2 = mapped2.find(DownloadClient::TEST)->second;
  EXPECT_EQ(5U, list2.size());
  EXPECT_TRUE(
      std::equal(expected_list.begin(), expected_list.end(), list2.begin()));

  // If we are pruning entries with certain states, make sure those entries
  // don't show up in the results.
  std::set<Entry::State> ignored_states = {Entry::State::ACTIVE,
                                           Entry::State::COMPLETE};
  auto mapped3 = util::MapEntriesToClients(clients, entries, ignored_states);
  EXPECT_EQ(1U, mapped3.size());
  auto list3 = mapped3.find(DownloadClient::TEST)->second;
  EXPECT_EQ(4U, list3.size());
  EXPECT_TRUE(std::equal(expected_pruned_list.begin(),
                         expected_pruned_list.end(), list3.begin()));
}

TEST(DownloadServiceEntryUtilsTest, GetSchedulingCriteria) {
  Entry entry1 = test::BuildBasicEntry();
  Entry entry2 = test::BuildBasicEntry();
  Entry entry3 = test::BuildBasicEntry();
  Entry entry4 = test::BuildBasicEntry();

  entry1.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  entry1.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;

  entry2.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::NONE;
  entry2.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_SENSITIVE;

  entry3.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::UNMETERED;
  entry3.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;

  entry4.scheduling_params.network_requirements =
      SchedulingParams::NetworkRequirements::NONE;
  entry4.scheduling_params.battery_requirements =
      SchedulingParams::BatteryRequirements::BATTERY_INSENSITIVE;

  Model::EntryList list1 = {&entry1};
  Model::EntryList list2 = {&entry1, &entry2};
  Model::EntryList list3 = {&entry1, &entry3};
  Model::EntryList list4 = {&entry1, &entry2, &entry3};
  Model::EntryList list5 = {&entry1, &entry4};

  EXPECT_EQ(Criteria(true, true), util::GetSchedulingCriteria(list1));
  EXPECT_EQ(Criteria(true, false), util::GetSchedulingCriteria(list2));
  EXPECT_EQ(Criteria(false, true), util::GetSchedulingCriteria(list3));
  EXPECT_EQ(Criteria(false, false), util::GetSchedulingCriteria(list4));
  EXPECT_EQ(Criteria(false, false), util::GetSchedulingCriteria(list5));
}

}  // namespace download
