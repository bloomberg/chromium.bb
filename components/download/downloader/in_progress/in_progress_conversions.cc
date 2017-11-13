// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/in_progress_conversions.h"

#include <utility>

// TODO(jming): Write unit tests for conversion methods.
namespace download {

DownloadEntry InProgressConversions::DownloadEntryFromProto(
    const metadata_pb::DownloadEntry& proto) {
  DownloadEntry entry;
  entry.guid = proto.guid();
  entry.request_origin = proto.request_origin();
  return entry;
}

metadata_pb::DownloadEntry InProgressConversions::DownloadEntryToProto(
    const DownloadEntry& entry) {
  metadata_pb::DownloadEntry proto;
  proto.set_guid(entry.guid);
  proto.set_request_origin(entry.request_origin);
  return proto;
}

std::vector<DownloadEntry> InProgressConversions::DownloadEntriesFromProto(
    const metadata_pb::DownloadEntries& proto) {
  std::vector<DownloadEntry> entries;
  for (int i = 0; i < proto.entries_size(); i++)
    entries.push_back(DownloadEntryFromProto(proto.entries(i)));
  return entries;
}

metadata_pb::DownloadEntries InProgressConversions::DownloadEntriesToProto(
    const std::vector<DownloadEntry>& entries) {
  metadata_pb::DownloadEntries proto;
  for (size_t i = 0; i < entries.size(); i++) {
    metadata_pb::DownloadEntry* proto_entry = proto.add_entries();
    *proto_entry = DownloadEntryToProto(entries[i]);
  }
  return proto;
}

}  // namespace download
