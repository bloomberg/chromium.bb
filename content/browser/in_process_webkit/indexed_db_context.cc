// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/in_process_webkit/indexed_db_context.h"

#include "base/file_util.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "base/string_util.h"
#include "base/task.h"
#include "base/utf_string_conversions.h"
#include "content/browser/browser_thread.h"
#include "content/browser/in_process_webkit/indexed_db_quota_client.h"
#include "content/browser/in_process_webkit/webkit_context.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBDatabase.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebIDBFactory.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSecurityOrigin.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "webkit/database/database_util.h"
#include "webkit/glue/webkit_glue.h"
#include "webkit/quota/quota_manager.h"
#include "webkit/quota/special_storage_policy.h"

using webkit_database::DatabaseUtil;
using WebKit::WebIDBDatabase;
using WebKit::WebIDBFactory;
using WebKit::WebSecurityOrigin;

namespace {

void ClearLocalState(
    const FilePath& indexeddb_path,
    scoped_refptr<quota::SpecialStoragePolicy> special_storage_policy) {
  file_util::FileEnumerator file_enumerator(
      indexeddb_path, false, file_util::FileEnumerator::FILES);
  // TODO(pastarmovj): We might need to consider exchanging this loop for
  // something more efficient in the future.
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() != IndexedDBContext::kIndexedDBExtension)
      continue;
    WebSecurityOrigin origin =
        WebSecurityOrigin::createFromDatabaseIdentifier(
            webkit_glue::FilePathToWebString(file_path.BaseName()));
    if (special_storage_policy->IsStorageProtected(GURL(origin.toString())))
      continue;
    file_util::Delete(file_path, false);
  }
}

}  // namespace

const FilePath::CharType IndexedDBContext::kIndexedDBDirectory[] =
    FILE_PATH_LITERAL("IndexedDB");

const FilePath::CharType IndexedDBContext::kIndexedDBExtension[] =
    FILE_PATH_LITERAL(".leveldb");

class IndexedDBContext::IndexedDBGetUsageAndQuotaCallback :
    public quota::QuotaManager::GetUsageAndQuotaCallback {
 public:
  IndexedDBGetUsageAndQuotaCallback(IndexedDBContext* context,
                                    const GURL& origin_url)
      : context_(context),
        origin_url_(origin_url) {
  }

  void Run(quota::QuotaStatusCode status, int64 usage, int64 quota) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    DCHECK(status == quota::kQuotaStatusOk || status == quota::kQuotaErrorAbort)
        << "status was " << status;
    if (status == quota::kQuotaErrorAbort) {
      // We seem to no longer care to wait around for the answer.
      return;
    }
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableMethod(context_.get(),
                          &IndexedDBContext::GotUpdatedQuota,
                          origin_url_,
                          usage,
                          quota));
  }

  virtual void RunWithParams(
        const Tuple3<quota::QuotaStatusCode, int64, int64>& params) {
    Run(params.a, params.b, params.c);
  }

 private:
  scoped_refptr<IndexedDBContext> context_;
  const GURL origin_url_;
};

IndexedDBContext::IndexedDBContext(
    WebKitContext* webkit_context,
    quota::SpecialStoragePolicy* special_storage_policy,
    quota::QuotaManagerProxy* quota_manager_proxy,
    base::MessageLoopProxy* webkit_thread_loop)
    : clear_local_state_on_exit_(false),
      special_storage_policy_(special_storage_policy),
      quota_manager_proxy_(quota_manager_proxy) {
  data_path_ = webkit_context->data_path().Append(kIndexedDBDirectory);
  if (quota_manager_proxy) {
    quota_manager_proxy->RegisterClient(
        new IndexedDBQuotaClient(webkit_thread_loop, this));
  }
}

IndexedDBContext::~IndexedDBContext() {
  WebKit::WebIDBFactory* factory = idb_factory_.release();
  if (factory)
    BrowserThread::DeleteSoon(BrowserThread::WEBKIT, FROM_HERE, factory);

  if (clear_local_state_on_exit_) {
    // No WEBKIT thread here means we are running in a unit test where no clean
    // up is needed.
    BrowserThread::PostTask(BrowserThread::WEBKIT, FROM_HERE,
        NewRunnableFunction(&ClearLocalState, data_path_,
                            special_storage_policy_));
  }
}

WebIDBFactory* IndexedDBContext::GetIDBFactory() {
  if (!idb_factory_.get())
    idb_factory_.reset(WebIDBFactory::create());
  DCHECK(idb_factory_.get());
  return idb_factory_.get();
}

FilePath IndexedDBContext::GetIndexedDBFilePath(
    const string16& origin_id) const {
  FilePath::StringType id =
      webkit_glue::WebStringToFilePathString(origin_id).append(
          FILE_PATH_LITERAL(".indexeddb"));
  return data_path_.Append(id.append(kIndexedDBExtension));
}

