// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_
#define CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/controller_service_worker.mojom.h"

namespace content {

namespace mojom {
class ServiceWorkerContainerHost;
}  // namespace mojom

// Vends a connection to the controller service worker for a given
// ServiceWorkerContainerHost. This is co-owned by
// ServiceWorkerProviderContext::ControlleeState and
// ServiceWorkerSubresourceLoader{,Factory}.
class CONTENT_EXPORT ControllerServiceWorkerConnector
    : public base::RefCounted<ControllerServiceWorkerConnector> {
 public:
  explicit ControllerServiceWorkerConnector(
      mojom::ServiceWorkerContainerHost* container_host);

  // This may return nullptr if the connection to the ContainerHost (in the
  // browser process) is already terminated.
  mojom::ControllerServiceWorker* GetControllerServiceWorker();

  void OnContainerHostConnectionClosed();

 private:
  friend class base::RefCounted<ControllerServiceWorkerConnector>;
  ~ControllerServiceWorkerConnector();

  void OnControllerConnectionClosed();

  // Connection to the ServiceWorkerProviderHost that lives in the
  // browser process. This is used to (re-)obtain Mojo connection to
  // |controller_service_worker_| when it is not established.
  // Cleared when the connection is dropped.
  mojom::ServiceWorkerContainerHost* container_host_;

  // Connection to the ControllerServiceWorker. The consumer of this connection
  // should not need to know which process this is connected to.
  // (Currently this is connected to BrowserSideControllerServiceWorker,
  // but will eventually be directly connected to the controller service worker
  // in the renderer process)
  mojom::ControllerServiceWorkerPtr controller_service_worker_;

  DISALLOW_COPY_AND_ASSIGN(ControllerServiceWorkerConnector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_
