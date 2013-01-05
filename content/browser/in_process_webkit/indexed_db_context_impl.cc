// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_context_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "content/browser/in_process_webkit/indexed_db_quota_client.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/content_switches.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "webkit/base/file_path_string_conversions.h"
#include "webkit/database/database_util.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

using webkit_database::DatabaseUtil;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBFactory;
using WebKit::WebSecurityOrigin;

namespace content {
const FilePath::CharType IndexedDBContextImpl::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

const FilePath::CharType IndexedDBContextImpl::kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");

namespace {

void GetAllOriginsAndPaths(
    const FilePath& indexeddb_path,
    std::vector<GURL>* origins,
    std::vector<FilePath>* file_paths) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  if (indexeddb_path.empty())
    return;
  file_util::FileEnumerator file_enumerator(indexeddb_path,
      false, file_util::FileEnumerator::DIRECTORIES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == IndexedDBContextImpl::kIndexedDBExtension) {
      WebKit::WebString origin_id_webstring =
          webkit_base::FilePathToWebString(file_path.BaseName());
      origins->push_back(
          DatabaseUtil::GetOriginFromIdentifier(origin_id_webstring));
      if (file_paths)
        file_paths->push_back(file_path);
    }
  }
}

// Deletes session-only databases.
void ClearSessionOnlyOrigins(
    const FilePath& indexeddb_path,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  std::vector<GURL> origins;
  std::vector<FilePath> file_paths;
  GetAllOriginsAndPaths(indexeddb_path, &origins, &file_paths);
  DCHECK_EQ(origins.size(), file_paths.size());
  std::vector<FilePath>::const_iterator file_path_iter = file_paths.begin();
  for (std::vector<GURL>::const_iterator iter = origins.begin();
       iter != origins.end(); ++iter, ++file_path_iter) {
    if (!special_storage_policy->IsStorageSessionOnly(*iter))
      continue;
    if (special_storage_policy->IsStorageProtected(*iter))
      continue;
    file_util::Delete(*file_path_iter, true);
  }
}

}  // namespace

IndexedDBContextImpl::IndexedDBContextImpl(
    const FilePath& data_path,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::MessageLoopProxy* webkit_thread_loop)
    : force_keep_session_state_(false),
      special_storage_policy_(special_storage_policy),
      quota_manager_proxy_(quota_manager_proxy) {
  if (!data_path.empty())
    data_path_ = data_path.Append(kIndexedDBDirectory);
  if (quota_manager_proxy &&
      !CommandLine::ForCurrentProcess()->HasSwitch(switches::kSingleProcess)) {
    quota_manager_proxy->RegisterClient(
        new IndexedDBQuotaClient(webkit_thread_loop, this));
  }
}

WebIDBFactory* IndexedDBContextImpl::GetIDBFactory() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  if (!idb_factory_.get()) {
    // Prime our cache of origins with existing databases so we can
    // detect when dbs are newly created.
    GetOriginSet();
    idb_factory_.reset(WebIDBFactory::create());
  }
  return idb_factory_.get();
}

std::vector<GURL> IndexedDBContextImpl::GetAllOrigins() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  std::vector<GURL> origins;
  std::set<GURL>* origins_set = GetOriginSet();
  for (std::set<GURL>::const_iterator iter = origins_set->begin();
       iter != origins_set->end(); ++iter) {
    origins.push_back(*iter);
  }
  return origins;
}

int64 IndexedDBContextImpl::GetOriginDiskUsage(const GURL& origin_url) {
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return 0;
  EnsureDiskUsageCacheInitialized(origin_url);
  return origin_size_map_[origin_url];
}

base::Time IndexedDBContextImpl::GetOriginLastModified(const GURL& origin_url) {
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return base::Time();
  string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url);
  FilePath idb_directory = GetIndexedDBFilePath(origin_id);
  base::PlatformFileInfo file_info;
  if (!file_util::GetFileInfo(idb_directory, &file_info))
    return base::Time();
  return file_info.last_modified;
}

