// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_H_

#include <string>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/threading/thread_checker.h"

namespace base {
class MessageLoopProxy;
}

namespace content {

class ServiceWorkerFetchStore;

// The set of stores for a given ServiceWorker. It is owned by the
// ServiceWorkerFetchStoresManager. Provided callbacks are called on the
// |callback_loop| provided in the constructor.
class ServiceWorkerFetchStores {
 public:
  enum FetchStoresError {
    FETCH_STORES_ERROR_NO_ERROR,
    FETCH_STORES_ERROR_NOT_IMPLEMENTED,
    FETCH_STORES_ERROR_NOT_FOUND,
    FETCH_STORES_ERROR_EXISTS,
    FETCH_STORES_ERROR_STORAGE
  };

  enum BackendType { BACKEND_TYPE_SIMPLE_CACHE, BACKEND_TYPE_MEMORY };

  typedef base::Callback<void(bool, FetchStoresError)> BoolAndErrorCallback;
  typedef base::Callback<void(int, FetchStoresError)> StoreAndErrorCallback;
  typedef base::Callback<void(const std::vector<std::string>&,
                              FetchStoresError)> StringsAndErrorCallback;

  ServiceWorkerFetchStores(
      const base::FilePath& origin_path,
      BackendType backend,
      const scoped_refptr<base::MessageLoopProxy>& callback_loop);
  virtual ~ServiceWorkerFetchStores();

  void CreateStore(const std::string& key,
                   const StoreAndErrorCallback& callback);
  void Get(const std::string& key, const StoreAndErrorCallback& callback);
  void Has(const std::string& key, const BoolAndErrorCallback& callback) const;
  void Delete(const std::string& key, const StoreAndErrorCallback& callback);
  void Keys(const StringsAndErrorCallback& callback) const;
  // TODO(jkarlin): Add match() function.

 private:
  base::FilePath origin_path_;
  BackendType backend_type_;
  scoped_refptr<base::MessageLoopProxy> callback_loop_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchStores);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORES_H_
