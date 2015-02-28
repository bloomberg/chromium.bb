// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_service_worker_service_factory.h"

#include "base/bind.h"
#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/message_port_delegate.h"
#include "content/public/common/navigator_connect_client.h"

namespace content {

namespace {

// MessagePortDelegate implementation that directs all messages to the
// oncrossoriginmessage event of a service worker.
// TODO(mek): Somehow clean up message ports when a service worker is
// unregistered.
class NavigatorConnectServiceWorkerService : public MessagePortDelegate {
 public:
  NavigatorConnectServiceWorkerService(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
      const NavigatorConnectClient& client,
      const scoped_refptr<ServiceWorkerRegistration>&
          service_worker_registration);
  ~NavigatorConnectServiceWorkerService() override;

  // MessagePortDelegate implementation.
  void SendMessage(
      int route_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports) override;
  void SendMessagesAreQueued(int route_id) override;

 private:
  // Callback called by SendMessage when the ServiceWorkerRegistration for this
  // service has been located.
  void DeliverMessage(
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      ServiceWorkerStatusCode service_worker_status,
      const scoped_refptr<ServiceWorkerRegistration>&
          service_worker_registration);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;
  NavigatorConnectClient client_;
  int64 service_worker_registration_id_;
  GURL service_worker_registration_origin_;

  base::WeakPtrFactory<NavigatorConnectServiceWorkerService> weak_factory_;
};

NavigatorConnectServiceWorkerService::NavigatorConnectServiceWorkerService(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context,
    const NavigatorConnectClient& client,
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration)
    : service_worker_context_(service_worker_context),
      client_(client),
      service_worker_registration_id_(service_worker_registration->id()),
      service_worker_registration_origin_(
          service_worker_registration->pattern().GetOrigin()),
      weak_factory_(this) {
}

NavigatorConnectServiceWorkerService::~NavigatorConnectServiceWorkerService() {
}

void NavigatorConnectServiceWorkerService::SendMessage(
    int route_id,
    const MessagePortMessage& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  DCHECK(route_id == client_.message_port_id);
  DCHECK(message.message_as_value.empty());

  // Hold messages on transferred message ports. Actual delivery of the message
  // by the service can be asynchronous. When a message is delivered,
  // WebMessagePortChannelImpl instances will be constructed which send
  // MessagePortHostMsg_ReleaseMessages to release messages.
  for (const auto& port : sent_message_ports)
    MessagePortService::GetInstance()->HoldMessages(port.id);

  service_worker_context_->context()->storage()->FindRegistrationForId(
      service_worker_registration_id_, service_worker_registration_origin_,
      base::Bind(&NavigatorConnectServiceWorkerService::DeliverMessage,
                 weak_factory_.GetWeakPtr(), message.message_as_string,
                 sent_message_ports));
}

void NavigatorConnectServiceWorkerService::SendMessagesAreQueued(int route_id) {
  NOTREACHED() << "navigator.connect endpoints should never queue messages.";
}

void NavigatorConnectServiceWorkerService::DeliverMessage(
    const base::string16& message,
    const std::vector<TransferredMessagePort>& sent_message_ports,
    ServiceWorkerStatusCode service_worker_status,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  if (service_worker_status != SERVICE_WORKER_OK) {
    // TODO(mek): Do something when no service worker was found.
    return;
  }

  ServiceWorkerVersion* active_version =
      service_worker_registration->active_version();
  if (!active_version) {
    // TODO(mek): Do something when no active version exists.
    return;
  }

  active_version->DispatchCrossOriginMessageEvent(
      client_, message, sent_message_ports,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace

NavigatorConnectServiceWorkerServiceFactory::
    NavigatorConnectServiceWorkerServiceFactory(const scoped_refptr<
        ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context), weak_factory_(this) {
}

NavigatorConnectServiceWorkerServiceFactory::
    ~NavigatorConnectServiceWorkerServiceFactory() {
}

bool NavigatorConnectServiceWorkerServiceFactory::HandlesUrl(
    const GURL& target_url) {
  // Always return true, all URLs could potentially have a service worker, and
  // this factory will be installed as first factory, so it will only be used
  // if no other factory claims to handle the url.
  return true;
}

void NavigatorConnectServiceWorkerServiceFactory::Connect(
    const NavigatorConnectClient& client,
    const ConnectCallback& callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  // Find the right service worker to service this connection.
  service_worker_context_->context()->storage()->FindRegistrationForDocument(
      client.target_url,
      base::Bind(&NavigatorConnectServiceWorkerServiceFactory::
                     GotServiceWorkerRegistration,
                 weak_factory_.GetWeakPtr(), callback, client));
}

void NavigatorConnectServiceWorkerServiceFactory::GotServiceWorkerRegistration(
    const ConnectCallback& callback,
    const NavigatorConnectClient& client,
    ServiceWorkerStatusCode status,
    const scoped_refptr<ServiceWorkerRegistration>& registration) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK) {
    // No service worker found, reject connection attempt.
    OnConnectResult(callback, client, registration, status, false);
    return;
  }

  ServiceWorkerVersion* active_version = registration->active_version();
  if (!active_version) {
    // No active version, reject connection attempt.
    OnConnectResult(callback, client, registration, status, false);
    return;
  }

  active_version->DispatchCrossOriginConnectEvent(
      base::Bind(&NavigatorConnectServiceWorkerServiceFactory::OnConnectResult,
                 weak_factory_.GetWeakPtr(), callback, client, registration),
      client);
}

void NavigatorConnectServiceWorkerServiceFactory::OnConnectResult(
    const ConnectCallback& callback,
    const NavigatorConnectClient& client,
    const scoped_refptr<ServiceWorkerRegistration>& service_worker_registration,
    ServiceWorkerStatusCode status,
    bool accept_connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (status != SERVICE_WORKER_OK || !accept_connection) {
    callback.Run(nullptr);
    return;
  }

  // TODO(mek): Keep track of NavigatorConnectServiceWorkerService instances and
  // clean them up when a service worker registration is deleted.
  callback.Run(new NavigatorConnectServiceWorkerService(
      service_worker_context_, client, service_worker_registration));
}

}  // namespace content
