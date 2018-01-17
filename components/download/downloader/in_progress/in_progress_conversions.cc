// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/download/downloader/in_progress/in_progress_conversions.h"

#include <utility>
#include "base/logging.h"

namespace download {

DownloadEntry InProgressConversions::DownloadEntryFromProto(
    const metadata_pb::DownloadEntry& proto) {
  DownloadEntry entry;
  entry.guid = proto.guid();
  entry.request_origin = proto.request_origin();
  entry.download_source = DownloadSourceFromProto(proto.download_source());
  entry.ukm_download_id = proto.ukm_download_id();
  return entry;
}

metadata_pb::DownloadEntry InProgressConversions::DownloadEntryToProto(
    const DownloadEntry& entry) {
  metadata_pb::DownloadEntry proto;
  proto.set_guid(entry.guid);
  proto.set_request_origin(entry.request_origin);
  proto.set_download_source(DownloadSourceToProto(entry.download_source));
  proto.set_ukm_download_id(entry.ukm_download_id);
  return proto;
}

// static
DownloadSource InProgressConversions::DownloadSourceFromProto(
    metadata_pb::DownloadSource download_source) {
  switch (download_source) {
    case metadata_pb::DownloadSource::UNKNOWN:
      return DownloadSource::UNKNOWN;
    case metadata_pb::DownloadSource::NAVIGATION:
      return DownloadSource::NAVIGATION;
    case metadata_pb::DownloadSource::DRAG_AND_DROP:
      return DownloadSource::DRAG_AND_DROP;
    case metadata_pb::DownloadSource::FROM_RENDERER:
      return DownloadSource::FROM_RENDERER;
    case metadata_pb::DownloadSource::EXTENSION_API:
      return DownloadSource::EXTENSION_API;
    case metadata_pb::DownloadSource::EXTENSION_INSTALLER:
      return DownloadSource::EXTENSION_INSTALLER;
    case metadata_pb::DownloadSource::INTERNAL_API:
      return DownloadSource::INTERNAL_API;
    case metadata_pb::DownloadSource::WEB_CONTENTS_API:
      return DownloadSource::WEB_CONTENTS_API;
    case metadata_pb::DownloadSource::OFFLINE_PAGE:
      return DownloadSource::OFFLINE_PAGE;
    case metadata_pb::DownloadSource::CONTEXT_MENU:
      return DownloadSource::CONTEXT_MENU;
  }
  NOTREACHED();
  return DownloadSource::UNKNOWN;
}

// static
metadata_pb::DownloadSource InProgressConversions::DownloadSourceToProto(
    DownloadSource download_source) {
  switch (download_source) {
    case DownloadSource::UNKNOWN:
      return metadata_pb::DownloadSource::UNKNOWN;
    case DownloadSource::NAVIGATION:
      return metadata_pb::DownloadSource::NAVIGATION;
    case DownloadSource::DRAG_AND_DROP:
      return metadata_pb::DownloadSource::DRAG_AND_DROP;
    case DownloadSource::FROM_RENDERER:
      return metadata_pb::DownloadSource::FROM_RENDERER;
    case DownloadSource::EXTENSION_API:
      return metadata_pb::DownloadSource::EXTENSION_API;
    case DownloadSource::EXTENSION_INSTALLER:
      return metadata_pb::DownloadSource::EXTENSION_INSTALLER;
    case DownloadSource::INTERNAL_API:
      return metadata_pb::DownloadSource::INTERNAL_API;
    case DownloadSource::WEB_CONTENTS_API:
      return metadata_pb::DownloadSource::WEB_CONTENTS_API;
    case DownloadSource::OFFLINE_PAGE:
      return metadata_pb::DownloadSource::OFFLINE_PAGE;
    case DownloadSource::CONTEXT_MENU:
      return metadata_pb::DownloadSource::CONTEXT_MENU;
  }
  NOTREACHED();
  return metadata_pb::DownloadSource::UNKNOWN;
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
