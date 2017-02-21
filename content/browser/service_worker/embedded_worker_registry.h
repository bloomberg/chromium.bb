// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
#define CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/service_worker_status_code.h"

class GURL;

namespace IPC {
class Message;
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
  static scoped_refptr<EmbeddedWorkerRegistry> Create(
      const base::WeakPtr<ServiceWorkerContextCore>& contxet);

  // Used for DeleteAndStartOver. Creates a new registry which takes over
  // |next_embedded_worker_id_| and |process_sender_map_| from |old_registry|.
  static scoped_refptr<EmbeddedWorkerRegistry> Create(
      const base::WeakPtr<ServiceWorkerContextCore>& context,
      EmbeddedWorkerRegistry* old_registry);

  bool OnMessageReceived(const IPC::Message& message, int process_id);

  // Creates and removes a new worker instance entry for bookkeeping.
  // This doesn't actually start or stop the worker.
  std::unique_ptr<EmbeddedWorkerInstance> CreateWorker();

  // Called from EmbeddedWorkerInstance, relayed to the child process.
  ServiceWorkerStatusCode StopWorker(int process_id,
                                     int embedded_worker_id);

  // Stop all active workers, even if they're handling events.
  void Shutdown();

  // Called back from EmbeddedWorker in the child process, relayed via
  // ServiceWorkerDispatcherHost.
  void OnWorkerReadyForInspection(int process_id, int embedded_worker_id);
  void OnWorkerScriptLoaded(int process_id, int embedded_worker_id);
  void OnWorkerThreadStarted(int process_id,
                             int thread_id,
                             int embedded_worker_id);
  void OnWorkerScriptLoadFailed(int process_id, int embedded_worker_id);
  void OnWorkerScriptEvaluated(int process_id,
                               int embedded_worker_id,
                               bool success);
  void OnWorkerStarted(int process_id, int embedded_worker_id);
  void OnWorkerStopped(int process_id, int embedded_worker_id);
  void OnReportException(int embedded_worker_id,
                         const base::string16& error_message,
                         int line_number,
                         int column_number,
                         const GURL& source_url);
  void OnReportConsoleMessage(int embedded_worker_id,
                              int source_identifier,
                              int message_level,
                              const base::string16& message,
                              int line_number,
                              const GURL& source_url);

  // Removes information about the service workers running on the process and
  // calls ServiceWorkerVersion::OnDetached() on each. Called when the process
  // is terminated. Under normal operation, the workers should already have
  // been stopped before the process is terminated, in which case this function
  // does nothing. But in some cases the process can be terminated unexpectedly
  // or the workers can fail to stop cleanly.
  void RemoveProcess(int process_id);

  // Returns an embedded worker instance for given |embedded_worker_id|.
  EmbeddedWorkerInstance* GetWorker(int embedded_worker_id);

  // Returns true if |embedded_worker_id| is managed by this registry.
  bool CanHandle(int embedded_worker_id) const;

 private:
  friend class base::RefCounted<EmbeddedWorkerRegistry>;
  friend class MojoEmbeddedWorkerInstanceTest;
  friend class EmbeddedWorkerInstance;
  friend class EmbeddedWorkerInstanceTest;
  FRIEND_TEST_ALL_PREFIXES(EmbeddedWorkerInstanceTest,
                           RemoveWorkerInSharedProcess);

  using WorkerInstanceMap = std::map<int, EmbeddedWorkerInstance*>;

  EmbeddedWorkerRegistry(
      const base::WeakPtr<ServiceWorkerContextCore>& context,
      int initial_embedded_worker_id);
  ~EmbeddedWorkerRegistry();

  ServiceWorkerStatusCode Send(int process_id, IPC::Message* message);

  // Called when EmbeddedWorkerInstance is ready for IPC. This function
  // prepares a route to the child worker thread.
  // TODO(shimazu): Remove this function once mojofication is completed.
  void BindWorkerToProcess(int process_id, int embedded_worker_id);

  // RemoveWorker is called when EmbeddedWorkerInstance is destructed.
  // |process_id| could be invalid (i.e. ChildProcessHost::kInvalidUniqueID)
  // if it's not running.
  void RemoveWorker(int process_id, int embedded_worker_id);
  // DetachWorker is called when EmbeddedWorkerInstance releases a process.
  // |process_id| could be invalid (i.e. ChildProcessHost::kInvalidUniqueID)
  // if it's not running.
  void DetachWorker(int process_id, int embedded_worker_id);

  EmbeddedWorkerInstance* GetWorkerForMessage(int process_id,
                                              int embedded_worker_id);

  base::WeakPtr<ServiceWorkerContextCore> context_;

  WorkerInstanceMap worker_map_;

  // Map from process_id to embedded_worker_id.
  // This map only contains starting and running workers.
  std::map<int, std::set<int> > worker_process_map_;

  int next_embedded_worker_id_;
  const int initial_embedded_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(EmbeddedWorkerRegistry);
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_EMBEDDED_WORKER_REGISTRY_H_
