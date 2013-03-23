// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WORKER_HOST_WORKER_STORAGE_PARTITION_H_
#define CONTENT_BROWSER_WORKER_HOST_WORKER_STORAGE_PARTITION_H_

#include "base/memory/ref_counted.h"

namespace quota {
class QuotaManager;
}

namespace fileapi {
class FileSystemContext;
}  // namespace fileapi

namespace net {
class URLRequestContextGetter;
}

namespace webkit_database {
class DatabaseTracker;
}  // namespace webkit_database

namespace content {
class ChromeAppCacheService;
class IndexedDBContextImpl;

// Contains the data from StoragePartition for use by Worker APIs.
//
// StoragePartition meant for the UI so we can't use it directly in many
// Worker APIs that run on the IO thread. While we could make it RefCounted,
// the Worker system is the only place that really needs access on the IO
// thread. Instead of changing the lifetime semantics of StoragePartition,
// we just create a parallel struct here to simplify the APIs of various
// methods in WorkerServiceImpl.
//
// This class is effectively a struct, but we make it a class because we want to
// define copy constructors, assignment operators, and an Equals() function for
// it which makes it look awkward as a struct.
class WorkerStoragePartition {
 public:
  WorkerStoragePartition(
      net::URLRequestContextGetter* url_request_context,
      net::URLRequestContextGetter* media_url_request_context,
      ChromeAppCacheService* appcache_service,
      quota::QuotaManager* quota_manager,
      fileapi::FileSystemContext* filesystem_context,
      webkit_database::DatabaseTracker* database_tracker,
      IndexedDBContextImpl* indexed_db_context);
  ~WorkerStoragePartition();

  // Declaring so these don't get inlined which has the unfortunate effect of
  // requiring all including classes to have the full definition of every member
  // type.
  WorkerStoragePartition(const WorkerStoragePartition& other);
  const WorkerStoragePartition& operator=(const WorkerStoragePartition& rhs);

  bool Equals(const WorkerStoragePartition& other) const;

  net::URLRequestContextGetter* url_request_context() const {
    return url_request_context_.get();
  }

  net::URLRequestContextGetter* media_url_request_context() const {
    return media_url_request_context_.get();
  }

  ChromeAppCacheService* appcache_service() const {
    return appcache_service_.get();
  }

  quota::QuotaManager* quota_manager() const {
    return quota_manager_.get();
  }

  fileapi::FileSystemContext* filesystem_context() const {
    return filesystem_context_.get();
  }

  webkit_database::DatabaseTracker* database_tracker() const {
    return database_tracker_.get();
  }

  IndexedDBContextImpl* indexed_db_context() const {
    return indexed_db_context_.get();
  }

 private:
  void Copy(const WorkerStoragePartition& other);

  scoped_refptr<net::URLRequestContextGetter> url_request_context_;
  scoped_refptr<net::URLRequestContextGetter> media_url_request_context_;
  scoped_refptr<ChromeAppCacheService> appcache_service_;
  scoped_refptr<quota::QuotaManager> quota_manager_;
  scoped_refptr<fileapi::FileSystemContext> filesystem_context_;
  scoped_refptr<webkit_database::DatabaseTracker> database_tracker_;
  scoped_refptr<IndexedDBContextImpl> indexed_db_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_WORKER_HOST_WORKER_STORAGE_PARTITION_H_
