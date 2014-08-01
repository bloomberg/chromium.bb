// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/service_worker_fetch_stores_manager.h"

#include <map>
#include <string>

#include "base/bind.h"
#include "base/id_map.h"
#include "base/sha1.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_fetch_stores.h"
#include "content/public/browser/browser_thread.h"
#include "url/gurl.h"

namespace {

base::FilePath ConstructOriginPath(const base::FilePath& root_path,
                                   const GURL& origin) {
  std::string origin_hash = base::SHA1HashString(origin.spec());
  std::string origin_hash_hex = StringToLowerASCII(
      base::HexEncode(origin_hash.c_str(), origin_hash.length()));
  return root_path.AppendASCII(origin_hash_hex);
}

}  // namespace

namespace content {

// static
scoped_ptr<ServiceWorkerFetchStoresManager>
ServiceWorkerFetchStoresManager::Create(
    const base::FilePath& path,
    base::SequencedTaskRunner* stores_task_runner) {
  base::FilePath root_path = path;
  if (!path.empty()) {
    root_path = path.Append(ServiceWorkerContextCore::kServiceWorkerDirectory)
                    .AppendASCII("Stores");
  }

  return make_scoped_ptr(
      new ServiceWorkerFetchStoresManager(root_path, stores_task_runner));
}

// static
scoped_ptr<ServiceWorkerFetchStoresManager>
ServiceWorkerFetchStoresManager::Create(
    ServiceWorkerFetchStoresManager* old_manager) {
  return make_scoped_ptr(new ServiceWorkerFetchStoresManager(
      old_manager->root_path(), old_manager->stores_task_runner()));
}

ServiceWorkerFetchStoresManager::~ServiceWorkerFetchStoresManager() {
  for (ServiceWorkerFetchStoresMap::iterator it =
           service_worker_fetch_stores_.begin();
       it != service_worker_fetch_stores_.end();
       ++it) {
    stores_task_runner_->DeleteSoon(FROM_HERE, it->second);
  }
}

void ServiceWorkerFetchStoresManager::CreateStore(
    const GURL& origin,
    const std::string& key,
    const ServiceWorkerFetchStores::StoreAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerFetchStores* stores =
      FindOrCreateServiceWorkerFetchStores(origin);

  stores_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&ServiceWorkerFetchStores::CreateStore,
                 base::Unretained(stores),
                 key,
                 callback));
}

void ServiceWorkerFetchStoresManager::Get(
    const GURL& origin,
    const std::string& key,
    const ServiceWorkerFetchStores::StoreAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerFetchStores* stores =
      FindOrCreateServiceWorkerFetchStores(origin);
  stores_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ServiceWorkerFetchStores::Get,
                                           base::Unretained(stores),
                                           key,
                                           callback));
}

void ServiceWorkerFetchStoresManager::Has(
    const GURL& origin,
    const std::string& key,
    const ServiceWorkerFetchStores::BoolAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerFetchStores* stores =
      FindOrCreateServiceWorkerFetchStores(origin);
  stores_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ServiceWorkerFetchStores::Has,
                                           base::Unretained(stores),
                                           key,
                                           callback));
}

void ServiceWorkerFetchStoresManager::Delete(
    const GURL& origin,
    const std::string& key,
    const ServiceWorkerFetchStores::StoreAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerFetchStores* stores =
      FindOrCreateServiceWorkerFetchStores(origin);
  stores_task_runner_->PostTask(FROM_HERE,
                                base::Bind(&ServiceWorkerFetchStores::Delete,
                                           base::Unretained(stores),
                                           key,
                                           callback));
}

void ServiceWorkerFetchStoresManager::Keys(
    const GURL& origin,
    const ServiceWorkerFetchStores::StringsAndErrorCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  ServiceWorkerFetchStores* stores =
      FindOrCreateServiceWorkerFetchStores(origin);

  stores_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(
          &ServiceWorkerFetchStores::Keys, base::Unretained(stores), callback));
}

ServiceWorkerFetchStoresManager::ServiceWorkerFetchStoresManager(
    const base::FilePath& path,
    base::SequencedTaskRunner* stores_task_runner)
    : root_path_(path), stores_task_runner_(stores_task_runner) {
}

ServiceWorkerFetchStores*
ServiceWorkerFetchStoresManager::FindOrCreateServiceWorkerFetchStores(
    const GURL& origin) {
  ServiceWorkerFetchStoresMap::const_iterator it =
      service_worker_fetch_stores_.find(origin);
  if (it == service_worker_fetch_stores_.end()) {
    ServiceWorkerFetchStores::BackendType backend =
        root_path_.empty()
            ? ServiceWorkerFetchStores::BACKEND_TYPE_MEMORY
            : ServiceWorkerFetchStores::BACKEND_TYPE_SIMPLE_CACHE;
    ServiceWorkerFetchStores* fetch_stores =
        new ServiceWorkerFetchStores(ConstructOriginPath(root_path_, origin),
                                     backend,
                                     base::MessageLoopProxy::current());
    // The map owns fetch_stores.
    service_worker_fetch_stores_.insert(std::make_pair(origin, fetch_stores));
    return fetch_stores;
  }
  return it->second;
}

}  // namespace content
