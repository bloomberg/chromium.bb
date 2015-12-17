// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/test_offline_page_model_builder.h"

#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/common/chrome_constants.h"
#include "components/offline_pages/offline_page_model.h"
#include "components/offline_pages/offline_page_test_store.h"
#include "content/public/browser/browser_context.h"

namespace offline_pages {

scoped_ptr<KeyedService> BuildTestOfflinePageModel(
    content::BrowserContext* context) {
  scoped_refptr<base::SingleThreadTaskRunner> task_runner =
      base::ThreadTaskRunnerHandle::Get();

  scoped_ptr<OfflinePageTestStore> metadata_store(
      new OfflinePageTestStore(task_runner));

  base::FilePath archives_dir =
      context->GetPath().Append(chrome::kOfflinePageArchviesDirname);

  return scoped_ptr<KeyedService>(
      new OfflinePageModel(metadata_store.Pass(), archives_dir, task_runner));
}

}  // namespace offline_pages
