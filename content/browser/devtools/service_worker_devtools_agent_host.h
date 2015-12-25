// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_

#include <stdint.h>

#include <map>

#include "base/macros.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/devtools/worker_devtools_agent_host.h"

namespace content {

class ServiceWorkerDevToolsAgentHost : public WorkerDevToolsAgentHost {
 public:
  using List = std::vector<scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using Map = std::map<std::string,
                       scoped_refptr<ServiceWorkerDevToolsAgentHost>>;
  using ServiceWorkerIdentifier =
      ServiceWorkerDevToolsManager::ServiceWorkerIdentifier;

  ServiceWorkerDevToolsAgentHost(WorkerId worker_id,
                                 const ServiceWorkerIdentifier& service_worker);

  void UnregisterWorker();

  // DevToolsAgentHost overrides.
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;

  // WorkerDevToolsAgentHost overrides.
  void OnAttachedStateChanged(bool attached) override;

  int64_t service_worker_version_id() const;

  bool Matches(const ServiceWorkerIdentifier& other);

 private:
  ~ServiceWorkerDevToolsAgentHost() override;
  scoped_ptr<ServiceWorkerIdentifier> service_worker_;
  scoped_ptr<devtools::network::NetworkHandler> network_handler_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
