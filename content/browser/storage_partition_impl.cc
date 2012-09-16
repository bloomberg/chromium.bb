// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/storage_partition_impl.h"

#include "content/browser/fileapi/browser_file_system_helper.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "webkit/database/database_tracker.h"
#include "webkit/quota/quota_manager.h"

namespace content {

namespace {

// These constants are used to create the directory structure under the profile
// where renderers with a non-default storage partition keep their persistent
// state. This will contain a set of directories that partially mirror the
// directory structure of BrowserContext::GetPath().
//
// The kStoragePartitionDirname is contains an extensions directory which is
// further partitioned by extension id, followed by another level of directories
// for the "default" extension storage partition and one directory for each
// persistent partition used by an extension's browser tags. Example:
//
//   {kStoragePartitionDirname}/extensions/ABCDEF/default
//   {kStoragePartitionDirname}/extensions/ABCDEF/{hash(guest partition)}
//
// The code in GetPartitionPath() constructs these path names.
const FilePath::CharType kStoragePartitionDirname[] =
    FILE_PATH_LITERAL("Storage Partitions");
const FilePath::CharType kExtensionsDirname[] =
    FILE_PATH_LITERAL("extensions");
const FilePath::CharType kDefaultPartitionDirname[] =
    FILE_PATH_LITERAL("default");

}  // namespace

// static
FilePath StoragePartition::GetPartitionPath(const std::string& partition_id) {
  if (partition_id.empty()) {
    // The default profile just sits inside the top-level profile directory.
    return FilePath();
  }

  // TODO(ajwong): This should check that we create a valid path name.
  CHECK(IsStringASCII(partition_id));
  return FilePath(kStoragePartitionDirname)
      .Append(kExtensionsDirname)
      .AppendASCII(partition_id)
      .Append(kDefaultPartitionDirname);
}

StoragePartitionImpl::StoragePartitionImpl(
    const FilePath& partition_path,
    quota::QuotaManager* quota_manager,
    ChromeAppCacheService* appcache_service,
    fileapi::FileSystemContext* filesystem_context,
    webkit_database::DatabaseTracker* database_tracker,
    DOMStorageContextImpl* dom_storage_context,
    IndexedDBContextImpl* indexed_db_context)
    : partition_path_(partition_path),
      quota_manager_(quota_manager),
      appcache_service_(appcache_service),
      filesystem_context_(filesystem_context),
      database_tracker_(database_tracker),
      dom_storage_context_(dom_storage_context),
      indexed_db_context_(indexed_db_context) {
}

StoragePartitionImpl::~StoragePartitionImpl() {
  // These message loop checks are just to avoid leaks in unittests.
  if (GetDatabaseTracker() &&
      BrowserThread::IsMessageLoopValid(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE,
        base::Bind(&webkit_database::DatabaseTracker::Shutdown,
                   GetDatabaseTracker()));
  }

  if (GetDOMStorageContext())
    GetDOMStorageContext()->Shutdown();
}

// TODO(ajwong): Break the direct dependency on |context|. We only
// need 3 pieces of info from it.
StoragePartitionImpl* StoragePartitionImpl::Create(
    BrowserContext* context,
    const std::string& partition_id,
    const FilePath& profile_path) {
  // Ensure that these methods are called on the UI thread, except for
  // unittests where a UI thread might not have been created.
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI) ||
         !BrowserThread::IsMessageLoopValid(BrowserThread::UI));

  FilePath partition_path =
      profile_path.Append(GetPartitionPath(partition_id));

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

  return new StoragePartitionImpl(partition_path,
                                  quota_manager,
                                  appcache_service,
                                  filesystem_context,
                                  database_tracker,
                                  dom_storage_context,
                                  indexed_db_context);
}

FilePath StoragePartitionImpl::GetPath() {
  return partition_path_;
}

quota::QuotaManager* StoragePartitionImpl::GetQuotaManager() {
  return quota_manager_;
}

ChromeAppCacheService* StoragePartitionImpl::GetAppCacheService() {
  return appcache_service_;
}

fileapi::FileSystemContext* StoragePartitionImpl::GetFileSystemContext() {
  return filesystem_context_;
}

webkit_database::DatabaseTracker* StoragePartitionImpl::GetDatabaseTracker() {
  return database_tracker_;
}

DOMStorageContextImpl* StoragePartitionImpl::GetDOMStorageContext() {
  return dom_storage_context_;
}

IndexedDBContextImpl* StoragePartitionImpl::GetIndexedDBContext() {
  return indexed_db_context_;
}

}  // namespace content
