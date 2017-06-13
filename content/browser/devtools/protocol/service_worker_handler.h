// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_
#define CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_

#include <stdint.h>

#include <set>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "content/browser/devtools/protocol/devtools_domain_handler.h"
#include "content/browser/devtools/protocol/service_worker.h"
#include "content/browser/devtools/service_worker_devtools_agent_host.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core_observer.h"
#include "content/browser/service_worker/service_worker_info.h"

namespace content {

class RenderFrameHostImpl;
class ServiceWorkerContextWatcher;
class ServiceWorkerContextWrapper;

namespace protocol {

class ServiceWorkerHandler : public DevToolsDomainHandler,
                             public ServiceWorker::Backend {
 public:
  ServiceWorkerHandler();
  ~ServiceWorkerHandler() override;

  void Wire(UberDispatcher* dispatcher) override;
  void SetRenderFrameHost(RenderFrameHostImpl* host) override;

  Response Enable() override;
  Response Disable() override;
  Response Unregister(const std::string& scope_url) override;
  Response StartWorker(const std::string& scope_url) override;
  Response SkipWaiting(const std::string& scope_url) override;
  Response StopWorker(const std::string& version_id) override;
  Response UpdateRegistration(const std::string& scope_url) override;
  Response InspectWorker(const std::string& version_id) override;
  Response SetForceUpdateOnPageLoad(bool force_update_on_page_load) override;
  Response DeliverPushMessage(const std::string& origin,
                              const std::string& registration_id,
                              const std::string& data) override;
  Response DispatchSyncEvent(const std::string& origin,
                             const std::string& registration_id,
                             const std::string& tag,
                             bool last_chance) override;

 private:
  void OnWorkerRegistrationUpdated(
      const std::vector<ServiceWorkerRegistrationInfo>& registrations);
  void OnWorkerVersionUpdated(
      const std::vector<ServiceWorkerVersionInfo>& registrations);
  void OnErrorReported(int64_t registration_id,
                       int64_t version_id,
                       const ServiceWorkerContextCoreObserver::ErrorInfo& info);

  void OpenNewDevToolsWindow(int process_id, int devtools_agent_route_id);
  void ClearForceUpdate();

  scoped_refptr<ServiceWorkerContextWrapper> context_;
  std::unique_ptr<ServiceWorker::Frontend> frontend_;
  bool enabled_;
  scoped_refptr<ServiceWorkerContextWatcher> context_watcher_;
  RenderFrameHostImpl* render_frame_host_;

  base::WeakPtrFactory<ServiceWorkerHandler> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerHandler);
};

}  // namespace protocol
}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_PROTOCOL_SERVICE_WORKER_HANDLER_H_
