// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_cache.h"

#include <string>

#include "base/files/file_path.h"

namespace content {

// static
ServiceWorkerCache* ServiceWorkerCache::CreateMemoryCache(
    const std::string& name) {
  return new ServiceWorkerCache(base::FilePath(), name);
}

// static
ServiceWorkerCache* ServiceWorkerCache::CreatePersistentCache(
    const base::FilePath& path,
    const std::string& name) {
  return new ServiceWorkerCache(path, name);
}

void ServiceWorkerCache::CreateBackend(
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

ServiceWorkerCache::ServiceWorkerCache(const base::FilePath& path,
                                       const std::string& name)
    : path_(path), name_(name), id_(0) {
}

ServiceWorkerCache::~ServiceWorkerCache() {
}

}  // namespace content
