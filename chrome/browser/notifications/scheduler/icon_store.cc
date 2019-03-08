// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/notifications/scheduler/icon_store.h"

#include <utility>

#include "chrome/browser/notifications/scheduler/icon_entry.h"
#include "chrome/browser/notifications/scheduler/proto_conversion.h"

namespace notifications {

IconStore::IconStore(
    std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon>> db)
    : ProtoDbCollectionStore<IconEntry, proto::Icon>(std::move(db), "icon") {}

IconStore::~IconStore() = default;

proto::Icon IconStore::EntryToProto(const IconEntry& entry) {
  return IconEntryToProto(entry);
}

std::unique_ptr<IconEntry> IconStore::ProtoToEntry(proto::Icon& proto) {
  return IconProtoToEntry(proto);
}

}  // namespace notifications
