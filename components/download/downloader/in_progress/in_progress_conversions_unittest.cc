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

  // Entry with guid, request origin and download source.
  entry.guid = "guid";
  entry.request_origin = "request origin";
  entry.download_source = DownloadSource::DRAG_AND_DROP;
  entry.ukm_download_id = 123;
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));
}

TEST_F(InProgressConversionsTest, DownloadEntries) {
  // Entries vector with no entries.
  std::vector<DownloadEntry> entries;
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with one entry.
  entries.push_back(
      DownloadEntry("guid", "request origin", DownloadSource::UNKNOWN, 123));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with multiple entries.
  entries.push_back(
      DownloadEntry("guid2", "request origin", DownloadSource::UNKNOWN, 456));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));
}

TEST_F(InProgressConversionsTest, DownloadSource) {
  DownloadSource sources[] = {
      DownloadSource::UNKNOWN,       DownloadSource::NAVIGATION,
      DownloadSource::DRAG_AND_DROP, DownloadSource::FROM_RENDERER,
      DownloadSource::EXTENSION_API, DownloadSource::EXTENSION_INSTALLER,
      DownloadSource::INTERNAL_API,  DownloadSource::WEB_CONTENTS_API,
      DownloadSource::OFFLINE_PAGE,  DownloadSource::CONTEXT_MENU};

  for (auto source : sources) {
    EXPECT_EQ(source, DownloadSourceFromProto(DownloadSourceToProto(source)));
  }
}

}  // namespace download
