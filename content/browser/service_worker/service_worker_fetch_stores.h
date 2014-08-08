// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/id_map.h"
#include "base/threading/thread_checker.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ServiceWorkerFetchStore;

// TODO(jkarlin): Constrain the total bytes used per origin.
// The set of stores for a given origin. It is owned by the
// ServiceWorkerFetchStoresManager. Provided callbacks are called on the
// |callback_loop| provided in the constructor.
class ServiceWorkerFetchStores {
 public:
  enum FetchStoresError {
    FETCH_STORES_ERROR_NO_ERROR,
    FETCH_STORES_ERROR_NOT_IMPLEMENTED,
    FETCH_STORES_ERROR_NOT_FOUND,
    FETCH_STORES_ERROR_EXISTS,
    FETCH_STORES_ERROR_STORAGE,
    FETCH_STORES_ERROR_EMPTY_KEY,
  };

  typedef base::Callback<void(bool, FetchStoresError)> BoolAndErrorCallback;
  typedef base::Callback<void(int, FetchStoresError)> StoreAndErrorCallback;
  typedef base::Callback<void(const std::vector<std::string>&,
                              FetchStoresError)> StringsAndErrorCallback;

  ServiceWorkerFetchStores(
      const base::FilePath& origin_path,
      bool memory_only,
      const scoped_refptr<base::MessageLoopProxy>& callback_loop);

  virtual ~ServiceWorkerFetchStores();

  // Create a ServiceWorkerFetchStore if it doesn't already exist and call the
  // callback with the cache's id. If it already
  // exists the callback is called with FETCH_STORES_ERROR_EXISTS.
  void CreateStore(const std::string& store_name,
                   const StoreAndErrorCallback& callback);

  // Get the cache id for the given key. If not found returns
  // FETCH_STORES_ERROR_NOT_FOUND.
  void GetStore(const std::string& store_name,
                const StoreAndErrorCallback& callback);

  // Calls the callback with whether or not the cache exists.
  void HasStore(const std::string& store_name,
                const BoolAndErrorCallback& callback);

  // Deletes the cache if it exists. If it doesn't exist,
  // FETCH_STORES_ERROR_NOT_FOUND is returned.
  void DeleteStore(const std::string& store_name,
                   const BoolAndErrorCallback& callback);

  // Calls the callback with a vector of cache names (keys) available.
  void EnumerateStores(const StringsAndErrorCallback& callback);

  // TODO(jkarlin): Add match() function.

  void InitializeStoreCallback(const ServiceWorkerFetchStore* store,
                               const StoreAndErrorCallback& callback,
                               bool success);

 private:
  class MemoryLoader;
  class SimpleCacheLoader;
  class StoresLoader;

  typedef IDMap<ServiceWorkerFetchStore, IDMapOwnPointer> StoreMap;
  typedef StoreMap::KeyType StoreID;
  typedef std::map<std::string, StoreID> NameMap;

  ServiceWorkerFetchStore* GetLoadedStore(const std::string& key) const;

  void LazyInit();
  void InitStore(ServiceWorkerFetchStore* store);

  bool initialized_;
  StoreMap store_map_;
  NameMap name_map_;
  base::FilePath origin_path_;
  scoped_refptr<base::MessageLoopProxy> callback_loop_;
  scoped_ptr<StoresLoader> stores_loader_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchStores);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_H_