void IndexedDBContextImpl::DeleteForOrigin(const GURL& origin_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  if (data_path_.empty() || !IsInOriginSet(origin_url))
    return;

  if (connections_.find(origin_url) != connections_.end()) {
    ConnectionSet& connections = connections_[origin_url];
    ConnectionSet::iterator it = connections.begin();
    while (it != connections.end()) {
      // Remove before closing so callbacks don't double-erase
      WebKit::WebIDBDatabase* db = *it;
      connections.erase(it++);
      db->forceClose();
    }
    DCHECK(connections_[origin_url].size() == 0);
    connections_.erase(origin_url);
  }

  string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url);
  FilePath idb_directory = GetIndexedDBFilePath(origin_id);
  EnsureDiskUsageCacheInitialized(origin_url);
  const bool recursive = true;
  bool deleted = file_util::Delete(idb_directory, recursive);

  QueryDiskAndUpdateQuotaUsage(origin_url);
  if (deleted) {
    RemoveFromOriginSet(origin_url);
    origin_size_map_.erase(origin_url);
    space_available_map_.erase(origin_url);
  }
}

FilePath IndexedDBContextImpl::GetFilePathForTesting(
  const string16& origin_id) const {
  return GetIndexedDBFilePath(origin_id);
}

void IndexedDBContextImpl::ConnectionOpened(const GURL& origin_url,
                                            WebIDBDatabase* connection) {
  DCHECK(connections_[origin_url].count(connection) == 0);
  if (quota_manager_proxy()) {
    quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kIndexedDatabase, origin_url,
        quota::kStorageTypeTemporary);
  }
  connections_[origin_url].insert(connection);
  if (AddToOriginSet(origin_url)) {
    // A newly created db, notify the quota system.
    QueryDiskAndUpdateQuotaUsage(origin_url);
  } else {
    EnsureDiskUsageCacheInitialized(origin_url);
  }
  QueryAvailableQuota(origin_url);
}

void IndexedDBContextImpl::ConnectionClosed(const GURL& origin_url,
                                            WebIDBDatabase* connection) {
  // May not be in the map if connection was forced to close
  if (connections_.find(origin_url) == connections_.end() ||
      connections_[origin_url].count(connection) != 1)
    return;
  if (quota_manager_proxy()) {
    quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kIndexedDatabase, origin_url,
        quota::kStorageTypeTemporary);
  }
  connections_[origin_url].erase(connection);
  if (connections_[origin_url].size() == 0) {
    QueryDiskAndUpdateQuotaUsage(origin_url);
    connections_.erase(origin_url);
  }
}

void IndexedDBContextImpl::TransactionComplete(const GURL& origin_url) {
  DCHECK(connections_.find(origin_url) != connections_.end() &&
         connections_[origin_url].size() > 0);
  QueryDiskAndUpdateQuotaUsage(origin_url);
  QueryAvailableQuota(origin_url);
}

bool IndexedDBContextImpl::WouldBeOverQuota(const GURL& origin_url,
                                            int64 additional_bytes) {
  if (space_available_map_.find(origin_url) == space_available_map_.end()) {
    // We haven't heard back from the QuotaManager yet, just let it through.
    return false;
  }
  bool over_quota = additional_bytes > space_available_map_[origin_url];
  return over_quota;
}

bool IndexedDBContextImpl::IsOverQuota(const GURL& origin_url) {
  const int kOneAdditionalByte = 1;
  return WouldBeOverQuota(origin_url, kOneAdditionalByte);
}

quota::QuotaManagerProxy* IndexedDBContextImpl::quota_manager_proxy() {
  return quota_manager_proxy_;
}

IndexedDBContextImpl::~IndexedDBContextImpl() {
  WebKit::WebIDBFactory* factory = idb_factory_.release();
  if (factory) {
    if (!BrowserThread::DeleteSoon(BrowserThread::WEBKIT_DEPRECATED,
                                   FROM_HERE, factory))
      delete factory;
  }

  if (data_path_.empty())
    return;

  if (force_keep_session_state_)
    return;

  bool has_session_only_databases =
      special_storage_policy_.get() &&
      special_storage_policy_->HasSessionOnlyOrigins();

  // Clearning only session-only databases, and there are none.
  if (!has_session_only_databases)
    return;

  // No WEBKIT thread here means we are running in a unit test where no clean
  // up is needed.
  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(&ClearSessionOnlyOrigins,
                 data_path_,
                 special_storage_policy_));
}

