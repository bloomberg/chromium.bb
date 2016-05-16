// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#include <stddef.h>
#include <stdint.h>
#include <algorithm>
#include <limits>
#include <memory>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/guid.h"
#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/rand_util.h"
#include "build/build_config.h"
#include "content/browser/blob_storage/chrome_blob_storage_context.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/mojo/browser_shell_connection.h"
#include "content/browser/mojo/constants.h"
#include "content/browser/push_messaging/push_messaging_router.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/blob_handle.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_instance.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/mojo_shell_connection.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/channel_id_service.h"
#include "net/ssl/channel_id_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "services/shell/public/cpp/connection.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/interfaces/shell_client.mojom.h"
#include "services/user/public/cpp/constants.h"
#include "services/user/user_id_map.h"
#include "services/user/user_shell_client.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/browser/fileapi/external_mount_points.h"

using base::UserDataAdapter;

namespace content {

namespace {

base::LazyInstance<std::set<std::string>> g_used_user_ids =
    LAZY_INSTANCE_INITIALIZER;
base::LazyInstance<std::vector<std::pair<BrowserContext*, std::string>>>
g_context_to_user_id = LAZY_INSTANCE_INITIALIZER;

// Key names on BrowserContext.
const char kDownloadManagerKeyName[] = "download_manager";
const char kMojoShellConnection[] = "mojo-shell-connection";
const char kMojoWasInitialized[] = "mojo-was-initialized";
const char kStoragePartitionMapKeyName[] = "content_storage_partition_map";

#if defined(OS_CHROMEOS)
const char kMountPointsKey[] = "mount_points";
#endif  // defined(OS_CHROMEOS)

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionImplMap(browser_context);
    browser_context->SetUserData(kStoragePartitionMapKeyName, partition_map);
  }
  return partition_map;
}

StoragePartition* GetStoragePartitionFromConfig(
    BrowserContext* browser_context,
    const std::string& partition_domain,
    const std::string& partition_name,
    bool in_memory) {
  StoragePartitionImplMap* partition_map =
      GetStoragePartitionMap(browser_context);

  if (browser_context->IsOffTheRecord())
    in_memory = true;

  return partition_map->Get(partition_domain, partition_name, in_memory);
}

void SaveSessionStateOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    AppCacheServiceImpl* appcache_service) {
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  context->cookie_store()->SetForceKeepSessionState();
  context->channel_id_service()->GetChannelIDStore()->
      SetForceKeepSessionState();
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnIndexedDBThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void ShutdownServiceWorkerContext(StoragePartition* partition) {
  ServiceWorkerContextWrapper* wrapper =
      static_cast<ServiceWorkerContextWrapper*>(
          partition->GetServiceWorkerContext());
  wrapper->process_manager()->Shutdown();
}

void SetDownloadManager(BrowserContext* context,
                        content::DownloadManager* download_manager) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  DCHECK(download_manager);
  context->SetUserData(kDownloadManagerKeyName, download_manager);
}

class BrowserContextShellConnectionHolder
    : public base::SupportsUserData::Data {
 public:
  BrowserContextShellConnectionHolder(
      std::unique_ptr<shell::Connection> connection,
      shell::mojom::ShellClientRequest request)
      : root_connection_(std::move(connection)),
        shell_connection_(new BrowserShellConnection(std::move(request))) {}
  ~BrowserContextShellConnectionHolder() override {}

  BrowserShellConnection* shell_connection() { return shell_connection_.get(); }

 private:
  std::unique_ptr<shell::Connection> root_connection_;
  std::unique_ptr<BrowserShellConnection> shell_connection_;

  DISALLOW_COPY_AND_ASSIGN(BrowserContextShellConnectionHolder);
};

}  // namespace

// static
void BrowserContext::AsyncObliterateStoragePartition(
    BrowserContext* browser_context,
    const GURL& site,
    const base::Closure& on_gc_required) {
  GetStoragePartitionMap(browser_context)->AsyncObliterate(site,
                                                           on_gc_required);
}

// static
void BrowserContext::GarbageCollectStoragePartitions(
    BrowserContext* browser_context,
    std::unique_ptr<base::hash_set<base::FilePath>> active_paths,
    const base::Closure& done) {
  GetStoragePartitionMap(browser_context)
      ->GarbageCollect(std::move(active_paths), done);
}

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    DownloadManager* download_manager =
        new DownloadManagerImpl(
            GetContentClient()->browser()->GetNetLog(), context);

    SetDownloadManager(context, download_manager);
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
  }

  return static_cast<DownloadManager*>(
      context->GetUserData(kDownloadManagerKeyName));
}

