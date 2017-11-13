// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_IN_PROGRESS_IN_PROGRESS_CONVERSIONS_H_
#define COMPONENTS_DOWNLOAD_IN_PROGRESS_IN_PROGRESS_CONVERSIONS_H_

#include "base/macros.h"
#include "components/download/downloader/in_progress/download_entry.h"
#include "components/download/downloader/in_progress/proto/download_entry.pb.h"

namespace download {

class InProgressConversions {
 public:
  static DownloadEntry DownloadEntryFromProto(
      const metadata_pb::DownloadEntry& proto);

  static metadata_pb::DownloadEntry DownloadEntryToProto(
      const DownloadEntry& entry);

  static std::vector<DownloadEntry> DownloadEntriesFromProto(
      const metadata_pb::DownloadEntries& proto);

  static metadata_pb::DownloadEntries DownloadEntriesToProto(
      const std::vector<DownloadEntry>& entries);

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(InProgressConversions);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_IN_PROGRESS_IN_PROGRESS_CONVERSIONS_H_
