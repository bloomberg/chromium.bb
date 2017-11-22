// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFINE_PAGES_CORE_MODEL_PERSISTENT_PAGES_CONSISTENCY_CHECK_TASK_H_
#define COMPONENTS_OFFINE_PAGES_CORE_MODEL_PERSISTENT_PAGES_CONSISTENCY_CHECK_TASK_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class ClientPolicyController;
class OfflinePageMetadataStoreSQL;

// Task that does consistency check between persistent pages stored in metadata
// store and persistent archive files. No callback is required for this task.
// The task includes 2 steps:
// 1. Iterate through all persistent entries in the DB, delete the ones that
// do not have associated files.
// 2. Iterate through all MHTML files in the persistent archives directory,
// delete the ones that do not have associated DB entries.
class PersistentPagesConsistencyCheckTask : public Task {
 public:
  PersistentPagesConsistencyCheckTask(OfflinePageMetadataStoreSQL* store,
                                      ClientPolicyController* policy_controller,
                                      const base::FilePath& archives_dir);
  ~PersistentPagesConsistencyCheckTask() override;

  // Task implementation
  void Run() override;

 private:
  void OnCheckConsistencyDone(bool result);

  // The store for consistency check. Not owned.
  OfflinePageMetadataStoreSQL* store_;
  // The client policy controller to get policies for persistent namespaces.
  // Not owned.
  ClientPolicyController* policy_controller_;
  base::FilePath archives_dir_;

  base::WeakPtrFactory<PersistentPagesConsistencyCheckTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(PersistentPagesConsistencyCheckTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_PERSISTENT_PAGES_CONSISTENCY_CHECK_TASK_H_
