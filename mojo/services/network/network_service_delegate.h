// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_
#define MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread.h"
#include "mojo/services/network/network_context.h"
#include "mojo/services/network/public/interfaces/cookie_store.mojom.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/url_loader_factory.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket_factory.mojom.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/message_loop_ref.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
class NetworkServiceDelegateObserver;

class NetworkServiceDelegate : public ShellClient,
                               public InterfaceFactory<NetworkService>,
                               public InterfaceFactory<CookieStore>,
                               public InterfaceFactory<WebSocketFactory>,
                               public InterfaceFactory<URLLoaderFactory> {
 public:
  NetworkServiceDelegate();
  ~NetworkServiceDelegate() override;

  void AddObserver(NetworkServiceDelegateObserver* observer);
  void RemoveObserver(NetworkServiceDelegateObserver* observer);

 private:
  // mojo::ShellClient implementation.
  void Initialize(Connector* connector, const Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(Connection* connection) override;

  // InterfaceFactory<NetworkService> implementation.
  void Create(Connection* connection,
              InterfaceRequest<NetworkService> request) override;

  // InterfaceFactory<CookieStore> implementation.
  void Create(Connection* connection,
              InterfaceRequest<CookieStore> request) override;

  // InterfaceFactory<WebSocketFactory> implementation.
  void Create(Connection* connection,
              InterfaceRequest<WebSocketFactory> request) override;

  // InterfaceFactory<URLLoaderFactory> implementation.
  void Create(Connection* connection,
              InterfaceRequest<URLLoaderFactory> request) override;

 private:
  void Quit();

  mojo::TracingImpl tracing_;

  // Observers that want notifications that our worker thread is going away.
  base::ObserverList<NetworkServiceDelegateObserver> observers_;

  scoped_ptr<NetworkContext> context_;

  mojo::MessageLoopRefFactory ref_factory_;

  DISALLOW_COPY_AND_ASSIGN(NetworkServiceDelegate);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_NETWORK_NETWORK_SERVICE_DELEGATE_H_
