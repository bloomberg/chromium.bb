// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"

namespace send_tab_to_self {

namespace {

// Converts a time object to the format used in sync protobufs (ms since the
// Windows epoch).
int64_t TimeToProtoTime(const base::Time t) {
  return t.ToDeltaSinceWindowsEpoch().InMicroseconds();
}

// Converts a time field from sync protobufs to a time object.
base::Time ProtoTimeToTime(int64_t proto_t) {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(proto_t));
}

}  // namespace

SendTabToSelfEntry::SendTabToSelfEntry(const std::string& guid,
                                       const GURL& url,
                                       const std::string& title,
                                       base::Time shared_time,
                                       base::Time original_navigation_time,
                                       const std::string& device_name)
    : guid_(guid),
      url_(url),
      title_(title),
      device_name_(device_name),
      shared_time_(shared_time),
      original_navigation_time_(original_navigation_time) {
  DCHECK(!guid_.empty());
  DCHECK(url_.is_valid());
}

SendTabToSelfEntry::~SendTabToSelfEntry() {}

const std::string& SendTabToSelfEntry::GetGUID() const {
  return guid_;
}

const GURL& SendTabToSelfEntry::GetURL() const {
  return url_;
}

const std::string& SendTabToSelfEntry::GetTitle() const {
  return title_;
}

base::Time SendTabToSelfEntry::GetSharedTime() const {
  return shared_time_;
}

base::Time SendTabToSelfEntry::GetOriginalNavigationTime() const {
  return original_navigation_time_;
}

const std::string& SendTabToSelfEntry::GetDeviceName() const {
  return device_name_;
}

std::unique_ptr<sync_pb::SendTabToSelfSpecifics> SendTabToSelfEntry::AsProto()
    const {
  auto pb_entry = std::make_unique<sync_pb::SendTabToSelfSpecifics>();

  pb_entry->set_guid(GetGUID());
  pb_entry->set_title(GetTitle());
  pb_entry->set_url(GetURL().spec());
  pb_entry->set_shared_time_usec(TimeToProtoTime(GetSharedTime()));
  pb_entry->set_navigation_time_usec(
      TimeToProtoTime(GetOriginalNavigationTime()));
  pb_entry->set_device_name(GetDeviceName());

  return pb_entry;
}

std::unique_ptr<SendTabToSelfEntry> SendTabToSelfEntry::FromProto(
    const sync_pb::SendTabToSelfSpecifics& pb_entry,
    base::Time now) {
  std::string guid(pb_entry.guid());
  DCHECK(!guid.empty());

  GURL url(pb_entry.url());
  DCHECK(url.is_valid());

  base::Time shared_time = ProtoTimeToTime(pb_entry.shared_time_usec());
  if (shared_time > now) {
    shared_time = now;
  }

  base::Time navigation_time;
  if (pb_entry.has_navigation_time_usec()) {
    navigation_time = ProtoTimeToTime(pb_entry.navigation_time_usec());
  }

  return std::make_unique<SendTabToSelfEntry>(guid, url, pb_entry.title(),
                                              shared_time, navigation_time,
                                              pb_entry.device_name());
}

}  // namespace send_tab_to_self
