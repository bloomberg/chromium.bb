// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_H_

#include <stdint.h>

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/memory/weak_ptr.h"
#include "base/task_runner_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "components/offline_pages/core/offline_page_item.h"
#include "components/offline_pages/core/offline_store_types.h"
#include "components/offline_pages/task/sql_store_base.h"

namespace base {
class SequencedTaskRunner;
}

namespace sql {
class Database;
}

namespace offline_pages {
typedef StoreUpdateResult<OfflinePageItem> OfflinePagesUpdateResult;

// OfflinePageMetadataStore keeps metadata for the offline pages in an SQLite
// database.
//
// This store has a history of schema updates in pretty much every release.
// Original schema was delivered in M52. Since then, the following changes
// happened:
// * In M53 expiration_time was added,
// * In M54 title was added,
// * In M55 we dropped the following fields (never used): version, status,
//   offline_url, user_initiated.
// * In M56 original_url was added.
// * In M57 expiration_time was dropped. Existing expired pages would be
//   removed when metadata consistency check happens.
// * In M58-M60 there were no changes.
// * In M61 request_origin was added.
// * In M62 system_download_id, file_missing_time, upgrade_attempt and digest
//   were added to support P2P sharing feature.
//
// Here is a procedure to update the schema for this store:
// * Decide how to detect that the store is on a particular version, which
//   typically means that a certain field exists or is missing. This happens in
//   Upgrade section of |CreateSchema|
// * Work out appropriate change and apply it to all existing upgrade paths. In
//   the interest of performing a single update of the store, it upgrades from a
//   detected version to the current one. This means that when making a change,
//   more than a single query may have to be updated (in case of fields being
//   removed or needed to be initialized to a specific, non-default value).
//   Such approach is preferred to doing N updates for every changed version on
//   a startup after browser update.
// * New upgrade method should specify which version it is upgrading from, e.g.
//   |UpgradeFrom54|.
// * Upgrade should use |UpgradeWithQuery| and simply specify SQL command to
//   move data from old table (prefixed by temp_) to the new one.
class OfflinePageMetadataStore : public SqlStoreBase {
 public:
  // This is the first version saved in the meta table, which was introduced in
  // the store in M65. It is set once a legacy upgrade is run successfully for
  // the last time in |UpgradeFromLegacyVersion|.
  static const int kFirstPostLegacyVersion = 1;
  static const int kCurrentVersion = 3;
  static const int kCompatibleVersion = kFirstPostLegacyVersion;

  // TODO(fgorski): Move to private and expose ForTest factory.
  // Applies in PrefetchStore as well.
  // Creates the store in memory. Should only be used for testing.
  explicit OfflinePageMetadataStore(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);

  // Creates the store with database pointing to provided directory.
  OfflinePageMetadataStore(
      scoped_refptr<base::SequencedTaskRunner> background_task_runner,
      const base::FilePath& database_dir);

  ~OfflinePageMetadataStore() override;

  // Helper function used to force incorrect state for testing purposes.
  StoreState GetStateForTesting() const;

 protected:
  // SqlStoreBase:
  base::OnceCallback<bool(sql::Database* db)> GetSchemaInitializationFunction()
      override;
  void OnOpenStart(base::TimeTicks last_open_time) override;
  void OnOpenDone(bool success) override;
  void OnTaskBegin(bool is_initialized) override;
  void OnTaskRunComplete() override;
  void OnTaskReturnComplete() override;
  void OnCloseStart(InitializationStatus status_before_close) override;
  void OnCloseComplete() override;
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_OFFLINE_PAGE_METADATA_STORE_H_
