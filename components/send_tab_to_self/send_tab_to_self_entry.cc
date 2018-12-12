// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/send_tab_to_self_entry.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "components/sync/protocol/send_tab_to_self_specifics.pb.h"

namespace send_tab_to_self {

SendTabToSelfEntry::SendTabToSelfEntry(const GURL& url,
                                       const std::string& title,
                                       const base::Time shared_time) {
  NOTIMPLEMENTED();
}

SendTabToSelfEntry::SendTabToSelfEntry(const std::string& title,
                                       const GURL& url,
                                       int64_t shared_time) {
  NOTIMPLEMENTED();
}

SendTabToSelfEntry::~SendTabToSelfEntry() {
  NOTIMPLEMENTED();
}

const GURL& SendTabToSelfEntry::GetURL() const {
  return url_;
}

const std::string& SendTabToSelfEntry::GetTitle() const {
  return title_;
}

base::Time SendTabToSelfEntry::GetSharedTime() const {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(shared_time_us_));
}

std::unique_ptr<sync_pb::SendTabToSelfSpecifics> SendTabToSelfEntry::AsProto()
    const {
  NOTIMPLEMENTED();
  return nullptr;
}

std::unique_ptr<SendTabToSelfEntry> SendTabToSelfEntry::FromProto(
    const sync_pb::SendTabToSelfSpecifics& pb_entry,
    base::Time now) {
  NOTIMPLEMENTED();
  return nullptr;
}

}  // namespace send_tab_to_self
