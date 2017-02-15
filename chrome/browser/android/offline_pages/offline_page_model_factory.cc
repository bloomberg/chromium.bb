// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_model_factory.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model_impl.h"
#include "content/public/browser/browser_thread.h"

namespace offline_pages {

OfflinePageModelFactory::OfflinePageModelFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePageModel",
          BrowserContextDependencyManager::GetInstance()) {
}

// static
OfflinePageModelFactory* OfflinePageModelFactory::GetInstance() {
  return base::Singleton<OfflinePageModelFactory>::get();
}

// static
OfflinePageModel* OfflinePageModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<OfflinePageModelImpl*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

KeyedService* OfflinePageModelFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      content::BrowserThread::GetBlockingPool()->GetSequencedTaskRunner(
          content::BrowserThread::GetBlockingPool()->GetSequenceToken());

  base::FilePath store_path =
      profile->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStore> metadata_store(
      new OfflinePageMetadataStoreSQL(background_task_runner, store_path));

  base::FilePath archives_dir =
      profile->GetPath().Append(chrome::kOfflinePageArchivesDirname);

  return new OfflinePageModelImpl(std::move(metadata_store), archives_dir,
                                  background_task_runner);
}

}  // namespace offline_pages
