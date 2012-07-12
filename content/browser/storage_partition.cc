// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition.h"

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/dom_storage/dom_storage_context_impl.h"
#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/browser/in_process_webkit/indexed_db_context_impl.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

namespace content {

StoragePartition::StoragePartition(
    const FilePath& partition_path,
    quota::QuotaManager* quota_manager,
    ChromeAppCacheService* appcache_service,
    fileapi::FileSystemContext* filesystem_context,
    webkit_database::DatabaseTracker* database_tracker,
    DOMStorageContextImpl* dom_storage_context,
    IndexedDBContext* indexed_db_context)
    : partition_path_(partition_path),
      quota_manager_(quota_manager),
      appcache_service_(appcache_service),
      filesystem_context_(filesystem_context),
      database_tracker_(database_tracker),
      dom_storage_context_(dom_storage_context),
      indexed_db_context_(indexed_db_context) {
}

StoragePartition::~StoragePartition() {
  // These message loop checks are just to avoid leaks in unittests.
  if (database_tracker() &&
      BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&webkit_database::DatabaseTracker::Shutdown,
                   database_tracker()));
  }

  if (dom_storage_context())
    dom_storage_context()->Shutdown();
}

// TODO(ajwong): Break the direct dependency on |context|. We only
// need 3 pieces of info from it.
StoragePartition* StoragePartition::Create(BrowserContext* context,
                                           const FilePath& partition_path) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

  // All of the clients have to be created and registered with the
  // QuotaManager prior to the QuotaManger being used. We do them
  // all together here prior to handing out a reference to anything
  // that utilizes the QuotaManager.
  scoped_refptr<quota::QuotaManager> quota_manager =
      new quota::QuotaManager(
          context->IsOffTheRecord(), partition_path,
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::IO),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::DB),
          context->GetSpecialStoragePolicy());

  // Each consumer is responsible for registering its QuotaClient during
  // its construction.
  scoped_refptr<fileapi::FileSystemContext> filesystem_context =
      CreateFileSystemContext(partition_path, context->IsOffTheRecord(),
                              context->GetSpecialStoragePolicy(),
                              quota_manager->proxy());

  scoped_refptr<webkit_database::DatabaseTracker> database_tracker =
      new webkit_database::DatabaseTracker(
          partition_path, context->IsOffTheRecord(),
          context->GetSpecialStoragePolicy(), quota_manager->proxy(),
          BrowserThread::GetMessageLoopProxyForThread(BrowserThread::FILE));

  FilePath path = context->IsOffTheRecord() ? FilePath() : partition_path;
  scoped_refptr<DOMStorageContextImpl> dom_storage_context =
      new DOMStorageContextImpl(path, context->GetSpecialStoragePolicy());

  scoped_refptr<IndexedDBContextImpl> indexed_db_context =
      new IndexedDBContextImpl(path, context->GetSpecialStoragePolicy(),
                               quota_manager->proxy(),
                               BrowserThread::GetMessageLoopProxyForThread(
                                   BrowserThread::WEBKIT_DEPRECATED));

  scoped_refptr<ChromeAppCacheService> appcache_service =
      new ChromeAppCacheService(quota_manager->proxy());

  return new StoragePartition(partition_path,
                              quota_manager,
                              appcache_service,
                              filesystem_context,
                              database_tracker,
                              dom_storage_context,
                              indexed_db_context);
}

}  // namespace content
