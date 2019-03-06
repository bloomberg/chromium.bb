// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_

#include <map>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

namespace content {

class EmbeddedWorkerInstance;
class ServiceWorkerContextCore;
class ServiceWorkerVersion;

// Hangs off ServiceWorkerContextCore (its reference is also held by each
// EmbeddedWorkerInstance).  Operated only on IO thread.
class CONTENT_EXPORT EmbeddedWorkerRegistry
    : public base::RefCounted<EmbeddedWorkerRegistry> {
 public:
  static scoped_refptr<EmbeddedWorkerRegistry> Create(
      const base::WeakPtr<ServiceWorkerContextCore>& context);

  // Used for DeleteAndStartOver. Creates a new registry which takes over
  // |next_embedded_worker_id_| and |process_sender_map_| from |old_registry|.
  static scoped_refptr<EmbeddedWorkerRegistry> Create(
      const base::WeakPtr<ServiceWorkerContextCore>& context,
      EmbeddedWorkerRegistry* old_registry);

  // Creates and removes a new worker instance entry for bookkeeping.
  // This doesn't actually start or stop the worker.
  std::unique_ptr<EmbeddedWorkerInstance> CreateWorker(
      ServiceWorkerVersion* owner_version);

  // Returns an embedded worker instance for given |embedded_worker_id|.
  EmbeddedWorkerInstance* GetWorker(int embedded_worker_id);

 private:
  friend class base::RefCounted<EmbeddedWorkerRegistry>;
  friend class EmbeddedWorkerInstance;
  friend class EmbeddedWorkerInstanceTest;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest,
                           RemoveWorkerInSharedProcess);

  using WorkerInstanceMap = std::map<int, EmbeddedWorkerInstance*>;

  EmbeddedWorkerRegistry(
      const base::WeakPtr<ServiceWorkerContextCore>& context,
      int initial_embedded_worker_id);
  ~EmbeddedWorkerRegistry();

  // RemoveWorker is called when EmbeddedWorkerInstance is destructed.
  void RemoveWorker(int embedded_worker_id);

  base::WeakPtr<ServiceWorkerContextCore> context_;

  WorkerInstanceMap worker_map_;

  int next_embedded_worker_id_;
  const int initial_embedded_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
