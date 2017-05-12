// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_PERSISTENT_STORE_H_
#define COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_PERSISTENT_STORE_H_

#include <memory>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/feature_engagement_tracker/internal/proto/event.pb.h"
#include "components/feature_engagement_tracker/internal/store.h"
#include "components/leveldb_proto/proto_database.h"

namespace feature_engagement_tracker {

// A PersistentStore provides a DB layer that persists the data to disk.  The
// data is retrieved once during the load process and after that this store is
// write only.  Data will be persisted asynchronously so it is not guaranteed to
// always save every write during shutdown.
class PersistentStore : public Store {
 public:
  // Builds a PersistentStore backed by the ProtoDatabase |db|.  The database
  // will be loaded and/or created at |storage_dir|.
  PersistentStore(const base::FilePath& storage_dir,
                  std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db);
  ~PersistentStore() override;

  // Store implementation.
  void Load(const OnLoadedCallback& callback) override;
  bool IsReady() const override;
  void WriteEvent(const Event& event) override;
  void DeleteEvent(const std::string& event_name) override;

 private:
  void OnInitComplete(const OnLoadedCallback& callback, bool success);
  void OnLoadComplete(const OnLoadedCallback& callback,
                      bool success,
                      std::unique_ptr<std::vector<Event>> entries);

  const base::FilePath storage_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<Event>> db_;

  // Whether or not the underlying ProtoDatabase is ready.  This will be false
  // until the OnLoadedCallback is broadcast.  It will also be false if loading
  // fails.
  bool ready_;

  base::WeakPtrFactory<PersistentStore> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(PersistentStore);
};

}  // namespace feature_engagement_tracker

#endif  // COMPONENTS_FEATURE_ENGAGEMENT_TRACKER_INTERNAL_PERSISTENT_STORE_H_
