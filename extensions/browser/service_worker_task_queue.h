// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_SERVICE_WORKER_TASK_QUEUE_H_
#define EXTENSIONS_BROWSER_SERVICE_WORKER_TASK_QUEUE_H_

#include <map>
#include <set>

#include "base/memory/weak_ptr.h"
#include "base/version.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/lazy_context_task_queue.h"
#include "extensions/common/extension_id.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class ServiceWorkerContext;
}

namespace extensions {
class Extension;
class LazyContextId;

// TODO(lazyboy): Clean up queue when extension is unloaded/uninstalled.
class ServiceWorkerTaskQueue : public KeyedService,
                               public LazyContextTaskQueue {
 public:
  explicit ServiceWorkerTaskQueue(content::BrowserContext* browser_context);
  ~ServiceWorkerTaskQueue() override;

  // Convenience method to return the ServiceWorkerTaskQueue for a given
  // |context|.
  static ServiceWorkerTaskQueue* Get(content::BrowserContext* context);

  bool ShouldEnqueueTask(content::BrowserContext* context,
                         const Extension* extension) override;
  void AddPendingTaskToDispatchEvent(
      const LazyContextId* context_id,
      LazyContextTaskQueue::PendingTask task) override;

  // Performs Service Worker related tasks upon |extension| activation,
  // e.g. registering |extension|'s worker, executing any pending tasks.
  void ActivateExtension(const Extension* extension);
  // Performs Service Worker related tasks upon |extension| deactivation,
  // e.g. unregistering |extension|'s worker.
  void DeactivateExtension(const Extension* extension);

  // Called once an extension Service Worker started running.
  void DidStartServiceWorkerContext(const ExtensionId& extension_id,
                                    int64_t service_worker_version_id);
  // Called once an extension Service Worker was destroyed.
  void DidStopServiceWorkerContext(const ExtensionId& extension_id,
                                   int64_t service_worker_version_id);

  class TestObserver {
   public:
    TestObserver() = default;
    virtual ~TestObserver() = default;

    // Called when an extension with id |extension_id| is going to be activated.
    // |will_register_service_worker| is true if a Service Worker will be
    // registered.
    virtual void OnActivateExtension(const ExtensionId& extension_id,
                                     bool will_register_service_worker) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(TestObserver);
  };

  static void SetObserverForTest(TestObserver* observer);

 private:
  struct TaskInfo;

  static void DidStartWorkerForScopeOnIO(
      LazyContextTaskQueue::PendingTask task,
      const ExtensionId& extension_id,
      base::WeakPtr<ServiceWorkerTaskQueue> task_queue,
      int64_t version_id,
      int process_id,
      int thread_id);
  static void StartServiceWorkerOnIOToRunTask(
      base::WeakPtr<ServiceWorkerTaskQueue> task_queue_weak,
      const GURL& scope,
      const ExtensionId& extension_id,
      content::ServiceWorkerContext* service_worker_context,
      LazyContextTaskQueue::PendingTask task);

  void RunTaskAfterStartWorker(const LazyContextId* context_id,
                               LazyContextTaskQueue::PendingTask task);

  void DidRegisterServiceWorker(const ExtensionId& extension_id, bool success);
  void DidUnregisterServiceWorker(const ExtensionId& extension_id,
                                  bool success);

  void DidStartWorkerForScope(LazyContextTaskQueue::PendingTask task,
                              const ExtensionId& extension_id,
                              int64_t version_id,
                              int process_id,
                              int thread_id);

  // The following three methods retrieve, store, and remove information
  // about Service Worker registration of SW based background pages:
  //
  // Retrieves the last version of the extension, returns invalid version if
  // there isn't any such extension.
  base::Version RetrieveRegisteredServiceWorkerVersion(
      const ExtensionId& extension_id);
  // Records that the extension with |extension_id| and |version| successfully
  // registered a Service Worker.
  void SetRegisteredServiceWorkerInfo(const ExtensionId& extension_id,
                                      const base::Version& version);
  // Clears any record of registered Service Worker for the given extension with
  // |extension_id|.
  void RemoveRegisteredServiceWorkerInfo(const ExtensionId& extension_id);

  // Set of extension ids that hasn't completed Service Worker registration.
  std::set<ExtensionId> pending_registrations_;

  // Map of extension id -> pending tasks. These are run once the Service Worker
  // registration of the extension completes.
  std::map<ExtensionId, std::vector<TaskInfo>> pending_tasks_;

  struct WaitingDidStartWorkerTask;

  using WaitingDidStartWorkerTaskKey =
      std::pair<ExtensionId, int64_t /* service_worker_version_id */>;
  // All service workers that are currently loaded.
  std::set<WaitingDidStartWorkerTaskKey> running_service_worker_contexts_;
  // All service worker tasks that are waiting for the worker to start.
  std::map<WaitingDidStartWorkerTaskKey, std::vector<WaitingDidStartWorkerTask>>
      waiting_did_start_worker_tasks_;

  content::BrowserContext* const browser_context_ = nullptr;

  base::WeakPtrFactory<ServiceWorkerTaskQueue> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTaskQueue);
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_SERVICE_WORKER_TASK_QUEUE_H_
