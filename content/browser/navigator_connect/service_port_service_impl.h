// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_NAVIGATOR_CONNECT_SERVICE_PORT_SERVICE_IMPL_H_
#define CONTENT_BROWSER_NAVIGATOR_CONNECT_SERVICE_PORT_SERVICE_IMPL_H_

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "content/common/service_port_service.mojom.h"
#include "mojo/public/cpp/bindings/strong_binding.h"

namespace content {
struct MessagePortMessage;
class MessagePortMessageFilter;
class NavigatorConnectContextImpl;
struct TransferredMessagePort;

// Browser side class representing a ServicePortCollection in the renderer. One
// instance of this class is created for each ServicePortCollection, and is
// automatically destroyed when the ServicePortCollection in the renderer goes
// away.
class ServicePortServiceImpl : public ServicePortService {
 public:
  // Factory method called by mojo to create a new instance of this class to
  // handle requests from a new ServicePortCollection.
  static void Create(const scoped_refptr<NavigatorConnectContextImpl>&
                         navigator_connect_context,
                     const scoped_refptr<MessagePortMessageFilter>&
                         message_port_message_filter,
                     mojo::InterfaceRequest<ServicePortService> request);
  ~ServicePortServiceImpl() override;

  // Called by NavigatorConnectContextImpl to post a message to a port that is
  // owned by this ServicePortCollection.
  void PostMessageToClient(
      int port_id,
      const MessagePortMessage& message,
      const std::vector<TransferredMessagePort>& sent_message_ports);

 private:
  static void CreateOnIOThread(
      const scoped_refptr<NavigatorConnectContextImpl>&
          navigator_connect_context,
      const scoped_refptr<MessagePortMessageFilter>&
          message_port_message_filter,
      mojo::InterfaceRequest<ServicePortService> request);
  ServicePortServiceImpl(const scoped_refptr<NavigatorConnectContextImpl>&
                             navigator_connect_context,
                         const scoped_refptr<MessagePortMessageFilter>&
                             message_port_message_filter,
                         mojo::InterfaceRequest<ServicePortService> request);

  // ServicePortService methods:
  void SetClient(ServicePortServiceClientPtr client) override;
  void Connect(const mojo::String& target_url,
               const mojo::String& origin,
               const ConnectCallback& callback) override;
  void PostMessageToPort(
      int32_t port_id,
      const mojo::String& message,
      mojo::Array<MojoTransferredMessagePortPtr> ports) override;
  void ClosePort(int32_t port_id) override;

  // Callback called when a connection to a service has been establised or
  // rejected.
  void OnConnectResult(const ConnectCallback& callback,
                       int message_port_id,
                       bool success);

  mojo::StrongBinding<ServicePortService> binding_;
  scoped_refptr<NavigatorConnectContextImpl> navigator_connect_context_;
  scoped_refptr<MessagePortMessageFilter> message_port_message_filter_;
  ServicePortServiceClientPtr client_;
  base::WeakPtrFactory<ServicePortServiceImpl> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ServicePortServiceImpl);
};

}  // namespace content

#endif  // CONTENT_BROWSER_NAVIGATOR_CONNECT_SERVICE_PORT_SERVICE_IMPL_H_