FilePath IndexedDBContextImpl::GetIndexedDBFilePath(
    const string16& origin_id) const {
  DCHECK(!data_path_.empty());
  FilePath::StringType id =
      webkit_base::WebStringToFilePathString(origin_id).append(
          FILE_PATH_LITERAL(".indexeddb"));
  return data_path_.Append(id.append(kIndexedDBExtension));
}

int64 IndexedDBContextImpl::ReadUsageFromDisk(const GURL& origin_url) const {
  if (data_path_.empty())
    return 0;
  string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url);
  FilePath file_path = GetIndexedDBFilePath(origin_id);
  return file_util::ComputeDirectorySize(file_path);
}

void IndexedDBContextImpl::EnsureDiskUsageCacheInitialized(
    const GURL& origin_url) {
  if (origin_size_map_.find(origin_url) == origin_size_map_.end())
    origin_size_map_[origin_url] = ReadUsageFromDisk(origin_url);
}

void IndexedDBContextImpl::QueryDiskAndUpdateQuotaUsage(
    const GURL& origin_url) {
  int64 former_disk_usage = origin_size_map_[origin_url];
  int64 current_disk_usage = ReadUsageFromDisk(origin_url);
  int64 difference = current_disk_usage - former_disk_usage;
  if (difference) {
    origin_size_map_[origin_url] = current_disk_usage;
    // quota_manager_proxy() is NULL in unit tests.
    if (quota_manager_proxy())
      quota_manager_proxy()->NotifyStorageModified(
          quota::QuotaClient::kIndexedDatabase,
          origin_url,
          quota::kStorageTypeTemporary,
          difference);
  }
}

void IndexedDBContextImpl::GotUsageAndQuota(const GURL& origin_url,
                                            quota::QuotaStatusCode status,
                                            int64 usage, int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  DCHECK(status == quota::kQuotaStatusOk || status == quota::kQuotaErrorAbort)
      << "status was " << status;
  if (status == quota::kQuotaErrorAbort) {
    // We seem to no longer care to wait around for the answer.
    return;
  }
  BrowserThread::PostTask(
      BrowserThread::WEBKIT_DEPRECATED, FROM_HERE,
      base::Bind(&IndexedDBContextImpl::GotUpdatedQuota, this, origin_url,
                 usage, quota));
}

void IndexedDBContextImpl::GotUpdatedQuota(const GURL& origin_url, int64 usage,
                                           int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
  space_available_map_[origin_url] = quota - usage;
}

void IndexedDBContextImpl::QueryAvailableQuota(const GURL& origin_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT_DEPRECATED));
    if (quota_manager_proxy())
      BrowserThread::PostTask(
          BrowserThread::IO, FROM_HERE,
          base::Bind(&IndexedDBContextImpl::QueryAvailableQuota, this,
                     origin_url));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!quota_manager_proxy() || !quota_manager_proxy()->quota_manager())
    return;
  quota_manager_proxy()->quota_manager()->GetUsageAndQuota(
      origin_url,
      quota::kStorageTypeTemporary,
      base::Bind(&IndexedDBContextImpl::GotUsageAndQuota, this, origin_url));
}

std::set<GURL>* IndexedDBContextImpl::GetOriginSet() {
  if (!origin_set_.get()) {
    origin_set_.reset(new std::set<GURL>);
    std::vector<GURL> origins;
    GetAllOriginsAndPaths(data_path_, &origins, NULL);
    for (std::vector<GURL>::const_iterator iter = origins.begin();
         iter != origins.end(); ++iter) {
      origin_set_->insert(*iter);
    }
  }
  return origin_set_.get();
}

void IndexedDBContextImpl::ResetCaches() {
  origin_set_.reset();
  origin_size_map_.clear();
  space_available_map_.clear();
}

}  // namespace content
