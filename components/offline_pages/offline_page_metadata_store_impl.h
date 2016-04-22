// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_IMPL_H_
#define COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_IMPL_H_

#include <stdint.h>

#include <vector>

#include "base/files/file_path.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "components/leveldb_proto/proto_database.h"
#include "components/offline_pages/offline_page_metadata_store.h"

namespace base {
class SequencedTaskRunner;
}

namespace offline_pages {

class OfflinePageEntry;

// Implements OfflinePageMetadataStore using leveldb_proto::ProtoDatabase
// component. Stores metadata of offline pages as serialized protobufs in a
// LevelDB key/value pairs.
// Underlying implementation guarantees that all of the method calls will be
// executed sequentially, and started operations will finish even after the
// store is already destroyed (callbacks will be called).
class OfflinePageMetadataStoreImpl : public OfflinePageMetadataStore {
 public:
  OfflinePageMetadataStoreImpl(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);
  ~OfflinePageMetadataStoreImpl() override;

  // OfflinePageMetadataStore implementation:
  void Load(const LoadCallback& callback) override;
  void AddOrUpdateOfflinePage(const OfflinePageItem& offline_page_record,
                              const UpdateCallback& callback) override;
  void RemoveOfflinePages(const std::vector<int64_t>& offline_ids,
                          const UpdateCallback& callback) override;
  void Reset(const ResetCallback& callback) override;

 private:
  friend class OfflinePageMetadataStoreImplTest;

  void LoadContinuation(const LoadCallback& callback, bool success);
  void LoadDone(const LoadCallback& callback,
                bool success,
                std::unique_ptr<std::vector<OfflinePageEntry>> entries);
  void NotifyLoadResult(const LoadCallback& callback,
                        LoadStatus status,
                        const std::vector<OfflinePageItem>& result);

  // Implements the update.
  void UpdateEntries(std::unique_ptr<leveldb_proto::ProtoDatabase<
                         OfflinePageEntry>::KeyEntryVector> entries_to_save,
                     std::unique_ptr<std::vector<std::string>> keys_to_remove,
                     const UpdateCallback& callback);

  void UpdateDone(const OfflinePageMetadataStore::UpdateCallback& callback,
                  bool success);

  void ResetDone(const ResetCallback& callback, bool success);

  // Called when a database has to be migrated from old bookmark ids
  // to new offline ids.
  // TODO(bburns): Perhaps remove this eventually?
  void DatabaseUpdateDone(const OfflinePageMetadataStore::LoadCallback& cb,
                          LoadStatus status,
                          const std::vector<OfflinePageItem>& result,
                          bool success);

  scoped_refptr<base::SequencedTaskRunner> background_task_runner_;
  base::FilePath database_dir_;
  std::unique_ptr<leveldb_proto::ProtoDatabase<OfflinePageEntry>> database_;

  base::WeakPtrFactory<OfflinePageMetadataStoreImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMetadataStoreImpl);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_OFFLINE_PAGE_METADATA_STORE_IMPL_H_