// static
storage::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* context) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

#if defined(OS_CHROMEOS)
  if (!context->GetUserData(kMountPointsKey)) {
    scoped_refptr<storage::ExternalMountPoints> mount_points =
        storage::ExternalMountPoints::CreateRefCounted();
    context->SetUserData(
        kMountPointsKey,
        new UserDataAdapter<storage::ExternalMountPoints>(mount_points.get()));
  }

  return UserDataAdapter<storage::ExternalMountPoints>::Get(context,
                                                            kMountPointsKey);
#else
  return NULL;
#endif
}

StoragePartition* BrowserContext::GetStoragePartition(
    BrowserContext* browser_context,
    SiteInstance* site_instance) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory = false;

  // TODO(ajwong): After GetDefaultStoragePartition() is removed, get rid of
  // this conditional and require that |site_instance| is non-NULL.
  if (site_instance) {
    GetContentClient()->browser()->GetStoragePartitionConfigForSite(
        browser_context, site_instance->GetSiteURL(), true,
        &partition_domain, &partition_name, &in_memory);
  }

  return GetStoragePartitionFromConfig(
      browser_context, partition_domain, partition_name, in_memory);
}

StoragePartition* BrowserContext::GetStoragePartitionForSite(
    BrowserContext* browser_context,
    const GURL& site) {
  std::string partition_domain;
  std::string partition_name;
  bool in_memory;

  GetContentClient()->browser()->GetStoragePartitionConfigForSite(
      browser_context, site, true, &partition_domain, &partition_name,
      &in_memory);

  return GetStoragePartitionFromConfig(
      browser_context, partition_domain, partition_name, in_memory);
}

void BrowserContext::ForEachStoragePartition(
    BrowserContext* browser_context,
    const StoragePartitionCallback& callback) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStoragePartitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(callback);
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, NULL);
}

// static
void BrowserContext::CreateMemoryBackedBlob(BrowserContext* browser_context,
                                            const char* data, size_t length,
                                            const BlobCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::CreateMemoryBackedBlob,
                 make_scoped_refptr(blob_context), data, length),
      callback);
}

// static
void BrowserContext::CreateFileBackedBlob(
    BrowserContext* browser_context,
    const base::FilePath& path,
    int64_t offset,
    int64_t size,
    const base::Time& expected_modification_time,
    const BlobCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  ChromeBlobStorageContext* blob_context =
      ChromeBlobStorageContext::GetFor(browser_context);
  BrowserThread::PostTaskAndReplyWithResult(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ChromeBlobStorageContext::CreateFileBackedBlob,
                 make_scoped_refptr(blob_context), path, offset, size,
                 expected_modification_time),
      callback);
}

// static
void BrowserContext::DeliverPushMessage(
    BrowserContext* browser_context,
    const GURL& origin,
    int64_t service_worker_registration_id,
    const PushEventPayload& payload,
    const base::Callback<void(PushDeliveryStatus)>& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  PushMessagingRouter::DeliverMessage(browser_context, origin,
                                      service_worker_registration_id, payload,
                                      callback);
}

// static
void BrowserContext::NotifyWillBeDestroyed(BrowserContext* browser_context) {
  // Service Workers must shutdown before the browser context is destroyed,
  // since they keep render process hosts alive and the codebase assumes that
  // render process hosts die before their profile (browser context) dies.
  ForEachStoragePartition(browser_context,
                          base::Bind(ShutdownServiceWorkerContext));
}

void BrowserContext::EnsureResourceContextInitialized(BrowserContext* context) {
  // This will be enough to tickle initialization of BrowserContext if
  // necessary, which initializes ResourceContext. The reason we don't call
  // ResourceContext::InitializeResourceContext() directly here is that
  // ResourceContext initialization may call back into BrowserContext
  // and when that call returns it'll end rewriting its UserData map. It will
  // end up rewriting the same value but this still causes a race condition.
  //
  // See http://crbug.com/115678.
  GetDefaultStoragePartition(context);
}

