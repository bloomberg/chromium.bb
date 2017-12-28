// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_

#include <stdint.h>

#include <map>

#include "base/containers/flat_set.h"
#include "base/macros.h"
#include "base/memory/singleton.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/unguessable_token.h"
#include "content/common/content_export.h"
#include "url/gurl.h"

namespace network {
struct URLLoaderCompletionStatus;
}

namespace content {

class BrowserContext;
struct ResourceRequest;
struct ResourceResponseHead;
class ServiceWorkerDevToolsAgentHost;
class ServiceWorkerContextCore;

// Manages ServiceWorkerDevToolsAgentHost's. This class lives on UI thread.
class CONTENT_EXPORT ServiceWorkerDevToolsManager {
 public:
  class Observer {
   public:
    virtual void WorkerCreated(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerReadyForInspection(
        ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerVersionInstalled(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerVersionDoomed(ServiceWorkerDevToolsAgentHost* host) {}
    virtual void WorkerDestroyed(ServiceWorkerDevToolsAgentHost* host) {}

   protected:
    virtual ~Observer() {}
  };

  class ServiceWorkerIdentifier {
   public:
    ServiceWorkerIdentifier(
        const ServiceWorkerContextCore* context,
        base::WeakPtr<ServiceWorkerContextCore> context_weak,
        int64_t version_id,
        const GURL& url,
        const GURL& scope,
        const base::UnguessableToken& devtools_worker_token);
    ServiceWorkerIdentifier(const ServiceWorkerIdentifier& other);
    ~ServiceWorkerIdentifier();

    bool Matches(const ServiceWorkerIdentifier& other) const;

    const ServiceWorkerContextCore* context() const { return context_; }
    base::WeakPtr<ServiceWorkerContextCore> context_weak() const {
      return context_weak_;
    }
    int64_t version_id() const { return version_id_; }
    GURL url() const { return url_; }
    GURL scope() const { return scope_; }
    const base::UnguessableToken& devtools_worker_token() const {
      return devtools_worker_token_;
    }

   private:
    const ServiceWorkerContextCore* const context_;
    const base::WeakPtr<ServiceWorkerContextCore> context_weak_;
    const int64_t version_id_;
    const GURL url_;
    const GURL scope_;
    const base::UnguessableToken devtools_worker_token_;
  };

  // Returns the ServiceWorkerDevToolsManager singleton.
  static ServiceWorkerDevToolsManager* GetInstance();

  ServiceWorkerDevToolsAgentHost* GetDevToolsAgentHostForWorker(
      int worker_process_id,
      int worker_route_id);
  void AddAllAgentHosts(
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result);
  void AddAllAgentHostsForBrowserContext(
      BrowserContext* browser_context,
      std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>* result);

  // Returns true when the worker must be paused on start because a DevTool
  // window for the same former ServiceWorkerIdentifier is still opened or
  // debug-on-start is enabled in chrome://serviceworker-internals.
  bool WorkerCreated(int worker_process_id,
                     int worker_route_id,
                     const ServiceWorkerIdentifier& service_worker_id,
                     bool is_installed_version);
  void WorkerReadyForInspection(int worker_process_id, int worker_route_id);
  void WorkerVersionInstalled(int worker_process_id, int worker_route_id);
  void WorkerVersionDoomed(int worker_process_id, int worker_route_id);
  void WorkerDestroyed(int worker_process_id, int worker_route_id);
  void NavigationPreloadRequestSent(int worker_process_id,
                                    int worker_route_id,
                                    const std::string& request_id,
                                    const ResourceRequest& request);
  void NavigationPreloadResponseReceived(int worker_process_id,
                                         int worker_route_id,
                                         const std::string& request_id,
                                         const GURL& url,
                                         const ResourceResponseHead& head);
  void NavigationPreloadCompleted(
      int worker_process_id,
      int worker_route_id,
      const std::string& request_id,
      const network::URLLoaderCompletionStatus& status);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void set_debug_service_worker_on_start(bool debug_on_start);
  bool debug_service_worker_on_start() const {
    return debug_service_worker_on_start_;
  }
  void AgentHostDestroyed(ServiceWorkerDevToolsAgentHost* agent_host);

 private:
  friend struct base::DefaultSingletonTraits<ServiceWorkerDevToolsManager>;
  friend class ServiceWorkerDevToolsAgentHost;

  using WorkerId = std::pair<int, int>;

  ServiceWorkerDevToolsManager();
  ~ServiceWorkerDevToolsManager();

  base::ObserverList<Observer> observer_list_;
  bool debug_service_worker_on_start_;

  // We retatin agent hosts as long as the service worker is alive.
  std::map<WorkerId, scoped_refptr<ServiceWorkerDevToolsAgentHost>> live_hosts_;

  // Clients may retain agent host for the terminated shared worker,
  // and we reconnect them when shared worker is restarted.
  base::flat_set<ServiceWorkerDevToolsAgentHost*> terminated_hosts_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsManager);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_MANAGER_H_
