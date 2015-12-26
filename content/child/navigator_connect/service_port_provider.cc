// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/navigator_connect/service_port_provider.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner_util.h"
#include "content/child/child_thread_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/message_port_messages.h"
#include "content/common/service_port_type_converters.h"
#include "content/public/common/navigator_connect_client.h"
#include "mojo/common/common_type_converters.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/modules/navigator_services/WebServicePortProviderClient.h"

namespace content {

namespace {

void ConnectToServiceOnMainThread(
    mojo::InterfaceRequest<ServicePortService> ptr) {
  ChildThreadImpl::current()->service_registry()->ConnectToRemoteService(
      std::move(ptr));
}

}  // namespace

ServicePortProvider::ServicePortProvider(
    blink::WebServicePortProviderClient* client,
    const scoped_refptr<base::SingleThreadTaskRunner>& main_loop)
    : client_(client), binding_(this), main_loop_(main_loop) {
  DCHECK(client_);
  AddRef();
}

void ServicePortProvider::destroy() {
  Release();
}

void ServicePortProvider::connect(
    const blink::WebURL& target_url,
    const blink::WebString& origin,
    blink::WebServicePortConnectCallbacks* raw_callbacks) {
  scoped_ptr<blink::WebServicePortConnectCallbacks> callbacks(raw_callbacks);
  // base::Unretained is safe here, as the mojo channel will be deleted (and
  // will wipe its callbacks) before 'this' is deleted.
  GetServicePortServicePtr()->Connect(
      target_url.string().utf8(), origin.utf8(),
      base::Bind(&ServicePortProvider::OnConnectResult, base::Unretained(this),
                 base::Passed(&callbacks)));
}

void ServicePortProvider::postMessage(
    blink::WebServicePortID port_id,
    const blink::WebString& message,
    blink::WebMessagePortChannelArray* channels) {
  // TODO(mek): If necesary, handle sending messages as values for system
  // services.

  // Have to extract IDs for the transferred MessagePorts on the main thread
  // to make sure all ports have been fully initialized. Actually sending the
  // message can safely be done on this thread, and using mojo, since there
  // shouldn't be any other IPCs where ordering matters.
  scoped_ptr<blink::WebMessagePortChannelArray> channel_array(channels);
  base::PostTaskAndReplyWithResult(
      main_loop_.get(), FROM_HERE,
      base::Bind(
          &WebMessagePortChannelImpl::ExtractMessagePortIDsWithoutQueueing,
          base::Passed(&channel_array)),
      base::Bind(&ServicePortProvider::PostMessageToBrowser, this, port_id,
                 // We cast WebString to string16 before crossing threads.
                 // for thread-safety.
                 static_cast<base::string16>(message)));
}

void ServicePortProvider::closePort(blink::WebServicePortID port_id) {
  GetServicePortServicePtr()->ClosePort(port_id);
}

void ServicePortProvider::PostMessageToPort(
    int32_t port_id,
    const mojo::String& message,
    mojo::Array<MojoTransferredMessagePortPtr> ports,
    mojo::Array<int32_t> new_routing_ids) {
  client_->postMessage(port_id, message.To<base::string16>(),
                       WebMessagePortChannelImpl::CreatePorts(
                           ports.To<std::vector<TransferredMessagePort>>(),
                           new_routing_ids.To<std::vector<int>>(), main_loop_));
}

ServicePortProvider::~ServicePortProvider() {
}

void ServicePortProvider::PostMessageToBrowser(
    int port_id,
    const base::string16& message,
    const std::vector<TransferredMessagePort> ports) {
  GetServicePortServicePtr()->PostMessageToPort(
      port_id, mojo::String::From(message),
      mojo::Array<MojoTransferredMessagePortPtr>::From(ports));
}

void ServicePortProvider::OnConnectResult(
    scoped_ptr<blink::WebServicePortConnectCallbacks> callbacks,
    ServicePortConnectResult result,
    int32_t port_id) {
  if (result == SERVICE_PORT_CONNECT_RESULT_ACCEPT) {
    callbacks->onSuccess(port_id);
  } else {
    callbacks->onError();
  }
}

ServicePortServicePtr& ServicePortProvider::GetServicePortServicePtr() {
  if (!service_port_service_.get()) {
    mojo::InterfaceRequest<ServicePortService> request =
        mojo::GetProxy(&service_port_service_);
    main_loop_->PostTask(FROM_HERE, base::Bind(&ConnectToServiceOnMainThread,
                                               base::Passed(&request)));

    // Setup channel for browser to post events back to this class.
    ServicePortServiceClientPtr client_ptr;
    binding_.Bind(GetProxy(&client_ptr));
    service_port_service_->SetClient(std::move(client_ptr));
  }
  return service_port_service_;
}

}  // namespace content
