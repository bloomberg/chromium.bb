// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFINE_PAGES_CORE_MODEL_TEMPORARY_PAGES_CONSISTENCY_CHECK_TASK_H_
#define COMPONENTS_OFFINE_PAGES_CORE_MODEL_TEMPORARY_PAGES_CONSISTENCY_CHECK_TASK_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class ClientPolicyController;
class OfflinePageMetadataStoreSQL;

// Task that does consistency check between temporary pages stored in metadata
// store and temporary archive files. No callback is required for this task.
// The task includes 3 steps:
// TODO(romax): https://crbug.com/770871. Move step 1 into a separate task
// since it's not supposed to run when Chrome is running.
// 1. Delete all temporary pages and their files if the files are in the
// previous temporary archives directory (the abandoned directory).
// 2. Iterate through all temporary entries in the DB, delete the ones that
// do not have associated files.
// 3. Iterate through all files in the temporary archives directory, delete the
// ones that do not have associated DB entries.
// NOTE: if the temporary archives directory is empty, only step 1 will be
// executed.
// TODO(romax): https://crbug.com/772171. This also needs to be revised during
// P2P sharing implementation.
class TemporaryPagesConsistencyCheckTask : public Task {
 public:
  TemporaryPagesConsistencyCheckTask(OfflinePageMetadataStoreSQL* store,
                                     ClientPolicyController* policy_controller,
                                     const base::FilePath& archives_dir,
                                     const base::FilePath& legacy_archives_dir);
  ~TemporaryPagesConsistencyCheckTask() override;

  // Task implementation
  void Run() override;

 private:
  void OnCheckConsistencyDone(bool result);

  // The store for consistency check. Not owned.
  OfflinePageMetadataStoreSQL* store_;
  // The client policy controller to get policies for temporary namespaces.
  // Not owned.
  ClientPolicyController* policy_controller_;
  base::FilePath archives_dir_;
  base::FilePath legacy_archives_dir_;

  base::WeakPtrFactory<TemporaryPagesConsistencyCheckTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(TemporaryPagesConsistencyCheckTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_TEMPORARY_PAGES_CONSISTENCY_CHECK_TASK_H_
