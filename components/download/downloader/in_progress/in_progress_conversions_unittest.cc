// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/in_progress_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace download {

class InProgressConversionsTest : public testing::Test,
                                  public InProgressConversions {
 public:
  ~InProgressConversionsTest() override {}
};

TEST_F(InProgressConversionsTest, DownloadEntry) {
  // Entry with no fields.
  DownloadEntry entry;
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));

  // Entry with guid and request origin.
  entry.guid = "guid";
  entry.request_origin = "request origin";
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));
}

TEST_F(InProgressConversionsTest, DownloadEntries) {
  // Entries vector with no entries.
  std::vector<DownloadEntry> entries;
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with one entry.
  entries.push_back(DownloadEntry("guid", "request origin"));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with multiple entries.
  entries.push_back(DownloadEntry("guid2", "request origin"));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));
}

}  // namespace download
