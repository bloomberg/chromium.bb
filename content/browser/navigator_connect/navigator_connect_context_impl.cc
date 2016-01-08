// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_context_impl.h"

#include <stdint.h>

#include "base/stl_util.h"
#include "content/browser/message_port_service.h"
#include "content/browser/navigator_connect/service_port_service_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/navigator_connect_service_factory.h"
#include "content/public/common/navigator_connect_client.h"
#include "mojo/common/url_type_converters.h"

namespace content {

struct NavigatorConnectContextImpl::Port {
  // ID of this port.
  int id;
  // ID of the port this port is connected to.
  int entangled_id;

  // Service url and client origin describing this connection. These fields will
  // always be the same as the same fields for the entangled port.
  GURL target_url;
  GURL client_origin;

  // Set to nullptr when the ServicePortService goes away.
  ServicePortServiceImpl* service = nullptr;

  // If this port is associated with a service worker, these fields store that
  // information.
  int64_t service_worker_registration_id = kInvalidServiceWorkerRegistrationId;
  GURL service_worker_registration_origin;
};

NavigatorConnectContextImpl::NavigatorConnectContextImpl(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context), next_port_id_(0) {}

NavigatorConnectContextImpl::~NavigatorConnectContextImpl() {
}

void NavigatorConnectContextImpl::AddFactory(
    scoped_ptr<NavigatorConnectServiceFactory> factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&NavigatorConnectContextImpl::AddFactoryOnIOThread, this,
                 base::Passed(&factory)));
}

void NavigatorConnectContextImpl::AddFactoryOnIOThread(
    scoped_ptr<NavigatorConnectServiceFactory> factory) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  service_factories_.push_back(factory.release());
}

void NavigatorConnectContextImpl::Connect(
    const GURL& target_url,
    const GURL& origin,
    ServicePortServiceImpl* service_port_service,
    const ConnectCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Create a new message channel.
  int client_port_id = next_port_id_++;
  int service_port_id = next_port_id_++;

  Port& client_port = ports_[client_port_id];
  client_port.id = client_port_id;
  client_port.entangled_id = service_port_id;
  client_port.target_url = target_url;
  client_port.client_origin = origin;
  client_port.service = service_port_service;

  Port& service_port = ports_[service_port_id];
  service_port.id = service_port_id;
  service_port.entangled_id = client_port_id;
  service_port.target_url = target_url;
  service_port.client_origin = origin;

  // Find the right service worker to service this connection.
  service_worker_context_->FindReadyRegistrationForDocument(
      target_url,
      base::Bind(&NavigatorConnectContextImpl::GotServiceWorkerRegistration,
                 this, callback, client_port_id, service_port_id));
}

void NavigatorConnectContextImpl::PostMessage(
    int sender_port_id,
    const MessagePortMessage& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(sender_port_id) != ports_.end());
  DCHECK(message.message_as_value.empty());

  const Port& sender_port = ports_[sender_port_id];
  DCHECK(ports_.find(sender_port.entangled_id) != ports_.end());
  const Port& port = ports_[sender_port.entangled_id];

  if (port.service_worker_registration_id !=
      kInvalidServiceWorkerRegistrationId) {
    // Port is associated with service worker, dispatch message event via
    // ServiceWorkerVersion.

    // Hold messages on transferred message ports. Actual delivery of the
    // message by the service can be asynchronous. When a message is delivered,
    // WebMessagePortChannelImpl instances will be constructed which send
    // MessagePortHostMsg_ReleaseMessages to release messages.
    for (const auto& sent_port : sent_message_ports)
      MessagePortService::GetInstance()->HoldMessages(sent_port.id);

    service_worker_context_->FindReadyRegistrationForId(
        port.service_worker_registration_id,
        port.service_worker_registration_origin,
        base::Bind(&NavigatorConnectContextImpl::DeliverMessage, this, port.id,
                   message.message_as_string, sent_message_ports));
    return;
  }

  if (!port.service) {
    // TODO(mek): Figure out what to do in this situation.
    return;
  }
  port.service->PostMessageToClient(port.id, message, sent_message_ports);
}

