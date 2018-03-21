// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_IN_PROGRESS_IN_PROGRESS_CONVERSIONS_H_
#define COMPONENTS_DOWNLOAD_IN_PROGRESS_IN_PROGRESS_CONVERSIONS_H_

#include "base/macros.h"
#include "components/download/downloader/in_progress/download_entry.h"
#include "components/download/downloader/in_progress/proto/download_entry.pb.h"
#include "components/download/downloader/in_progress/proto/download_source.pb.h"

namespace download {

class InProgressConversions {
 public:
  static DownloadEntry DownloadEntryFromProto(
      const metadata_pb::DownloadEntry& proto);

  static metadata_pb::DownloadEntry DownloadEntryToProto(
      const DownloadEntry& entry);

  static DownloadSource DownloadSourceFromProto(
      metadata_pb::DownloadSource download_source);

  static metadata_pb::DownloadSource DownloadSourceToProto(
      DownloadSource download_source);

  static std::vector<DownloadEntry> DownloadEntriesFromProto(
      const metadata_pb::DownloadEntries& proto);

  static metadata_pb::DownloadEntries DownloadEntriesToProto(
      const std::vector<DownloadEntry>& entries);

  static metadata_pb::HttpRequestHeader HttpRequestHeaderToProto(
      const std::pair<std::string, std::string>& header);

  static std::pair<std::string, std::string> HttpRequestHeaderFromProto(
      const metadata_pb::HttpRequestHeader& proto);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_IN_PROGRESS_IN_PROGRESS_CONVERSIONS_H_
