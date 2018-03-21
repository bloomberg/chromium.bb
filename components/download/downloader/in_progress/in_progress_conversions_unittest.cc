// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/in_progress_conversions.h"

#include "components/download/public/common/download_url_parameters.h"
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
  EXPECT_EQ(false, entry.fetch_error_body);
  EXPECT_TRUE(entry.request_headers.empty());
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));

  // Entry with guid, request origin and download source.
  entry.guid = "guid";
  entry.request_origin = "request origin";
  entry.download_source = DownloadSource::DRAG_AND_DROP;
  entry.ukm_download_id = 123;
  entry.bytes_wasted = 1234;
  entry.fetch_error_body = true;
  entry.request_headers.emplace_back(
      std::make_pair<std::string, std::string>("123", "456"));
  entry.request_headers.emplace_back(
      std::make_pair<std::string, std::string>("ABC", "def"));
  EXPECT_EQ(entry, DownloadEntryFromProto(DownloadEntryToProto(entry)));
}

TEST_F(InProgressConversionsTest, DownloadEntries) {
  // Entries vector with no entries.
  std::vector<DownloadEntry> entries;
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with one entry.
  DownloadUrlParameters::RequestHeadersType request_headers;
  entries.push_back(DownloadEntry("guid", "request origin",
                                  DownloadSource::UNKNOWN, false,
                                  request_headers, 123));
  EXPECT_EQ(entries, DownloadEntriesFromProto(DownloadEntriesToProto(entries)));

  // Entries vector with multiple entries.
  request_headers.emplace_back(
      DownloadUrlParameters::RequestHeadersNameValuePair("key", "value"));
  entries.push_back(DownloadEntry("guid2", "request origin",
                                  DownloadSource::UNKNOWN, true,
                                  request_headers, 456));
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

TEST_F(InProgressConversionsTest, HttpRequestHeaders) {
  std::pair<std::string, std::string> header;
  EXPECT_EQ(header,
            HttpRequestHeaderFromProto(HttpRequestHeaderToProto(header)));
  header = std::make_pair("123", "456");
  EXPECT_EQ(header,
            HttpRequestHeaderFromProto(HttpRequestHeaderToProto(header)));
}

}  // namespace download
