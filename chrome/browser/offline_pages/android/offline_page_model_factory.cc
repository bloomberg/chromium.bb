// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/offline_page_model_factory.h"

#include <utility>

#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "chrome/browser/offline_pages/android/cct_origin_observer.h"
#include "chrome/browser/offline_pages/fresh_offline_content_observer.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/core/archive_manager.h"
#include "components/offline_pages/core/offline_page_metadata_store_sql.h"
#include "components/offline_pages/core/offline_page_model_impl.h"

namespace offline_pages {

OfflinePageModelFactory::OfflinePageModelFactory()
    : BrowserContextKeyedServiceFactory(
          "OfflinePageModel",
          BrowserContextDependencyManager::GetInstance()) {}

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
      base::CreateSequencedTaskRunnerWithTraits({base::MayBlock()});

  base::FilePath store_path =
      profile->GetPath().Append(chrome::kOfflinePageMetadataDirname);
  std::unique_ptr<OfflinePageMetadataStore> metadata_store(
      new OfflinePageMetadataStoreSQL(background_task_runner, store_path));

  base::FilePath archives_dir =
      profile->GetPath().Append(chrome::kOfflinePageArchivesDirname);
  std::unique_ptr<ArchiveManager> archive_manager(
      new ArchiveManager(archives_dir, background_task_runner));

  OfflinePageModelImpl* model = new OfflinePageModelImpl(
      std::move(metadata_store), std::move(archive_manager),
      background_task_runner);

  CctOriginObserver::AttachToOfflinePageModel(model);

  FreshOfflineContentObserver::AttachToOfflinePageModel(model);

  return model;
}

}  // namespace offline_pages
