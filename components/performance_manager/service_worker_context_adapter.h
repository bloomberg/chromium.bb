// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CONTEXT_ADAPTER_H_
#define COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CONTEXT_ADAPTER_H_

#include <memory>
#include <string>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "base/scoped_observer.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"

namespace performance_manager {

// This class adapts an existing ServiceWorkerContext to ensure that the
// OnVersionStoppedRunning() notifications are sent as soon as the render
// process of a running service worker exits.
//
// It implements ServiceWorkerContext so it can be used interchangeably where a
// ServiceWorkerContext* is needed, and it also observes the underlying context
// so that it can receive the original notifications and control when they are
// sent to the observers.
//
// Lives on the UI thread. Must outlive |underlying_context|.
//
// Note: This is a temporary class that can be removed when the representation
//       of a worker in the content/ layer (ServiceWorkerVersion) is moved to
//       the UI thread. At that point, it'll be able to observe its associated
//       RenderProcessHost itself. See https://crbug.com/824858.
class ServiceWorkerContextAdapter
    : public content::ServiceWorkerContext,
      public content::ServiceWorkerContextObserver {
 public:
  explicit ServiceWorkerContextAdapter(
      content::ServiceWorkerContext* underlying_context);
  ~ServiceWorkerContextAdapter() override;

  // content::ServiceWorkerContext:
  // Note that this is a minimal implementation for the use case of the
  // PerformanceManager. Only AddObserver/RemoveObserver are implemented.
  void AddObserver(content::ServiceWorkerContextObserver* observer) override;
  void RemoveObserver(content::ServiceWorkerContextObserver* observer) override;
  void RegisterServiceWorker(
      const GURL& script_url,
      const blink::mojom::ServiceWorkerRegistrationOptions& options,
      ResultCallback callback) override;
  void UnregisterServiceWorker(const GURL& scope,
                               ResultCallback callback) override;
  content::ServiceWorkerExternalRequestResult StartingExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) override;
  content::ServiceWorkerExternalRequestResult FinishedExternalRequest(
      int64_t service_worker_version_id,
      const std::string& request_uuid) override;
  void CountExternalRequestsForTest(
      const GURL& origin,
      CountExternalRequestsCallback callback) override;
  void GetAllOriginsInfo(GetUsageInfoCallback callback) override;
  void DeleteForOrigin(const GURL& origin_url,
                       ResultCallback callback) override;
  void PerformStorageCleanup(base::OnceClosure callback) override;
  void CheckHasServiceWorker(const GURL& url,
                             CheckHasServiceWorkerCallback callback) override;
  void CheckOfflineCapability(const GURL& url,
                              CheckOfflineCapabilityCallback callback) override;
  void ClearAllServiceWorkersForTest(base::OnceClosure callback) override;
  void StartWorkerForScope(const GURL& scope,
                           StartWorkerCallback info_callback,
                           base::OnceClosure failure_callback) override;
  void StartServiceWorkerAndDispatchMessage(
      const GURL& scope,
      blink::TransferableMessage message,
      ResultCallback result_callback) override;
  void StartServiceWorkerForNavigationHint(
      const GURL& document_url,
      StartServiceWorkerForNavigationHintCallback callback) override;
  void StopAllServiceWorkersForOrigin(const GURL& origin) override;
  void StopAllServiceWorkers(base::OnceClosure callback) override;
  const base::flat_map<int64_t /* version_id */,
                       content::ServiceWorkerRunningInfo>&
  GetRunningServiceWorkerInfos() override;

  // content::ServiceWorkerContextObserver:
  void OnRegistrationCompleted(const GURL& scope) override;
  void OnRegistrationStored(int64_t registration_id,
                            const GURL& scope) override;
  void OnVersionActivated(int64_t version_id, const GURL& scope) override;
  void OnVersionRedundant(int64_t version_id, const GURL& scope) override;
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(int64_t version_id) override;
  void OnNoControllees(int64_t version_id, const GURL& scope) override;
  void OnReportConsoleMessage(int64_t version_id,
                              const content::ConsoleMessage& message) override;
  void OnDestruct(ServiceWorkerContext* context) override;

 private:
  class RunningServiceWorker;

  // Invoked by a RunningServiceWorker when it observes that the render process
  // has exited.
  void OnRenderProcessExited(int64_t version_id);

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      scoped_underlying_context_observer_{this};

  base::ObserverList<content::ServiceWorkerContextObserver, true, false>::
      Unchecked observer_list_;

  // For each running service worker, tracks when their render process exits.
  base::flat_map<int64_t /*version_id*/, std::unique_ptr<RunningServiceWorker>>
      running_service_workers_;

#if DCHECK_IS_ON()
  // Keeps track of service worker whose render process exited early.
  base::flat_set<int64_t> stopped_service_workers_;
#endif  // DCHECK_IS_ON()
};

}  // namespace performance_manager

#endif  // COMPONENTS_PERFORMANCE_MANAGER_SERVICE_WORKER_CONTEXT_ADAPTER_H_
