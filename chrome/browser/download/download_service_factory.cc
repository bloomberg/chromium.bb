// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service_factory.h"

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/task_scheduler/post_task.h"
#include "base/task_scheduler/task_traits.h"
#include "chrome/browser/download/download_task_scheduler_impl.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_constants.h"
#include "components/download/content/factory/download_service_factory.h"
#include "components/download/public/clients.h"
#include "components/download/public/download_service.h"
#include "components/download/public/task_scheduler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/offline_pages/features/features.h"
#include "content/public/browser/browser_context.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/service/download_task_scheduler.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/offline_prefetch_download_client.h"
#endif

// static
DownloadServiceFactory* DownloadServiceFactory::GetInstance() {
  return base::Singleton<DownloadServiceFactory>::get();
}

// static
download::DownloadService* DownloadServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<download::DownloadService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

DownloadServiceFactory::DownloadServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "download::DownloadService",
          BrowserContextDependencyManager::GetInstance()) {}

DownloadServiceFactory::~DownloadServiceFactory() = default;

KeyedService* DownloadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  auto clients = base::MakeUnique<download::DownloadClientMap>();

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  clients->insert(std::make_pair(
      download::DownloadClient::OFFLINE_PAGE_PREFETCH,
      base::MakeUnique<offline_pages::OfflinePrefetchDownloadClient>(context)));
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  auto* download_manager = content::BrowserContext::GetDownloadManager(context);

  base::FilePath storage_dir;
  if (!context->IsOffTheRecord() && !context->GetPath().empty()) {
    storage_dir =
        context->GetPath().Append(chrome::kDownloadServiceStorageDirname);
  }

  scoped_refptr<base::SequencedTaskRunner> background_task_runner =
      base::CreateSequencedTaskRunnerWithTraits(
          {base::MayBlock(), base::TaskPriority::BACKGROUND});

  std::unique_ptr<download::TaskScheduler> task_scheduler;

#if defined(OS_ANDROID)
  task_scheduler = base::MakeUnique<download::android::DownloadTaskScheduler>();
#else
  task_scheduler = base::MakeUnique<DownloadTaskSchedulerImpl>(context);
#endif

  return download::CreateDownloadService(std::move(clients), download_manager,
                                         storage_dir, background_task_runner,
                                         std::move(task_scheduler));
}

content::BrowserContext* DownloadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
