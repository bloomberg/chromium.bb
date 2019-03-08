// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_ICON_STORE_H_
#define CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_ICON_STORE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "chrome/browser/notifications/proto/icon.pb.h"
#include "chrome/browser/notifications/scheduler/proto_db_collection_store.h"

namespace notifications {

class IconEntry;

// Used to read/write notification icon data from database. The icon is usually
// a large piece of data and should load only one icon at a time.
class IconStore : public ProtoDbCollectionStore<IconEntry, proto::Icon> {
 public:
  explicit IconStore(
      std::unique_ptr<leveldb_proto::ProtoDatabase<proto::Icon>> db);
  ~IconStore() override;

 private:
  // ProtoDbCollectionStore implementation.
  proto::Icon EntryToProto(const IconEntry& entry) override;
  std::unique_ptr<IconEntry> ProtoToEntry(proto::Icon& proto) override;

  DISALLOW_COPY_AND_ASSIGN(IconStore);
};

}  // namespace notifications

#endif  // CHROME_BROWSER_NOTIFICATIONS_SCHEDULER_ICON_STORE_H_
