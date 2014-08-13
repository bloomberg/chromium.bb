// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache.h"

#include <string>

#include "base/files/file_path.h"

namespace content {

// static
scoped_ptr<ServiceWorkerCache> ServiceWorkerCache::CreateMemoryCache(
    const std::string& name) {
  return make_scoped_ptr(new ServiceWorkerCache(base::FilePath(), name));
}

// static
scoped_ptr<ServiceWorkerCache> ServiceWorkerCache::CreatePersistentCache(
    const base::FilePath& path,
    const std::string& name) {
  return make_scoped_ptr(new ServiceWorkerCache(path, name));
}

void ServiceWorkerCache::CreateBackend(
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

base::WeakPtr<ServiceWorkerCache> ServiceWorkerCache::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

ServiceWorkerCache::ServiceWorkerCache(const base::FilePath& path,
                                       const std::string& name)
    : path_(path), name_(name), id_(0), weak_ptr_factory_(this) {
}

ServiceWorkerCache::~ServiceWorkerCache() {
}

}  // namespace content