void BrowserContext::SaveSessionState(BrowserContext* browser_context) {
  GetDefaultStoragePartition(browser_context)->GetDatabaseTracker()->
      SetForceKeepSessionState();
  StoragePartition* storage_partition =
      BrowserContext::GetDefaultStoragePartition(browser_context);

  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &SaveSessionStateOnIOThread,
            make_scoped_refptr(BrowserContext::GetDefaultStoragePartition(
                browser_context)->GetURLRequestContext()),
            static_cast<AppCacheServiceImpl*>(
                storage_partition->GetAppCacheService())));
  }

  DOMStorageContextWrapper* dom_storage_context_proxy =
      static_cast<DOMStorageContextWrapper*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_proxy->SetForceKeepSessionState();

  IndexedDBContextImpl* indexed_db_context_impl =
      static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
  // No task runner in unit tests.
  if (indexed_db_context_impl->TaskRunner()) {
    indexed_db_context_impl->TaskRunner()->PostTask(
        FROM_HERE,
        base::Bind(&SaveSessionStateOnIndexedDBThread,
                   make_scoped_refptr(indexed_db_context_impl)));
  }
}

void BrowserContext::SetDownloadManagerForTesting(
    BrowserContext* browser_context,
    DownloadManager* download_manager) {
  SetDownloadManager(browser_context, download_manager);
}

// static
void BrowserContext::Initialize(
    BrowserContext* browser_context,
    const base::FilePath& path) {
  // Generate a GUID for |browser_context| to use as the Mojo user id.
  std::string new_id = base::GenerateGUID();
  while (g_used_user_ids.Get().find(new_id) != g_used_user_ids.Get().end())
    new_id = base::GenerateGUID();

  g_used_user_ids.Get().insert(new_id);
  g_context_to_user_id.Get().push_back(std::make_pair(browser_context, new_id));

  user_service::AssociateMojoUserIDWithUserDir(new_id, path);
  browser_context->SetUserData(kMojoWasInitialized,
                               new base::SupportsUserData::Data);

  MojoShellConnection* shell = MojoShellConnection::Get();
  if (shell) {
    // NOTE: Many unit tests create a TestBrowserContext without initializing
    // Mojo or the global Mojo shell connection.

    shell::mojom::ShellClientPtr shell_client;
    shell::mojom::ShellClientRequest shell_client_request =
        mojo::GetProxy(&shell_client);

    shell::mojom::PIDReceiverPtr pid_receiver;
    shell::Connector::ConnectParams params(
        shell::Identity(kBrowserMojoApplicationName, new_id));
    params.set_client_process_connection(std::move(shell_client),
                                         mojo::GetProxy(&pid_receiver));
    pid_receiver->SetPID(base::GetCurrentProcId());

    BrowserContextShellConnectionHolder* connection_holder =
        new BrowserContextShellConnectionHolder(
          shell->GetConnector()->Connect(&params),
          std::move(shell_client_request));
    browser_context->SetUserData(kMojoShellConnection, connection_holder);

    BrowserShellConnection* connection = connection_holder->shell_connection();

    // New embedded application factories should be added to |connection| here.

    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kMojoLocalStorage)) {
      MojoApplicationInfo info;
      info.application_factory = base::Bind(
          &user_service::CreateUserShellClient,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB));
      connection->AddEmbeddedApplication(user_service::kUserServiceName, info);
    }
  }
}

// static
const std::string& BrowserContext::GetMojoUserIdFor(
    BrowserContext* browser_context) {
  CHECK(browser_context->GetUserData(kMojoWasInitialized))
      << "Attempting to get the mojo user id for a BrowserContext that was "
      << "never Initialize()ed.";

  auto it = std::find_if(
      g_context_to_user_id.Get().begin(),
      g_context_to_user_id.Get().end(),
      [&browser_context](const std::pair<BrowserContext*, std::string>& p) {
        return p.first == browser_context; });
  CHECK(it != g_context_to_user_id.Get().end());
  return it->second;
}

// static
shell::Connector* BrowserContext::GetMojoConnectorFor(
    BrowserContext* browser_context) {
  BrowserContextShellConnectionHolder* connection_holder =
      static_cast<BrowserContextShellConnectionHolder*>(
          browser_context->GetUserData(kMojoShellConnection));
  if (!connection_holder)
    return nullptr;
  return connection_holder->shell_connection()->GetConnector();
}

BrowserContext::~BrowserContext() {
  CHECK(GetUserData(kMojoWasInitialized))
      << "Attempting to destroy a BrowserContext that never called "
      << "Initialize()";

  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
}

}  // namespace content
