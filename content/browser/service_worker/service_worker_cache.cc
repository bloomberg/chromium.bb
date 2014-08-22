// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache.h"

#include <string>

#include "base/files/file_path.h"
#include "net/url_request/url_request_context.h"
#include "webkit/browser/blob/blob_storage_context.h"

namespace content {

// static
scoped_ptr<ServiceWorkerCache> ServiceWorkerCache::CreateMemoryCache(
    const std::string& name,
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_ptr(new ServiceWorkerCache(
      base::FilePath(), name, request_context, blob_context));
}

// static
scoped_ptr<ServiceWorkerCache> ServiceWorkerCache::CreatePersistentCache(
    const base::FilePath& path,
    const std::string& name,
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context) {
  return make_scoped_ptr(
      new ServiceWorkerCache(path, name, request_context, blob_context));
}

void ServiceWorkerCache::CreateBackend(
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

base::WeakPtr<ServiceWorkerCache> ServiceWorkerCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ServiceWorkerCache::ServiceWorkerCache(
    const base::FilePath& path,
    const std::string& name,
    net::URLRequestContext* request_context,
    base::WeakPtr<storage::BlobStorageContext> blob_context)
    : path_(path),
      name_(name),
      request_context_(request_context),
      blob_storage_context_(blob_context),
      id_(0),
      weak_ptr_factory_(this) {
}

ServiceWorkerCache::~ServiceWorkerCache() {
}

}  // namespace content
