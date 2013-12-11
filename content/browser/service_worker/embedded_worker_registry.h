// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_

#include <map>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"

class GURL;

namespace IPC {
class Message;
class Sender;
}

namespace content {

class EmbeddedWorkerInstance;
class ServiceWorkerContextCore;

// Acts as a thin stub between MessageFilter and each EmbeddedWorkerInstance,
// which sends/receives messages to/from each EmbeddedWorker in child process.
//
// Hangs off ServiceWorkerContextCore (its reference is also held by each
// EmbeddedWorkerInstance).  Operated only on IO thread.
class CONTENT_EXPORT EmbeddedWorkerRegistry
    : public NON_EXPORTED_BASE(base::RefCounted<EmbeddedWorkerRegistry>) {
 public:
  explicit EmbeddedWorkerRegistry(
      base::WeakPtr<ServiceWorkerContextCore> context);

  // Creates and removes a new worker instance entry for bookkeeping.
  // This doesn't actually start or stop the worker.
  scoped_ptr<EmbeddedWorkerInstance> CreateWorker();
  void RemoveWorker(int embedded_worker_id);

  // Called from EmbeddedWorkerInstance, relayed to the child process.
  bool StartWorker(int process_id,
                   int embedded_worker_id,
                   int64 service_worker_version_id,
                   const GURL& script_url);
  bool StopWorker(int process_id,
                  int embedded_worker_id);

  // Keeps a map from process_id to sender information.
  void AddChildProcessSender(int process_id, IPC::Sender* sender);
  void RemoveChildProcessSender(int process_id);

 private:
  friend class base::RefCounted<EmbeddedWorkerRegistry>;

  ~EmbeddedWorkerRegistry();
  bool Send(int process_id, IPC::Message* message);

  typedef std::map<int, EmbeddedWorkerInstance*> WorkerInstanceMap;
  typedef std::map<int, IPC::Sender*> ProcessToSenderMap;

  base::WeakPtr<ServiceWorkerContextCore> context_;

  WorkerInstanceMap worker_map_;
  ProcessToSenderMap process_sender_map_;

  int next_embedded_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
