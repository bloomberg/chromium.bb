// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_store.h"

#include <string>

#include "base/files/file_path.h"

namespace content {

// static
ServiceWorkerFetchStore* ServiceWorkerFetchStore::CreateMemoryStore(
    const std::string& name) {
  return new ServiceWorkerFetchStore(base::FilePath(), name);
}

// static
ServiceWorkerFetchStore* ServiceWorkerFetchStore::CreatePersistentStore(
    const base::FilePath& path,
    const std::string& name) {
  return new ServiceWorkerFetchStore(path, name);
}

void ServiceWorkerFetchStore::CreateBackend(
    const base::Callback<void(bool)>& callback) {
  callback.Run(true);
}

ServiceWorkerFetchStore::ServiceWorkerFetchStore(const base::FilePath& path,
                                                 const std::string& name)
    : path_(path), name_(name), id_(0) {
}

ServiceWorkerFetchStore::~ServiceWorkerFetchStore() {
}

}  // namespace content
