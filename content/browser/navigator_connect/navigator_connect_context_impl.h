// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_
#define CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_

#include <map>
#include "base/callback_forward.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "content/common/service_port_service.mojom.h"
#include "content/common/service_worker/service_worker_status_code.h"
#include "content/public/browser/navigator_connect_context.h"

class GURL;

namespace content {

struct MessagePortMessage;
class NavigatorConnectService;
class NavigatorConnectServiceFactory;
struct NavigatorConnectClient;
class ServicePortServiceImpl;
class ServiceWorkerContextWrapper;
class ServiceWorkerRegistration;
class ServiceWorkerVersion;
struct TransferredMessagePort;

// Tracks all active navigator.services connections, as well as available
// service factories. Delegates connection requests to the correct factory and
// passes messages on to the correct service.
// One instance of this class exists per StoragePartition.
// TODO(mek): Clean up connections, fire of closed events when connections die.
// TODO(mek): Update service side API to be fully ServicePort based.
// TODO(mek): Make ServicePorts that live in a service worker be able to survive
// the worker being restarted.
// TODO(mek): Add back ability for service ports to be backed by native code.
class NavigatorConnectContextImpl : public NavigatorConnectContext {
 public:
  using ConnectCallback =
      base::Callback<void(int message_port_id, bool success)>;

  explicit NavigatorConnectContextImpl(
      const scoped_refptr<ServiceWorkerContextWrapper>& service_worker_context);

  // Called when a new connection request comes in from a client. Finds the
  // correct service factory and passes the connection request off to there.
  // Can call the callback before this method call returns.
  void Connect(const GURL& target_url,
               const GURL& origin,
               ServicePortServiceImpl* service_port_service,
               const ConnectCallback& callback);

  // Called when a message is sent to a ServicePort. The |sender_port_id| is the
  // id of the port the message is sent from, this will look up what other port
  // the port is entangled with and deliver the message to that port.
  void PostMessage(
      int sender_port_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);

  // Called by a ServicePortServiceImpl instance when it is about to be
  // destroyed to inform this class that all its connections are no longer
  // valid.
  void ServicePortServiceDestroyed(
      ServicePortServiceImpl* service_port_service);

  // NavigatorConnectContext implementation.
  void AddFactory(scoped_ptr<NavigatorConnectServiceFactory> factory) override;

 private:
  ~NavigatorConnectContextImpl() override;

  void AddFactoryOnIOThread(scoped_ptr<NavigatorConnectServiceFactory> factory);

  // Callback called when a ServiceWorkerRegistration has been located (or
  // has failed to be located) for a connection attempt.
  void GotServiceWorkerRegistration(
      const ConnectCallback& callback,
      int client_port_id,
      int service_port_id,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  void DispatchConnectEvent(const ConnectCallback& callback,
                            int client_port_id,
                            int service_port_id,
                            const scoped_refptr<ServiceWorkerRegistration>&
                                service_worker_registration,
                            const scoped_refptr<ServiceWorkerVersion>& worker);

  // Callback called when dispatching a connect event failed.
  void OnConnectError(const ConnectCallback& calback,
                      int client_port_id,
                      int service_port_id,
                      ServiceWorkerStatusCode status);

  // Callback called with the response to a connect event.
  void OnConnectResult(const ConnectCallback& callback,
                       int client_port_id,
                       int service_port_id,
                       const scoped_refptr<ServiceWorkerRegistration>&
                           service_worker_registration,
                       const scoped_refptr<ServiceWorkerVersion>& worker,
                       int request_id,
                       ServicePortConnectResult result,
                       const mojo::String& name,
                       const mojo::String& data);

  // Callback called when a ServiceWorkerRegistration has been located to
  // deliver a message to.
  void DeliverMessage(
      int port_id,
      const base::string16& message,
      const std::vector<TransferredMessagePort>& sent_message_ports,
      ServiceWorkerStatusCode status,
      const scoped_refptr<ServiceWorkerRegistration>& registration);

  scoped_refptr<ServiceWorkerContextWrapper> service_worker_context_;

  // List of factories to try to handle URLs.
  ScopedVector<NavigatorConnectServiceFactory> service_factories_;

  // List of currently active ServicePorts.
  struct Port;
  std::map<int, Port> ports_;
  int next_port_id_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_NAVIGATOR_CONNECT_CONTEXT_IMPL_H_
