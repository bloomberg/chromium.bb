// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CREATE_ARCHIVE_TASK_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CREATE_ARCHIVE_TASK_H_

#include <memory>

#include "base/macros.h"
#include "base/time/clock.h"
#include "components/offline_pages/core/offline_page_archiver.h"
#include "components/offline_pages/core/offline_page_model.h"
#include "components/offline_pages/core/offline_page_types.h"
#include "components/offline_pages/core/task.h"

namespace base {
class FilePath;
}  // namespace base

namespace offline_pages {

// Task that start an archive creation using the OfflinePageArchiver passed in,
// along with the SavePageParams.
// The task will complete before the archive creation actually finishes, in
// order to allow multiple archivers to work in parallel.
// The lifetime of the archiver needs to be managed by the caller, this task
// will not own the archiver. The ownership of the callback passed in will be
// transferred to the archiver, so that it can still get executed after this
// task gets destroyed.
// TODO(romax): Verify that this task is actually needed. If we don't access the
// database, making this a task seems to much, and it can be simplified as a
// method.
class CreateArchiveTask : public Task {
 public:
  // The arguments of the callback are (in order):
  // - Proposed OfflinePageItem, filled with information from SavePageParams,
  // - Pointer to the archiver.
  // - ArchiverResult of the archive creation,
  // - The saved url, which is acquired from the WebContent of the archiver,
  // - The file path to the saved archive,
  // - Title of the saved page,
  // - Size of the saved file.
  // - Digest of the saved file.
  // TODO(romax): simplify the callback and related interface if possible. The
  // url, path, title and file size can be filled into the OfflinePageItem
  // during archive creation.
  typedef base::Callback<void(OfflinePageItem,
                              OfflinePageArchiver*,
                              OfflinePageArchiver::ArchiverResult,
                              const GURL&,
                              const base::FilePath&,
                              const base::string16&,
                              int64_t,
                              const std::string&)>
      CreateArchiveTaskCallback;

  CreateArchiveTask(const base::FilePath& archives_dir,
                    const OfflinePageModel::SavePageParams& save_page_params,
                    OfflinePageArchiver* archiver,
                    const CreateArchiveTaskCallback& callback);
  ~CreateArchiveTask() override;

  // Task implementation.
  void Run() override;

  void set_clock_for_testing(std::unique_ptr<base::Clock> clock) {
    clock_ = std::move(clock);
  }

  void set_skip_clearing_original_url_for_testing() {
    skip_clearing_original_url_for_testing_ = true;
  }

 private:
  void CreateArchive();
  void InformCreateArchiveFailed(OfflinePageArchiver::ArchiverResult result,
                                 const OfflinePageItem& proposed_page);

  // The directory to save the archive.
  base::FilePath archives_dir_;
  OfflinePageModel::SavePageParams save_page_params_;
  // The archiver used in the task. Not owned.
  OfflinePageArchiver* archiver_;
  CreateArchiveTaskCallback callback_;
  std::unique_ptr<base::Clock> clock_;

  bool skip_clearing_original_url_for_testing_;

  DISALLOW_COPY_AND_ASSIGN(CreateArchiveTask);
};

}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_MODEL_CREATE_ARCHIVE_TASK_H_
