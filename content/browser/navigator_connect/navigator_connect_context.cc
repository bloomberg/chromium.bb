// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_context.h"

#include "content/browser/message_port_service.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/browser/service_worker/service_worker_utils.h"
#include "content/common/navigator_connect_types.h"

namespace content {

struct NavigatorConnectContext::Connection {
  CrossOriginServiceWorkerClient client;
  int64 service_worker_registration_id;
  GURL service_worker_registration_origin;
};

NavigatorConnectContext::NavigatorConnectContext(
    const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context)
    : service_worker_context_(service_worker_context) {
}

NavigatorConnectContext::~NavigatorConnectContext() {
}

void NavigatorConnectContext::RegisterConnection(
    const CrossOriginServiceWorkerClient& client,
    const scoped_refptr<ServiceWorkerRegistration>&
        service_worker_registration) {
  MessagePortService::GetInstance()->UpdateMessagePort(
      client.message_port_id, this, client.message_port_id);
  MessagePortService::GetInstance()->ReleaseMessages(client.message_port_id);
  Connection& connection = connections_[client.message_port_id];
  connection.client = client;
  connection.service_worker_registration_id = service_worker_registration->id();
  connection.service_worker_registration_origin =
      service_worker_registration->pattern().GetOrigin();
}

void NavigatorConnectContext::SendMessage(
    int route_id,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids) {
  DCHECK(connections_.find(route_id) != connections_.end());
  const Connection& connection = connections_[route_id];

  // Hold messages while service worker is found, activated, and message sent
  // causing ServiceWorkerScriptContext::OnCrossOriginMessageToWorker to
  // construct WebMessagePortChannelImpl instances which send
  // MessagePortHostMsg_ReleaseMessages.
  for (int sent_message_port_id : sent_message_port_ids)
    MessagePortService::GetInstance()->HoldMessages(sent_message_port_id);

  service_worker_context_->context()->storage()->FindRegistrationForId(
      connection.service_worker_registration_id,
      connection.service_worker_registration_origin,
      base::Bind(&NavigatorConnectContext::DoSendMessage, this,
                 connection.client, message, sent_message_port_ids));
}

void NavigatorConnectContext::SendMessagesAreQueued(int route_id) {
  NOTREACHED() << "navigator.connect endpoints should never queue messages.";
}

void NavigatorConnectContext::DoSendMessage(
    const CrossOriginServiceWorkerClient& client,
    const base::string16& message,
    const std::vector<int>& sent_message_port_ids,
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
      client, message, sent_message_port_ids,
      base::Bind(&ServiceWorkerUtils::NoOpStatusCallback));
}

}  // namespace content
