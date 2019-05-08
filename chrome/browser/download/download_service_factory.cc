// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/download_service_factory.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/memory/singleton.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "chrome/browser/background_fetch/background_fetch_download_client.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"
#include "chrome/browser/download/deferred_client_wrapper.h"
#include "chrome/browser/download/download_manager_utils.h"
#include "chrome/browser/download/download_task_scheduler_impl.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/transition_manager/full_browser_transition_manager.h"
#include "chrome/common/chrome_constants.h"
#include "components/download/content/factory/download_service_factory_helper.h"
#include "components/download/content/factory/navigation_monitor_factory.h"
#include "components/download/public/background_service/blob_context_getter_factory.h"
#include "components/download/public/background_service/clients.h"
#include "components/download/public/background_service/download_service.h"
#include "components/download/public/background_service/features.h"
#include "components/download/public/common/simple_download_manager_coordinator.h"
#include "components/download/public/task/task_scheduler.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/leveldb_proto/content/proto_database_provider_factory.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/network_service_instance.h"
#include "content/public/browser/storage_partition.h"

#if defined(OS_ANDROID)
#include "chrome/browser/android/download/service/download_task_scheduler.h"
#endif

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/prefetch/offline_prefetch_download_client.h"
#endif

namespace {

std::unique_ptr<download::Client> CreateBackgroundFetchDownloadClient(
    Profile* profile) {
  return std::make_unique<BackgroundFetchDownloadClient>(profile);
}

#if defined(CHROMEOS)
std::unique_ptr<download::Client> CreatePluginVmImageDownloadClient(
    Profile* profile) {
  return std::make_unique<plugin_vm::PluginVmImageDownloadClient>(profile);
}
#endif

// Called on profile created to retrieve the BlobStorageContextGetter.
void OnProfileCreated(download::BlobContextGetterCallback callback,
                      Profile* profile) {
  auto blob_context_getter =
      content::BrowserContext::GetBlobStorageContext(profile);
  DCHECK(callback);
  std::move(callback).Run(blob_context_getter);
}

// Provides BlobContextGetter from Chrome asynchronously.
class DownloadBlobContextGetterFactory
    : public download::BlobContextGetterFactory {
 public:
  explicit DownloadBlobContextGetterFactory(ProfileKey* profile_key)
      : profile_key_(profile_key) {
    DCHECK(profile_key_);
  }
  ~DownloadBlobContextGetterFactory() override = default;

 private:
  // download::BlobContextGetterFactory implementation.
  void RetrieveBlobContextGetter(
      download::BlobContextGetterCallback callback) override {
    FullBrowserTransitionManager::Get()->RegisterCallbackOnProfileCreation(
        profile_key_, base::BindOnce(&OnProfileCreated, std::move(callback)));
  }

  ProfileKey* profile_key_;
  DISALLOW_COPY_AND_ASSIGN(DownloadBlobContextGetterFactory);
};

}  // namespace

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
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(SimpleDownloadManagerCoordinatorFactory::GetInstance());
  DependsOn(download::NavigationMonitorFactory::GetInstance());
  DependsOn(leveldb_proto::ProtoDatabaseProviderFactory::GetInstance());
}

DownloadServiceFactory::~DownloadServiceFactory() = default;

KeyedService* DownloadServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  auto clients = std::make_unique<download::DownloadClientMap>();

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Offline prefetch doesn't support incognito.
  if (!context->IsOffTheRecord()) {
    clients->insert(std::make_pair(
        download::DownloadClient::OFFLINE_PAGE_PREFETCH,
        std::make_unique<offline_pages::OfflinePrefetchDownloadClient>(
            context)));
  }
#endif  // BUILDFLAG(ENABLE_OFFLINE_PAGES)

  clients->insert(
      std::make_pair(download::DownloadClient::BACKGROUND_FETCH,
                     std::make_unique<download::DeferredClientWrapper>(
                         base::BindOnce(&CreateBackgroundFetchDownloadClient),
                         profile->GetProfileKey())));

#if defined(CHROMEOS)
  if (!context->IsOffTheRecord()) {
    clients->insert(
        std::make_pair(download::DownloadClient::PLUGIN_VM_IMAGE,
                       std::make_unique<download::DeferredClientWrapper>(
                           base::BindOnce(&CreatePluginVmImageDownloadClient),
                           profile->GetProfileKey())));
  }
#endif

  // Build in memory download service for incognito profile.
  if (context->IsOffTheRecord() &&
      base::FeatureList::IsEnabled(download::kDownloadServiceIncognito)) {
    auto blob_context_getter_factory =
        std::make_unique<DownloadBlobContextGetterFactory>(
            profile->GetProfileKey());
    scoped_refptr<base::SingleThreadTaskRunner> io_task_runner =
        base::CreateSingleThreadTaskRunnerWithTraits(
            {content::BrowserThread::IO});
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
        SystemNetworkContextManager::GetInstance()->GetSharedURLLoaderFactory();

    return download::BuildInMemoryDownloadService(
        profile->GetProfileKey(), std::move(clients),
        content::GetNetworkConnectionTracker(), base::FilePath(),
        std::move(blob_context_getter_factory), io_task_runner,
        url_loader_factory);
  } else {
    // Build download service for normal profile.
    base::FilePath storage_dir;
    if (!context->IsOffTheRecord() && !context->GetPath().empty()) {
      storage_dir =
          context->GetPath().Append(chrome::kDownloadServiceStorageDirname);
    }
    scoped_refptr<base::SequencedTaskRunner> background_task_runner =
        base::CreateSequencedTaskRunnerWithTraits(
            {base::MayBlock(), base::TaskPriority::BEST_EFFORT});

    std::unique_ptr<download::TaskScheduler> task_scheduler;
#if defined(OS_ANDROID)
    task_scheduler =
        std::make_unique<download::android::DownloadTaskScheduler>();
#else
    task_scheduler = std::make_unique<DownloadTaskSchedulerImpl>(context);
#endif
    // Some tests doesn't initialize DownloadManager when profile is created,
    // and cause the download service to fail. Call
    // InitializeSimpleDownloadManager() to initialize the DownloadManager
    // whenever profile becomes available.
    DownloadManagerUtils::InitializeSimpleDownloadManager(
        profile->GetProfileKey());
    return download::BuildDownloadService(
        profile->GetProfileKey(), profile->GetPrefs(), std::move(clients),
        content::GetNetworkConnectionTracker(), storage_dir,
        SimpleDownloadManagerCoordinatorFactory::GetForKey(
            profile->GetProfileKey()),
        background_task_runner, std::move(task_scheduler));
  }
}

content::BrowserContext* DownloadServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
