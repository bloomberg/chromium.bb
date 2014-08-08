// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORE_H_
#define CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORE_H_

#include "base/callback.h"
#include "base/files/file_path.h"

namespace content {

// TODO(jkarlin): Fill this in with a real FetchStore implementation as
// specified in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html.
// TODO(jkarlin): Unload store backend from memory once the store object is no
// longer referenced in javascript.

// Represents a ServiceWorker FetchStore as seen in
// https://slightlyoff.github.io/ServiceWorker/spec/service_worker/index.html.
// InitializeIfNeeded must be called before calling the other public members.
class ServiceWorkerFetchStore {
 public:
  static ServiceWorkerFetchStore* CreateMemoryStore(const std::string& name);
  static ServiceWorkerFetchStore* CreatePersistentStore(
      const base::FilePath& path,
      const std::string& name);
  virtual ~ServiceWorkerFetchStore();

  // Loads the backend and calls the callback with the result (true for
  // success). This must be called before member functions that require a
  // backend are called.
  void CreateBackend(const base::Callback<void(bool)>& callback);

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }
  int32 id() const { return id_; }
  void set_id(int32 id) { id_ = id; }

 private:
  ServiceWorkerFetchStore(const base::FilePath& path, const std::string& name);

  base::FilePath path_;
  std::string name_;
  int32 id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerFetchStore);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_SERVICE_WORKER_FETCH_STORE_H_
