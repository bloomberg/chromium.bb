// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_
#define CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "content/common/content_export.h"
#include "content/common/service_worker/controller_service_worker.mojom.h"
#include "content/common/service_worker/service_worker_container.mojom.h"

namespace content {

namespace mojom {
class ServiceWorkerContainerHost;
}  // namespace mojom

// Vends a connection to the controller service worker for a given
// ServiceWorkerContainerHost. This is owned by
// ServiceWorkerSubresourceLoaderFactory.
class CONTENT_EXPORT ControllerServiceWorkerConnector {
 public:
  // Observes the connection to the controller.
  class Observer {
   public:
    virtual void OnConnectionClosed() = 0;
  };

  enum class State {
    // The controller connection is dropped. Calling
    // GetControllerServiceWorker() in this state will result in trying to
    // get the new controller pointer from the browser.
    kDisconnected,

    // The controller connection is established.
    kConnected,

    // It is notified that the client lost the controller. This could only
    // happen due to an exceptional condition like the service worker could
    // no longer be read from the script cache. Calling
    // GetControllerServiceWorker() in this state will always return nullptr.
    kNoController,

    // The container host is shutting down. Calling
    // GetControllerServiceWorker() in this state will always return nullptr.
    kNoContainerHost,
  };

  // This should only be called if a controller exists for the client.
  // |this| is initialized on |task_runner|, and therefore must be
  // used only on |task_runner|.
  // |controller_info| can be null if the caller does not have the Mojo ptr
  // to the controller. In that case |controller_service_worker_| is populated
  // by talking to |controller_host_ptr_|.
  ControllerServiceWorkerConnector(
      mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
      mojom::ControllerServiceWorkerPtrInfo controller_info,
      const std::string& client_id,
      scoped_refptr<base::SequencedTaskRunner> task_runner);
  ~ControllerServiceWorkerConnector();

  // This may return nullptr if the connection to the ContainerHost (in the
  // browser process) is already terminated.
  mojom::ControllerServiceWorker* GetControllerServiceWorker(
      mojom::ControllerServiceWorkerPurpose purpose);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void OnContainerHostConnectionClosed();
  void OnControllerConnectionClosed();

  void UpdateController(mojom::ControllerServiceWorkerPtrInfo controller_info);

  State state() const { return state_; }

  const std::string& client_id() const { return client_id_; }

  base::WeakPtr<ControllerServiceWorkerConnector> GetWeakPtr() {
    return weak_factory_.GetWeakPtr();
  }

 private:
  void InitializeOnTaskRunner(
      mojom::ServiceWorkerContainerHostPtrInfo container_host_info,
      mojom::ControllerServiceWorkerPtrInfo controller_info);
  void SetControllerServiceWorkerPtr(
      mojom::ControllerServiceWorkerPtr controller_ptr);

  State state_ = State::kDisconnected;

  // Keeps the mojo end to the browser process on its own.
  // Non-null only for the service worker clients that are workers (i.e., only
  // when created for dedicated workers or shared workers).
  mojom::ServiceWorkerContainerHostPtr container_host_ptr_;

  // Connection to the ControllerServiceWorker. The consumer of this connection
  // should not need to know which process this is connected to.
  // (Currently this is connected to BrowserSideControllerServiceWorker,
  // but will eventually be directly connected to the controller service worker
  // in the renderer process)
  mojom::ControllerServiceWorkerPtr controller_service_worker_;

  base::ObserverList<Observer> observer_list_;

  // The web-exposed client id, used for FetchEvent#clientId (i.e.,
  // ServiceWorkerProviderHost::client_uuid and not
  // ServiceWorkerProviderHost::provider_id).
  std::string client_id_;

  base::WeakPtrFactory<ControllerServiceWorkerConnector> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ControllerServiceWorkerConnector);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SERVICE_WORKER_CONTROLLER_SERVICE_WORKER_CONNECTOR_H_
