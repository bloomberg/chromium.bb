// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_

#include <map>
#include <set>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"

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

  // Called from EmbeddedWorkerInstance, relayed to the child process.
  ServiceWorkerStatusCode StartWorker(int process_id,
                                      int embedded_worker_id,
                                      int64 service_worker_version_id,
                                      const GURL& scope,
                                      const GURL& script_url);
  ServiceWorkerStatusCode StopWorker(int process_id,
                                     int embedded_worker_id);

  // Called back from EmbeddedWorker in the child process, relayed via
  // ServiceWorkerDispatcherHost.
  void OnWorkerStarted(int process_id, int thread_id, int embedded_worker_id);
  void OnWorkerStopped(int process_id, int embedded_worker_id);
  // FIXME(dominicc): Rename this. The name leads to confusion that
  // this sends a message to the browser itself.
  void OnSendMessageToBrowser(int embedded_worker_id,
                              int request_id,
                              const IPC::Message& message);
  void OnReportException(int embedded_worker_id,
                         const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);

  // Keeps a map from process_id to sender information.
  void AddChildProcessSender(int process_id, IPC::Sender* sender);
  void RemoveChildProcessSender(int process_id);

  // Returns an embedded worker instance for given |embedded_worker_id|.
  EmbeddedWorkerInstance* GetWorker(int embedded_worker_id);

 private:
  friend class base::RefCounted<EmbeddedWorkerRegistry>;
  friend class EmbeddedWorkerInstance;

  typedef std::map<int, EmbeddedWorkerInstance*> WorkerInstanceMap;
  typedef std::map<int, IPC::Sender*> ProcessToSenderMap;

  ~EmbeddedWorkerRegistry();
  ServiceWorkerStatusCode Send(int process_id, IPC::Message* message);

  // RemoveWorker is called when EmbeddedWorkerInstance is destructed.
  // |process_id| could be invalid (i.e. -1) if it's not running.
  void RemoveWorker(int process_id, int embedded_worker_id);

  base::WeakPtr<ServiceWorkerContextCore> context_;

  WorkerInstanceMap worker_map_;
  ProcessToSenderMap process_sender_map_;

  // Map from process_id to embedded_worker_id.
  // This map only contains running workers.
  std::map<int, std::set<int> > worker_process_map_;

  int next_embedded_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
