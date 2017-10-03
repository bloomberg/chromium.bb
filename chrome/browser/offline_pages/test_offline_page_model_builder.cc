// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/test_offline_page_model_builder.h"

#include <utility>

#include "base/memory/ref_counted.h"
#include "base/path_service.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/common/chrome_constants.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/offline_page_model_impl.h"
#include "components/offline_pages/core/offline_page_test_store.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

std::unique_ptr<KeyedService> BuildTestOfflinePageModel(
    content::BrowserContext* context) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  std::unique_ptr<OfflinePageTestStore> metadata_store(
      new OfflinePageTestStore(task_runner));

  base::FilePath persistent_archives_dir =
      context->GetPath().Append(chrome::kOfflinePageArchivesDirname);
  // If PathService::Get returns false, the temporary_archives_dir will be
  // empty, and no temporary pages will be saved during this chrome lifecycle.
  base::FilePath temporary_archives_dir;
  if (PathService::Get(base::DIR_CACHE, &temporary_archives_dir)) {
    temporary_archives_dir =
        temporary_archives_dir.Append(chrome::kOfflinePageArchivesDirname);
  }
  std::unique_ptr<ArchiveManager> archive_manager(new ArchiveManager(
      temporary_archives_dir, persistent_archives_dir, task_runner));

  return std::unique_ptr<KeyedService>(new OfflinePageModelImpl(
      std::move(metadata_store), std::move(archive_manager), task_runner));
}

}  // namespace offline_pages
