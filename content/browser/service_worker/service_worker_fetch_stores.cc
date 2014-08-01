// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_stores.h"

#include <string>

#include "content/public/browser/browser_thread.h"

namespace content {

ServiceWorkerFetchStores::ServiceWorkerFetchStores(
    const base::FilePath& path,
    BackendType backend_type,
    const scoped_refptr<base::MessageLoopProxy>& callback_loop)
    : origin_path_(path),
      backend_type_(backend_type),
      callback_loop_(callback_loop) {
}

ServiceWorkerFetchStores::~ServiceWorkerFetchStores() {
}

void ServiceWorkerFetchStores::CreateStore(
    const std::string& key,
    const StoreAndErrorCallback& callback) {
  // TODO(jkarlin): Implement this.

  callback_loop_->PostTask(FROM_HERE,
                           base::Bind(callback, 0, FETCH_STORES_ERROR_EXISTS));
  return;
}

void ServiceWorkerFetchStores::Get(const std::string& key,
                                   const StoreAndErrorCallback& callback) {
  // TODO(jkarlin): Implement this.

  callback_loop_->PostTask(FROM_HERE,
                           base::Bind(callback, 0, FETCH_STORES_ERROR_EXISTS));
  return;
}

void ServiceWorkerFetchStores::Has(const std::string& key,
                                   const BoolAndErrorCallback& callback) const {
  // TODO(jkarlin): Implement this.

  callback_loop_->PostTask(
      FROM_HERE, base::Bind(callback, false, FETCH_STORES_ERROR_EXISTS));
  return;
}

void ServiceWorkerFetchStores::Delete(const std::string& key,
                                      const StoreAndErrorCallback& callback) {
  // TODO(jkarlin): Implement this.

  callback_loop_->PostTask(FROM_HERE,
                           base::Bind(callback, 0, FETCH_STORES_ERROR_EXISTS));
  return;
}

void ServiceWorkerFetchStores::Keys(
    const StringsAndErrorCallback& callback) const {
  // TODO(jkarlin): Implement this.
  std::vector<std::string> out;
  callback_loop_->PostTask(
      FROM_HERE, base::Bind(callback, out, FETCH_STORES_ERROR_EXISTS));
  return;
}

}  // namespace content
