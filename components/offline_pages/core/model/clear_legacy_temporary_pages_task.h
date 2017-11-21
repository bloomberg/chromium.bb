// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_LEGACY_TEMPORARY_PAGES_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_LEGACY_TEMPORARY_PAGES_TASK_H_

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "components/offline_pages/core/task.h"

namespace offline_pages {

class ClientPolicyController;
class OfflinePageMetadataStoreSQL;

// Task that delete all temporary pages and their files if the files are in the
// legacy archives directory.
// This task is supposed to be executed only when Chrome starts, in order to
// complete the consistency check for temporary pages in legacy archives
// directory.
class ClearLegacyTemporaryPagesTask : public Task {
 public:
  ClearLegacyTemporaryPagesTask(OfflinePageMetadataStoreSQL* store,
                                ClientPolicyController* policy_controller,
                                const base::FilePath& legacy_archives_dir);
  ~ClearLegacyTemporaryPagesTask() override;

  // Task implementation
  void Run() override;

 private:
  void OnClearLegacyTemporaryPagesDone(bool result);

  // The store for clearing legacy pages. Not owned.
  OfflinePageMetadataStoreSQL* store_;
  // The client policy controller to get temporary namespaces. Not owned.
  ClientPolicyController* policy_controller_;
  base::FilePath legacy_archives_dir_;

  base::WeakPtrFactory<ClearLegacyTemporaryPagesTask> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(ClearLegacyTemporaryPagesTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CLEAR_LEGACY_TEMPORARY_PAGES_TASK_H_
