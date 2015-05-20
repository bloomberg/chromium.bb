// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_SERVICE_WORKER_STASHED_PORT_MANAGER_H_
#define CONTENT_BROWSER_SERVICE_WORKER_STASHED_PORT_MANAGER_H_

#include <map>
#include <set>

#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/message_port_delegate.h"

namespace content {

class ServiceWorkerContextWrapper;

// This class keeps track of all stashed message ports. Stashed message ports
// are ports that will survive a service worker being shut down. This class
// registers itself with the MessagePortService as delegate for all these ports
// and is responsible for starting the right service worker when messages are
// sent to a stashed port.
// This class is created on the UI thread, but all its methods should only be
// called from the IO thread.
// TODO(mek): Cleanup ports when service worker registration is unregistered.
// TODO(mek): special handle channels where both ports are stashed. These need
// to be persisted to disk and restored later.
// TODO(mek): Make sure messages are delivered to the correct version of a
// service worker, once it's properly specced what the actual desired behavior
// is.
// TODO(mek): Handle stashing an already stashed port.
// TODO(mek): Make sure transfering a stashed port unregisters it from here.
class StashedPortManager
    : public MessagePortDelegate,
      public ServiceWorkerVersion::Listener,
      public base::RefCountedThreadSafe<StashedPortManager> {
 public:
  explicit StashedPortManager(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // Init and Shutdown are for use on the UI thread when the storagepartition is
  // being setup and torn down.
  void Init();
  void Shutdown();

  // Add a new port to be stashed. This updates the MessagePortService with the
  // new delegate for the port, and makes sure messages are queued when the
  // |service_worker| isn't running.
  void AddPort(ServiceWorkerVersion* service_worker,
               int message_port_id,
               const base::string16& name);

  // MessagePortDelegate implementation.
  void SendMessage(
      int route_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports) override;
  void MessageWasHeld(int route_id) override;
  void SendMessagesAreQueued(int route_id) override;

  // ServiceWorkerVersion::Listener implementation:
  void OnRunningStateChanged(ServiceWorkerVersion* service_worker) override;

 private:
  friend class base::RefCountedThreadSafe<StashedPortManager>;
  ~StashedPortManager() override;

  void InitOnIO();
  void ShutdownOnIO();

  // Helper method that transfers a message port to a specific service worker.
  // This is called when messages are sent to a port held by a service worker
  // that isn't currently running.
  void TransferMessagePort(int message_port_id,
                           ServiceWorkerStatusCode service_worker_status,
                           const scoped_refptr<ServiceWorkerRegistration>&
                               service_worker_registration);

  // Helper method called when transfering of ports either succeeded or failed.
  // Updates internal bookkeeping with the new status and route ids of these
  // ports.
  void OnPortsTransferred(
      const scoped_refptr<ServiceWorkerVersion>& service_worker,
      const std::vector<TransferredMessagePort>& ports,
      ServiceWorkerStatusCode service_worker_status,
      const std::vector<int>& route_ids);

  // Internal bookkeeping for each stashed port.
  struct StashedPort;

  // Maps message_port_id to StashedPort instances.
  std::map<int, StashedPort> ports_;

  // Set of all service worker versions that are currently being observed.
  std::set<ServiceWorkerVersion*> observed_service_workers_;

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_SERVICE_WORKER_STASHED_PORT_MANAGER_H_
