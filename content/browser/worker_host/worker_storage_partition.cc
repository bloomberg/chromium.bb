// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/worker_host/worker_storage_partition.h"

#include <string>

#include "content/browser/appcache/chrome_appcache_service.h"
#include "content/browser/indexed_db/indexed_db_context_impl.h"
#include "net/url_request/url_request_context_getter.h"
#include "webkit/database/database_tracker.h"
#include "webkit/fileapi/file_system_context.h"
#include "webkit/quota/quota_manager.h"

namespace content {

WorkerStoragePartition::WorkerStoragePartition(
    net::URLRequestContextGetter* url_request_context,
    net::URLRequestContextGetter* media_url_request_context,
    ChromeAppCacheService* appcache_service,
    quota::QuotaManager* quota_manager,
    fileapi::FileSystemContext* filesystem_context,
    webkit_database::DatabaseTracker* database_tracker,
    IndexedDBContextImpl* indexed_db_context)
    : url_request_context_(url_request_context),
      media_url_request_context_(media_url_request_context),
      appcache_service_(appcache_service),
      quota_manager_(quota_manager),
      filesystem_context_(filesystem_context),
      database_tracker_(database_tracker),
      indexed_db_context_(indexed_db_context) {
}

WorkerStoragePartition::WorkerStoragePartition(
    const WorkerStoragePartition& other) {
  Copy(other);
}

const WorkerStoragePartition& WorkerStoragePartition::operator=(
    const WorkerStoragePartition& rhs) {
  Copy(rhs);
  return *this;
}

bool WorkerStoragePartition::Equals(
    const WorkerStoragePartition& other) const {
  return url_request_context_ == other.url_request_context_ &&
         media_url_request_context_ == other.media_url_request_context_ &&
         appcache_service_ == other.appcache_service_ &&
         quota_manager_ == other.quota_manager_ &&
         filesystem_context_ == other.filesystem_context_ &&
         database_tracker_ == other.database_tracker_ &&
         indexed_db_context_ == other.indexed_db_context_;
}

WorkerStoragePartition::~WorkerStoragePartition() {
}

void WorkerStoragePartition::Copy(const WorkerStoragePartition& other) {
  url_request_context_ = other.url_request_context_;
  media_url_request_context_ = other.media_url_request_context_;
  appcache_service_ = other.appcache_service_;
  quota_manager_ = other.quota_manager_;
  filesystem_context_ = other.filesystem_context_;
  database_tracker_ = other.database_tracker_;
  indexed_db_context_ = other.indexed_db_context_;
}

}  // namespace content
