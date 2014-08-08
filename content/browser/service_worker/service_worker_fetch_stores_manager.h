// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_MANAGER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/files/file_path.h"
#include "content/browser/service_worker/service_worker_fetch_stores.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace content {

// Keeps track of a ServiceWorkerFetchStores per origin. There is one
// ServiceWorkerFetchStoresManager per ServiceWorkerContextCore.
// TODO(jkarlin): Remove ServiceWorkerFetchStores from memory once they're no
// longer in active use.
class CONTENT_EXPORT ServiceWorkerFetchStoresManager {
 public:
  static scoped_ptr<ServiceWorkerFetchStoresManager> Create(
      const base::FilePath& path,
      base::SequencedTaskRunner* stores_task_runner);

  static scoped_ptr<ServiceWorkerFetchStoresManager> Create(
      ServiceWorkerFetchStoresManager* old_manager);

  virtual ~ServiceWorkerFetchStoresManager();

  // Methods to support the FetchStores spec. These methods call the
  // corresponding ServiceWorkerFetchStores method on the appropriate thread.
  void CreateStore(
      const GURL& origin,
      const std::string& store_name,
      const ServiceWorkerFetchStores::StoreAndErrorCallback& callback);
  void GetStore(
      const GURL& origin,
      const std::string& store_name,
      const ServiceWorkerFetchStores::StoreAndErrorCallback& callback);
  void HasStore(const GURL& origin,
                const std::string& store_name,
                const ServiceWorkerFetchStores::BoolAndErrorCallback& callback);
  void DeleteStore(
      const GURL& origin,
      const std::string& store_name,
      const ServiceWorkerFetchStores::BoolAndErrorCallback& callback);
  void EnumerateStores(
      const GURL& origin,
      const ServiceWorkerFetchStores::StringsAndErrorCallback& callback);
  // TODO(jkarlin): Add match() function.

  base::FilePath root_path() const { return root_path_; }
  scoped_refptr<base::SequencedTaskRunner> stores_task_runner() const {
    return stores_task_runner_;
  }

 private:
  typedef std::map<GURL, ServiceWorkerFetchStores*> ServiceWorkerFetchStoresMap;

  ServiceWorkerFetchStoresManager(
      const base::FilePath& path,
      base::SequencedTaskRunner* stores_task_runner);

  // The returned ServiceWorkerFetchStores* is owned by
  // service_worker_fetch_stores_.
  ServiceWorkerFetchStores* FindOrCreateServiceWorkerFetchStores(
      const GURL& origin);

  base::FilePath root_path_;
  scoped_refptr<base::SequencedTaskRunner> stores_task_runner_;

  // The map owns the stores and the stores are only accessed on
  // |stores_task_runner_|.
  ServiceWorkerFetchStoresMap service_worker_fetch_stores_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchStoresManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_MANAGER_H_
