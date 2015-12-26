// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/service_port_service_impl.h"

#include <utility>

#include "content/browser/message_port_message_filter.h"
#include "content/browser/message_port_service.h"
#include "content/browser/navigator_connect/navigator_connect_context_impl.h"
#include "content/common/service_port_type_converters.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/common/message_port_types.h"
#include "mojo/common/common_type_converters.h"
#include "url/gurl.h"

namespace content {

// static
void ServicePortServiceImpl::Create(
    const scoped_refptr<NavigatorConnectContextImpl>& navigator_connect_context,
    const scoped_refptr<MessagePortMessageFilter>& message_port_message_filter,
    mojo::InterfaceRequest<ServicePortService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::Bind(&ServicePortServiceImpl::CreateOnIOThread,
                 navigator_connect_context, message_port_message_filter,
                 base::Passed(&request)));
}

ServicePortServiceImpl::~ServicePortServiceImpl() {
  // Should always be destroyed on the IO thread, but can't check that with
  // DCHECK_CURRENTLY_ON because this class could be destroyed during thread
  // shutdown, at which point that check doesn't work.
  navigator_connect_context_->ServicePortServiceDestroyed(this);
}

void ServicePortServiceImpl::PostMessageToClient(
    int port_id,
    const MessagePortMessage& message,
    const std::vector<TransferredMessagePort>& sent_message_ports) {
  DCHECK(client_.get());
  // Hold messages on transferred message ports. Normally this wouldn't be
  // needed, but since MessagePort uses regular IPC while this class uses mojo,
  // without holding messages mojo IPC might overtake regular IPC resulting in a
  // non-functional port. When  WebMessagePortChannelImpl instances are
  // constructed in the renderer, they will send
  // MessagePortHostMsg_ReleaseMessages to release messages.
  for (const auto& port : sent_message_ports)
    MessagePortService::GetInstance()->HoldMessages(port.id);

  std::vector<int> new_routing_ids;
  message_port_message_filter_->UpdateMessagePortsWithNewRoutes(
      sent_message_ports, &new_routing_ids);
  client_->PostMessageToPort(
      port_id, mojo::String::From(message.message_as_string),
      mojo::Array<MojoTransferredMessagePortPtr>::From(sent_message_ports),
      mojo::Array<int32_t>::From(new_routing_ids));
}

// static
void ServicePortServiceImpl::CreateOnIOThread(
    const scoped_refptr<NavigatorConnectContextImpl>& navigator_connect_context,
    const scoped_refptr<MessagePortMessageFilter>& message_port_message_filter,
    mojo::InterfaceRequest<ServicePortService> request) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  new ServicePortServiceImpl(navigator_connect_context,
                             message_port_message_filter, std::move(request));
}

ServicePortServiceImpl::ServicePortServiceImpl(
    const scoped_refptr<NavigatorConnectContextImpl>& navigator_connect_context,
    const scoped_refptr<MessagePortMessageFilter>& message_port_message_filter,
    mojo::InterfaceRequest<ServicePortService> request)
    : binding_(this, std::move(request)),
      navigator_connect_context_(navigator_connect_context),
      message_port_message_filter_(message_port_message_filter),
      weak_ptr_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void ServicePortServiceImpl::SetClient(ServicePortServiceClientPtr client) {
  DCHECK(!client_.get());
  // TODO(mek): Set ErrorHandler to listen for errors.
  client_ = std::move(client);
}

void ServicePortServiceImpl::Connect(const mojo::String& target_url,
                                     const mojo::String& origin,
                                     const ConnectCallback& callback) {
  navigator_connect_context_->Connect(
      GURL(target_url), GURL(origin), this,
      base::Bind(&ServicePortServiceImpl::OnConnectResult,
                 weak_ptr_factory_.GetWeakPtr(), callback));
}

void ServicePortServiceImpl::PostMessageToPort(
    int32_t port_id,
    const mojo::String& message,
    mojo::Array<MojoTransferredMessagePortPtr> ports) {
  // TODO(mek): Similar to http://crbug.com/490222 this code should make sure
  // port_id belongs to the process this IPC was received from.
  std::vector<TransferredMessagePort> transferred_ports =
      ports.To<std::vector<TransferredMessagePort>>();

  MessagePortService* mps = MessagePortService::GetInstance();
  // First, call QueueMessages for all transferred ports, since the ports
  // haven't sent their own IPC for that.
  for (const TransferredMessagePort& port : transferred_ports) {
    mps->QueueMessages(port.id);
  }

  navigator_connect_context_->PostMessage(
      port_id, MessagePortMessage(message.To<base::string16>()),
      transferred_ports);
}

void ServicePortServiceImpl::ClosePort(int32_t port_id) {
  MessagePortService::GetInstance()->Destroy(port_id);
}

void ServicePortServiceImpl::OnConnectResult(const ConnectCallback& callback,
                                             int message_port_id,
                                             bool success) {
  callback.Run(success ? SERVICE_PORT_CONNECT_RESULT_ACCEPT
                       : SERVICE_PORT_CONNECT_RESULT_REJECT,
               message_port_id);
}

}  // namespace content
