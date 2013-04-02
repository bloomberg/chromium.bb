// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/browser/browser_context.h"

#if !defined(OS_IOS)
#include "base/path_service.h"
#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/download/download_manager_impl.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "content/browser/loader/resource_dispatcher_host_impl.h"
#include "content/browser/storage_partition_impl.h"
#include "content/browser/storage_partition_impl_map.h"
#include "content/common/child_process_host_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/site_instance.h"
#include "net/cookies/cookie_monster.h"
#include "net/cookies/cookie_store.h"
#include "net/ssl/server_bound_cert_service.h"
#include "net/ssl/server_bound_cert_store.h"
#include "net/url_request/url_request_context.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/clipboard/clipboard.h"
#include "webkit/database/database_tracker.h"
#include "webkit/fileapi/external_mount_points.h"
#endif // !OS_IOS

using base::UserDataAdapter;

namespace content {

// Only ~BrowserContext() is needed on iOS.
#if !defined(OS_IOS)
namespace {

// Key names on BrowserContext.
const char kClipboardDestroyerKey[] = "clipboard_destroyer";
const char kDownloadManagerKeyName[] = "download_manager";
const char kMountPointsKey[] = "mount_points";
const char kStorageParitionMapKeyName[] = "content_storage_partition_map";

StoragePartitionImplMap* GetStoragePartitionMap(
    BrowserContext* browser_context) {
  StoragePartitionImplMap* partition_map =
      static_cast<StoragePartitionImplMap*>(
          browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map) {
    partition_map = new StoragePartitionImplMap(browser_context);
    browser_context->SetUserData(kStorageParitionMapKeyName, partition_map);
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

// Run |callback| on each DOMStorageContextImpl in |browser_context|.
void PurgeDOMStorageContextInPartition(StoragePartition* storage_partition) {
  static_cast<StoragePartitionImpl*>(storage_partition)->
      GetDOMStorageContext()->PurgeMemory();
}

void SaveSessionStateOnIOThread(
    const scoped_refptr<net::URLRequestContextGetter>& context_getter,
    appcache::AppCacheService* appcache_service) {
  net::URLRequestContext* context = context_getter->GetURLRequestContext();
  context->cookie_store()->GetCookieMonster()->
      SetForceKeepSessionState();
  context->server_bound_cert_service()->GetCertStore()->
      SetForceKeepSessionState();
  appcache_service->set_force_keep_session_state();
}

void SaveSessionStateOnWebkitThread(
    scoped_refptr<IndexedDBContextImpl> indexed_db_context) {
  indexed_db_context->SetForceKeepSessionState();
}

void PurgeMemoryOnIOThread(appcache::AppCacheService* appcache_service) {
  appcache_service->PurgeMemory();
}

// OffTheRecordClipboardDestroyer is supposed to clear the clipboard in
// destructor if current clipboard content came from corresponding OffTheRecord
// browser context.
class OffTheRecordClipboardDestroyer : public base::SupportsUserData::Data {
 public:
  virtual ~OffTheRecordClipboardDestroyer() {
    ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
    ExamineClipboard(clipboard, ui::Clipboard::BUFFER_STANDARD);
    if (ui::Clipboard::IsValidBuffer(ui::Clipboard::BUFFER_SELECTION))
      ExamineClipboard(clipboard, ui::Clipboard::BUFFER_SELECTION);
  }

  ui::Clipboard::SourceTag GetAsSourceTag() {
    return ui::Clipboard::SourceTag(this);
  }

 private:
  void ExamineClipboard(ui::Clipboard* clipboard,
                        ui::Clipboard::Buffer buffer) {
    ui::Clipboard::SourceTag source_tag = clipboard->ReadSourceTag(buffer);
    if (source_tag == ui::Clipboard::SourceTag(this))
      clipboard->Clear(buffer);
  }
};

// Returns existing OffTheRecordClipboardDestroyer or creates one.
OffTheRecordClipboardDestroyer* GetClipboardDestroyerForBrowserContext(
    BrowserContext* context) {
  if (base::SupportsUserData::Data* data = context->GetUserData(
          kClipboardDestroyerKey))
    return static_cast<OffTheRecordClipboardDestroyer*>(data);
  OffTheRecordClipboardDestroyer* data = new OffTheRecordClipboardDestroyer;
  context->SetUserData(kClipboardDestroyerKey, data);
  return data;
}

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
      scoped_ptr<base::hash_set<base::FilePath> > active_paths,
      const base::Closure& done) {
  GetStoragePartitionMap(browser_context)->GarbageCollect(
      active_paths.Pass(), done);
}

DownloadManager* BrowserContext::GetDownloadManager(
    BrowserContext* context) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!context->GetUserData(kDownloadManagerKeyName)) {
    ResourceDispatcherHostImpl* rdh = ResourceDispatcherHostImpl::Get();
    DCHECK(rdh);
    scoped_refptr<DownloadManager> download_manager =
        new DownloadManagerImpl(
            GetContentClient()->browser()->GetNetLog());

    context->SetUserData(
        kDownloadManagerKeyName,
        new UserDataAdapter<DownloadManager>(download_manager));
    download_manager->SetDelegate(context->GetDownloadManagerDelegate());
    download_manager->Init(context);
  }

