// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
#define CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_

#include "content/browser/devtools/embedded_worker_devtools_agent_host.h"

namespace content {

class ServiceWorkerDevToolsAgentHost : public EmbeddedWorkerDevToolsAgentHost {
 public:
  ServiceWorkerDevToolsAgentHost(WorkerId worker_id,
                                 const ServiceWorkerIdentifier& service_worker,
                                 bool debug_service_worker_on_start);

  // DevToolsAgentHost override.
  Type GetType() override;
  std::string GetTitle() override;
  GURL GetURL() override;
  bool Activate() override;
  bool Close() override;

  // IPCDevToolsAgentHost override.
  void OnClientAttached() override;
  void OnClientDetached() override;

  // EmbeddedWorkerDevToolsAgentHost override.
  bool Matches(const ServiceWorkerIdentifier& other) override;

 private:
  ~ServiceWorkerDevToolsAgentHost() override;
  scoped_ptr<ServiceWorkerIdentifier> service_worker_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerDevToolsAgentHost);
};

}  // namespace content

#endif  // CONTENT_BROWSER_DEVTOOLS_SERVICE_WORKER_DEVTOOLS_AGENT_HOST_H_
