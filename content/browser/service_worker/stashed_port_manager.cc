// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/stashed_port_manager.h"

#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/public/browser/browser_thread.h"

namespace content {

struct StashedPortManager::StashedPort {
  StashedPort();
  ~StashedPort();

  int64 service_worker_registration_id;
  GURL service_worker_origin;

  int message_port_id;
  base::string16 name;

  // The |route_id| to the port in the process this port currently lives in, or
  // MSG_ROUTING_NONE if the port isn't currently associated with any running
  // ServiceWorkerVersion.
  int route_id;
  // The running ServiceWorkerVersion this port is currently associated with.
  // Set to null if the port does not currently exist in a running worker.
  ServiceWorkerVersion* service_worker;
};

StashedPortManager::StashedPort::StashedPort() {
}

StashedPortManager::StashedPort::~StashedPort() {
}

StashedPortManager::StashedPortManager(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
}

StashedPortManager::~StashedPortManager() {
}

void StashedPortManager::Init() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StashedPortManager::InitOnIO, this));
}

void StashedPortManager::Shutdown() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::Bind(&StashedPortManager::ShutdownOnIO, this));
}

void StashedPortManager::InitOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // TODO(mek): Restore ports between service workers from storage.
}

void StashedPortManager::ShutdownOnIO() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (ServiceWorkerVersion* worker : observed_service_workers_) {
    worker->RemoveListener(this);
  }
  observed_service_workers_.clear();
  MessagePortService::GetInstance()->OnMessagePortDelegateClosing(this);
}

void StashedPortManager::AddPort(ServiceWorkerVersion* service_worker,
                                 int message_port_id,
                                 const base::string16& name) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(message_port_id) == ports_.end());

  StashedPort& port = ports_[message_port_id];
  port.service_worker_registration_id = service_worker->registration_id();
  port.service_worker_origin = service_worker->scope().GetOrigin();
  port.message_port_id = message_port_id;
  port.name = name;

  // If service worker is currently running, get its current route id and
  // start observing the worker.
  if (service_worker->running_status() == ServiceWorkerVersion::RUNNING) {
    MessagePortService::GetInstance()->GetMessagePortInfo(
        message_port_id, nullptr, &port.route_id);
    port.service_worker = service_worker;
    if (observed_service_workers_.insert(service_worker).second) {
      // Service Worker wasn't already being observed
      service_worker->AddListener(this);
    }
  } else {
    port.route_id = MSG_ROUTING_NONE;
    port.service_worker = nullptr;
  }

  MessagePortService::GetInstance()->UpdateMessagePort(message_port_id, this,
                                                       message_port_id);

  // If service worker is not currently running, all messages to this port
  // should be held in the browser process.
  if (port.route_id == MSG_ROUTING_NONE)
    MessagePortService::GetInstance()->HoldMessages(message_port_id);
}

void StashedPortManager::SendMessage(
    int message_port_id,
    const MessagePortMessage& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(message_port_id) != ports_.end());

  const StashedPort& port = ports_[message_port_id];
  DCHECK(port.service_worker);
  DCHECK_NE(port.route_id, MSG_ROUTING_NONE);
  port.service_worker->embedded_worker()
      ->message_port_message_filter()
      ->SendMessage(port.route_id, message, sent_message_ports);
}

void StashedPortManager::MessageWasHeld(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(message_port_id) != ports_.end());

  // Messages are queued in the browser process when the stashed port isn't
  // currently available in any running renderer processes. Try to transfer
  // the port to a (potentially newly started) service worker to enable
  // message delivery.
  const StashedPort& port = ports_[message_port_id];
  service_worker_context_->FindRegistrationForId(
      port.service_worker_registration_id, port.service_worker_origin,
      base::Bind(&StashedPortManager::TransferMessagePort, this,
                 message_port_id));
}

void StashedPortManager::SendMessagesAreQueued(int message_port_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(message_port_id) != ports_.end());
  // TODO(mek): Support transfering a stashed message port to a different
  // process.
}

void StashedPortManager::OnRunningStateChanged(
    ServiceWorkerVersion* service_worker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Listener is only added to service workers whose running state is RUNNING,
  // so running state here should only ever be able to be STOPPING or STOPPED.
  DCHECK(service_worker->running_status() == ServiceWorkerVersion::STOPPING ||
         service_worker->running_status() == ServiceWorkerVersion::STOPPED);

  // Hold messages for all ports associated with the no longer running worker.
  for (auto& port : ports_) {
    if (port.second.service_worker != service_worker)
      continue;
    port.second.route_id = MSG_ROUTING_NONE;
    port.second.service_worker = nullptr;
    MessagePortService::GetInstance()->HoldMessages(port.first);
  }
  observed_service_workers_.erase(service_worker);
  service_worker->RemoveListener(this);
}

void StashedPortManager::TransferMessagePort(
    int message_port_id,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  auto it = ports_.find(message_port_id);
  if (it == ports_.end()) {
    // Port no longer owned by StashedPortManager, no need to do anything else.
    return;
  }
  const StashedPort& port = it->second;

  if (service_worker_status != SERVICE_WORKER_OK) {
    // TODO(mek): SW no longer exists, somehow handle this.
    return;
  }

  // TODO(mek): Figure out and spec correct logic for determining which version
  // of a service worker a port should be associated with.
  scoped_refptr<ServiceWorkerVersion> version =
      service_worker_registration->active_version();
  if (!version)
    version = service_worker_registration->waiting_version();
  if (!version)
    version = service_worker_registration->installing_version();
  if (!version) {
    // TODO(mek): Do something when no version is found.
    return;
  }

  std::vector<TransferredMessagePort> ports(1);
  std::vector<base::string16> port_names(1);
  ports[0].id = port.message_port_id;
  port_names[0] = port.name;
  version->SendStashedMessagePorts(
      ports, port_names, base::Bind(&StashedPortManager::OnPortsTransferred,
                                    this, version, ports));
}

void StashedPortManager::OnPortsTransferred(
    const scoped_refptr<ServiceWorkerVersion>& service_worker,
    const std::vector<TransferredMessagePort>& ports,
    ServiceWorkerStatusCode service_worker_status,
    const std::vector<int>& route_ids) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (service_worker_status != SERVICE_WORKER_OK) {
    // TODO(mek): Handle failure.
    return;
  }

  if (service_worker->running_status() != ServiceWorkerVersion::RUNNING) {
    // TODO(mek): Figure out what to do in this case.
    return;
  }

  // Port was transfered to a service worker, start observing the worker so
  // messages can be held when the worker stops running.
  if (observed_service_workers_.insert(service_worker.get()).second) {
    // Service Worker wasn't already being observed
    service_worker->AddListener(this);
  }

  // Update ports with the new route ids and service worker version.
  DCHECK_EQ(ports.size(), route_ids.size());
  for (size_t i = 0; i < ports.size(); ++i) {
    DCHECK(ports_.find(ports[i].id) != ports_.end());
    StashedPort& port = ports_[ports[i].id];
    port.route_id = route_ids[i];
    port.service_worker = service_worker.get();
  }
}

}  // namespace content
