// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include <memory>

#include "base/test/simple_test_tick_clock.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace send_tab_to_self {

namespace {

bool IsEqualForTesting(const SendTabToSelfEntry& a,
                       const SendTabToSelfEntry& b) {
  return a.GetGUID() == b.GetGUID() && a.GetURL() == b.GetURL() &&
         a.GetTitle() == b.GetTitle() &&
         a.GetDeviceName() == b.GetDeviceName() &&
         a.GetSharedTime() == b.GetSharedTime() &&
         a.GetOriginalNavigationTime() == b.GetOriginalNavigationTime();
}

TEST(SendTabToSelfEntry, CompareEntries) {
  const SendTabToSelfEntry e1("1", GURL("http://example.com"), "bar",
                              base::Time::FromTimeT(10),
                              base::Time::FromTimeT(10), "device");
  const SendTabToSelfEntry e2("1", GURL("http://example.com"), "bar",
                              base::Time::FromTimeT(10),
                              base::Time::FromTimeT(10), "device");

  EXPECT_TRUE(IsEqualForTesting(e1, e2));
  const SendTabToSelfEntry e3("2", GURL("http://example.org"), "bar",
                              base::Time::FromTimeT(10),
                              base::Time::FromTimeT(10), "device");

  EXPECT_FALSE(IsEqualForTesting(e1, e3));
}

TEST(SendTabToSelfEntry, SharedTime) {
  SendTabToSelfEntry e("1", GURL("http://example.com"), "bar",
                       base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                       "device");
  EXPECT_EQ("bar", e.GetTitle());
  // Getters return Base::Time values.
  EXPECT_EQ(e.GetSharedTime(), base::Time::FromTimeT(10));
}

// Tests that the send tab to self entry is correctly encoded to
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, AsProto) {
  SendTabToSelfEntry entry("1", GURL("http://example.com"), "bar",
                           base::Time::FromTimeT(10), base::Time::FromTimeT(10),
                           "device");
  base::Time shared_time = entry.GetSharedTime();
  base::Time navigation_time = entry.GetSharedTime();

  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> pb_entry(entry.AsProto());
  EXPECT_EQ(pb_entry->url(), "http://example.com/");
  EXPECT_EQ(pb_entry->title(), "bar");
  EXPECT_EQ(pb_entry->shared_time_usec(),
            shared_time.ToDeltaSinceWindowsEpoch().InMicroseconds());

  EXPECT_EQ(pb_entry->navigation_time_usec(),
            navigation_time.ToDeltaSinceWindowsEpoch().InMicroseconds());
  EXPECT_EQ(pb_entry->device_name(), "device");
}

// Tests that the send tab to self entry is correctly parsed from
// sync_pb::SendTabToSelfSpecifics.
TEST(SendTabToSelfEntry, FromProto) {
  std::unique_ptr<sync_pb::SendTabToSelfSpecifics> pb_entry =
      std::make_unique<sync_pb::SendTabToSelfSpecifics>();
  pb_entry->set_guid("1");
  pb_entry->set_url("http://example.com/");
  pb_entry->set_title("title");
  pb_entry->set_device_name("device");
  pb_entry->set_shared_time_usec(1);
  pb_entry->set_navigation_time_usec(1);

  std::unique_ptr<SendTabToSelfEntry> entry(
      SendTabToSelfEntry::FromProto(*pb_entry, base::Time::FromTimeT(10)));

  EXPECT_EQ(entry->GetGUID(), "1");
  EXPECT_EQ(entry->GetURL().spec(), "http://example.com/");
  EXPECT_EQ(entry->GetTitle(), "title");
  EXPECT_EQ(entry->GetDeviceName(), "device");
  EXPECT_EQ(entry->GetSharedTime().ToDeltaSinceWindowsEpoch().InMicroseconds(),
            1);
  EXPECT_EQ(entry->GetOriginalNavigationTime()
                .ToDeltaSinceWindowsEpoch()
                .InMicroseconds(),
            1);
}

}  // namespace

}  // namespace send_tab_to_self