  return UserDataAdapter<DownloadManager>::Get(
      context, kDownloadManagerKeyName);
}

// static
fileapi::ExternalMountPoints* BrowserContext::GetMountPoints(
    BrowserContext* context) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

#if defined(OS_CHROMEOS)
  if (!context->GetUserData(kMountPointsKey)) {
    scoped_refptr<fileapi::ExternalMountPoints> mount_points =
        fileapi::ExternalMountPoints::CreateRefCounted();
    context->SetUserData(
        kMountPointsKey,
        new UserDataAdapter<fileapi::ExternalMountPoints>(
            mount_points));

    // Add Downloads mount point.
    base::FilePath home_path;
    if (PathService::Get(base::DIR_HOME, &home_path)) {
      mount_points->RegisterFileSystem(
          "Downloads",
          fileapi::kFileSystemTypeNativeLocal,
          home_path.AppendASCII("Downloads"));
    }
  }

  return UserDataAdapter<fileapi::ExternalMountPoints>::Get(
      context, kMountPointsKey);
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
          browser_context->GetUserData(kStorageParitionMapKeyName));
  if (!partition_map)
    return;

  partition_map->ForEach(callback);
}

StoragePartition* BrowserContext::GetDefaultStoragePartition(
    BrowserContext* browser_context) {
  return GetStoragePartition(browser_context, NULL);
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
            make_scoped_refptr(browser_context->GetRequestContext()),
            storage_partition->GetAppCacheService()));
  }

  DOMStorageContextImpl* dom_storage_context_impl =
      static_cast<DOMStorageContextImpl*>(
          storage_partition->GetDOMStorageContext());
  dom_storage_context_impl->SetForceKeepSessionState();

  if (BrowserThread::IsMessageLoopValid(BrowserThread::WEBKIT_DEPRECATED)) {
    IndexedDBContextImpl* indexed_db = static_cast<IndexedDBContextImpl*>(
        storage_partition->GetIndexedDBContext());
    BrowserThread::PostTask(
        BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
        base::Bind(&SaveSessionStateOnWebkitThread,
                   make_scoped_refptr(indexed_db)));
  }
}

void BrowserContext::PurgeMemory(BrowserContext* browser_context) {
  if (BrowserThread::IsMessageLoopValid(BrowserThread::IO)) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::Bind(
            &PurgeMemoryOnIOThread,
            BrowserContext::GetDefaultStoragePartition(browser_context)->
                GetAppCacheService()));
  }

  ForEachStoragePartition(browser_context,
                          base::Bind(&PurgeDOMStorageContextInPartition));
}

ui::Clipboard::SourceTag BrowserContext::GetMarkerForOffTheRecordContext(
    BrowserContext* context) {
  if (context && context->IsOffTheRecord()) {
    OffTheRecordClipboardDestroyer* clipboard_destroyer =
        GetClipboardDestroyerForBrowserContext(context);

    return clipboard_destroyer->GetAsSourceTag();
  }
  return ui::Clipboard::SourceTag();
}
#endif  // !OS_IOS

BrowserContext::~BrowserContext() {
#if !defined(OS_IOS)
  if (GetUserData(kDownloadManagerKeyName))
    GetDownloadManager(this)->Shutdown();
#endif
}

}  // namespace content