// Note: This is not called in response to a UI action in Content Settings. Only
// extension data deleter and quota manager currently call this.
void IndexedDBContext::DeleteIndexedDBForOrigin(const GURL& origin_url) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url);
  FilePath idb_directory = GetIndexedDBFilePath(origin_id);
  if (idb_directory.BaseName().value().substr(0, strlen("chrome-extension")) ==
      FILE_PATH_LITERAL("chrome-extension") ||
      connection_count_.find(origin_url) == connection_count_.end()) {
    EnsureDiskUsageCacheInitialized(origin_url);
    file_util::Delete(idb_directory, true /*recursive*/);
    QueryDiskAndUpdateQuotaUsage(origin_url);
  }
}

bool IndexedDBContext::IsUnlimitedStorageGranted(
    const GURL& origin) const {
  return special_storage_policy_->IsStorageUnlimited(origin);
}

// TODO(dgrogan): Merge this code with the similar loop in
// BrowsingDataIndexedDBHelperImpl::FetchIndexedDBInfoInWebKitThread.
void IndexedDBContext::GetAllOriginIdentifiers(
    std::vector<string16>* origin_ids) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  file_util::FileEnumerator file_enumerator(data_path_,
      false, file_util::FileEnumerator::DIRECTORIES);
  for (FilePath file_path = file_enumerator.Next(); !file_path.empty();
       file_path = file_enumerator.Next()) {
    if (file_path.Extension() == IndexedDBContext::kIndexedDBExtension) {
      WebKit::WebString origin_id_webstring =
          webkit_glue::FilePathToWebString(file_path.BaseName());
      origin_ids->push_back(origin_id_webstring);
    }
  }
}

int64 IndexedDBContext::GetOriginDiskUsage(const GURL& origin_url) {
  return ResetDiskUsageCache(origin_url);
}

void IndexedDBContext::ConnectionOpened(const GURL& origin_url) {
  if (quota_manager_proxy()) {
    quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kIndexedDatabase, origin_url,
        quota::kStorageTypeTemporary);
  }
  connection_count_[origin_url]++;
  QueryAvailableQuota(origin_url);
  EnsureDiskUsageCacheInitialized(origin_url);
}

void IndexedDBContext::ConnectionClosed(const GURL& origin_url) {
  DCHECK(connection_count_[origin_url] > 0);
  if (quota_manager_proxy()) {
    quota_manager_proxy()->NotifyStorageAccessed(
        quota::QuotaClient::kIndexedDatabase, origin_url,
        quota::kStorageTypeTemporary);
  }
  connection_count_[origin_url]--;
  if (connection_count_[origin_url] == 0) {
    QueryDiskAndUpdateQuotaUsage(origin_url);
    connection_count_.erase(origin_url);
  }
}

void IndexedDBContext::TransactionComplete(const GURL& origin_url) {
  DCHECK(connection_count_[origin_url] > 0);
  QueryDiskAndUpdateQuotaUsage(origin_url);
  QueryAvailableQuota(origin_url);
}

bool IndexedDBContext::WouldBeOverQuota(const GURL& origin_url,
                                        int64 additional_bytes) {
  if (space_available_map_.find(origin_url) == space_available_map_.end()) {
    // We haven't heard back from the QuotaManager yet, just let it through.
    return false;
  }
  bool over_quota = additional_bytes > space_available_map_[origin_url];
  return over_quota;
}

bool IndexedDBContext::IsOverQuota(const GURL& origin_url) {
  const int kOneAdditionalByte = 1;
  return WouldBeOverQuota(origin_url, kOneAdditionalByte);
}

quota::QuotaManagerProxy* IndexedDBContext::quota_manager_proxy() {
  return quota_manager_proxy_;
}

int64 IndexedDBContext::ReadUsageFromDisk(const GURL& origin_url) const {
  string16 origin_id = DatabaseUtil::GetOriginIdentifier(origin_url);
  FilePath file_path = GetIndexedDBFilePath(origin_id);
  return file_util::ComputeDirectorySize(file_path);
}

void IndexedDBContext::EnsureDiskUsageCacheInitialized(const GURL& origin_url) {
  if (origin_size_map_.find(origin_url) == origin_size_map_.end())
    ResetDiskUsageCache(origin_url);
}

void IndexedDBContext::QueryDiskAndUpdateQuotaUsage(const GURL& origin_url) {
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

void IndexedDBContext::GotUpdatedQuota(const GURL& origin_url, int64 usage,
                                       int64 quota) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
  space_available_map_[origin_url] = quota - usage;
}

void IndexedDBContext::QueryAvailableQuota(const GURL& origin_url) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO)) {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::WEBKIT));
    if (quota_manager_proxy())
      BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
          NewRunnableMethod(this, &IndexedDBContext::QueryAvailableQuota,
                            origin_url));
    return;
  }
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
  if (!quota_manager_proxy()->quota_manager())
    return;
  IndexedDBGetUsageAndQuotaCallback* callback =
      new IndexedDBGetUsageAndQuotaCallback(this, origin_url);
  quota_manager_proxy()->quota_manager()->GetUsageAndQuota(
      origin_url,
      quota::kStorageTypeTemporary,
      callback);
}

int64 IndexedDBContext::ResetDiskUsageCache(const GURL& origin_url) {
  origin_size_map_[origin_url] = ReadUsageFromDisk(origin_url);
  return origin_size_map_[origin_url];
}