void NavigatorConnectContextImpl::GotServiceWorkerRegistration(
    const ConnectCallback& callback,
    int client_port_id,
    int service_port_id,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(client_port_id) != ports_.end());
  DCHECK(ports_.find(service_port_id) != ports_.end());

  if (status != SERVICE_WORKER_OK) {
    // No service worker found, reject connection attempt.
    OnConnectError(callback, client_port_id, service_port_id, status);
    return;
  }

  ServiceWorkerVersion* active_version = registration->active_version();
  DCHECK(active_version);

  Port& service_port = ports_[service_port_id];
  service_port.service_worker_registration_id = registration->id();
  service_port.service_worker_registration_origin =
      registration->pattern().GetOrigin();

  active_version->RunAfterStartWorker(
      base::Bind(&NavigatorConnectContextImpl::OnConnectError, this, callback,
                 client_port_id, service_port_id),
      base::Bind(&NavigatorConnectContextImpl::DispatchConnectEvent, this,
                 callback, client_port_id, service_port_id, registration,
                 make_scoped_refptr(active_version)));
}

void NavigatorConnectContextImpl::DispatchConnectEvent(
    const ConnectCallback& callback,
    int client_port_id,
    int service_port_id,
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    const scoped_refptr<ServiceWorkerVersion>& worker) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ContainsKey(ports_, client_port_id));
  DCHECK(ContainsKey(ports_, service_port_id));

  const Port& service_port = ports_[service_port_id];
  int request_id = worker->StartRequest(
      ServiceWorkerMetrics::EventType::SERVICE_PORT_CONNECT,
      base::Bind(&NavigatorConnectContextImpl::OnConnectError, this, callback,
                 client_port_id, service_port_id));
  base::WeakPtr<ServicePortDispatcher> dispatcher =
      worker->GetMojoServiceForRequest<ServicePortDispatcher>(request_id);
  dispatcher->Connect(
      mojo::String::From(service_port.target_url),
      mojo::String::From(service_port.client_origin), service_port_id,
      base::Bind(&NavigatorConnectContextImpl::OnConnectResult, this, callback,
                 client_port_id, service_port_id, service_worker_registration,
                 worker, request_id));
}

void NavigatorConnectContextImpl::ServicePortServiceDestroyed(
    ServicePortServiceImpl* service_port_service) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  for (auto& port : ports_) {
    if (port.second.service != service_port_service)
      continue;
    port.second.service = nullptr;
    // TODO(mek): Should actually inform other side of connections that the
    // connection was closed, or in the case of service workers somehow keep
    // track of the connection.
  }
}

void NavigatorConnectContextImpl::OnConnectError(
    const ConnectCallback& callback,
    int client_port_id,
    int service_port_id,
    ServiceWorkerStatusCode status) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  // Destroy ports since connection failed.
  ports_.erase(service_port_id);
  ports_.erase(client_port_id);
  callback.Run(MSG_ROUTING_NONE, false);
}

void NavigatorConnectContextImpl::OnConnectResult(
    const ConnectCallback& callback,
    int client_port_id,
    int service_port_id,
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    const scoped_refptr<ServiceWorkerVersion>& worker,
    int request_id,
    ServicePortConnectResult result,
    const mojo::String& name,
    const mojo::String& data) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (!worker->FinishRequest(request_id))
    return;

  if (result != SERVICE_PORT_CONNECT_RESULT_ACCEPT) {
    OnConnectError(callback, client_port_id, service_port_id,
                   SERVICE_WORKER_ERROR_FAILED);
    return;
  }

  // TODO(mek): Might have to do something else if the client connection got
  // severed while the service side connection was being set up.
  callback.Run(client_port_id, true);
}

void NavigatorConnectContextImpl::DeliverMessage(
    int port_id,
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(ports_.find(port_id) != ports_.end());

  if (service_worker_status != SERVICE_WORKER_OK) {
    // TODO(mek): Do something when no service worker was found.
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  DCHECK(active_version);

  const Port& port = ports_[port_id];
  NavigatorConnectClient client(port.target_url, port.client_origin, port_id);
  active_version->DispatchCrossOriginMessageEvent(
      client, message, sent_message_ports,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace content
