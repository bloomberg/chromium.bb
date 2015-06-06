// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_http_server.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "components/devtools_service/devtools_service.h"
#include "mojo/application/public/cpp/application_impl.h"
#include "mojo/services/network/public/interfaces/http_message.mojom.h"
#include "mojo/services/network/public/interfaces/net_address.mojom.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"

namespace devtools_service {

class DevToolsHttpServer::HttpConnectionDelegateImpl
    : public mojo::HttpConnectionDelegate,
      public mojo::ErrorHandler {
 public:
  HttpConnectionDelegateImpl(
      DevToolsHttpServer* owner,
      mojo::HttpConnectionPtr connection,
      mojo::InterfaceRequest<HttpConnectionDelegate> delegate_request)
      : owner_(owner),
        connection_(connection.Pass()),
        binding_(this, delegate_request.Pass()) {
    DCHECK(owner_);
    DCHECK(connection_);
    DCHECK(binding_.is_bound());

    connection_.set_error_handler(this);
    binding_.set_error_handler(this);
  }

  mojo::HttpConnection* connection() { return connection_.get(); }

 private:
  // mojo::HttpConnectionDelegate implementation:
  void OnReceivedRequest(mojo::HttpRequestPtr request,
                         const OnReceivedRequestCallback& callback) override {
    owner_->OnReceivedRequest(this, request.Pass(), callback);
  }

  void OnReceivedWebSocketRequest(
      mojo::HttpRequestPtr request,
      const OnReceivedWebSocketRequestCallback& callback) override {
    owner_->OnReceivedWebSocketRequest(this, request.Pass(), callback);
  }

  // mojo::ErrorHandler implementation.
  void OnConnectionError() override { owner_->OnConnectionClosed(this); }

  DevToolsHttpServer* const owner_;
  mojo::HttpConnectionPtr connection_;
  mojo::Binding<HttpConnectionDelegate> binding_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnectionDelegateImpl);
};

DevToolsHttpServer::DevToolsHttpServer(DevToolsService* service,
                                       uint16_t remote_debugging_port)
    : service_(service) {
  VLOG(1) << "Remote debugging HTTP server is started on port "
          << remote_debugging_port << ".";
  mojo::NetworkServicePtr network_service;
  mojo::URLRequestPtr request(mojo::URLRequest::New());
  request->url = "mojo:network_service";
  service_->application()->ConnectToService(request.Pass(), &network_service);

  mojo::NetAddressPtr local_address(mojo::NetAddress::New());
  local_address->family = mojo::NET_ADDRESS_FAMILY_IPV4;
  local_address->ipv4 = mojo::NetAddressIPv4::New();
  local_address->ipv4->port = remote_debugging_port;
  local_address->ipv4->addr.resize(4);
  local_address->ipv4->addr[0] = 127;
  local_address->ipv4->addr[1] = 0;
  local_address->ipv4->addr[2] = 0;
  local_address->ipv4->addr[3] = 1;

  mojo::HttpServerDelegatePtr http_server_delegate;
  http_server_delegate_binding_.reset(
      new mojo::Binding<mojo::HttpServerDelegate>(this, &http_server_delegate));
  network_service->CreateHttpServer(
      local_address.Pass(), http_server_delegate.Pass(),
      mojo::NetworkService::CreateHttpServerCallback());
}

DevToolsHttpServer::~DevToolsHttpServer() {
  STLDeleteElements(&connections_);
}

void DevToolsHttpServer::OnConnected(
    mojo::HttpConnectionPtr connection,
    mojo::InterfaceRequest<mojo::HttpConnectionDelegate> delegate) {
  connections_.insert(
      new HttpConnectionDelegateImpl(this, connection.Pass(), delegate.Pass()));
}

void DevToolsHttpServer::OnReceivedRequest(
    HttpConnectionDelegateImpl* connection,
    mojo::HttpRequestPtr request,
    const OnReceivedRequestCallback& callback) {
  DCHECK(connections_.find(connection) != connections_.end());

  // TODO(yzshen): Implement it.
  static const char kNotImplemented[] = "Not implemented yet!";
  mojo::HttpResponsePtr response(mojo::HttpResponse::New());
  response->headers.resize(2);
  response->headers[0] = mojo::HttpHeader::New();
  response->headers[0]->name = "Content-Length";
  response->headers[0]->value = base::StringPrintf(
      "%lu", static_cast<unsigned long>(sizeof(kNotImplemented)));
  response->headers[1] = mojo::HttpHeader::New();
  response->headers[1]->name = "Content-Type";
  response->headers[1]->value = "text/html";

  uint32_t num_bytes = sizeof(kNotImplemented);
  MojoCreateDataPipeOptions options;
  options.struct_size = sizeof(MojoCreateDataPipeOptions);
  options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
  options.element_num_bytes = 1;
  options.capacity_num_bytes = num_bytes;
  mojo::DataPipe data_pipe(options);
  response->body = data_pipe.consumer_handle.Pass();
  WriteDataRaw(data_pipe.producer_handle.get(), kNotImplemented, &num_bytes,
               MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);

  callback.Run(response.Pass());
}

void DevToolsHttpServer::OnReceivedWebSocketRequest(
    HttpConnectionDelegateImpl* connection,
    mojo::HttpRequestPtr request,
    const OnReceivedWebSocketRequestCallback& callback) {
  DCHECK(connections_.find(connection) != connections_.end());

  // TODO(yzshen): Implement it.
  NOTIMPLEMENTED();
}

void DevToolsHttpServer::OnConnectionClosed(
    HttpConnectionDelegateImpl* connection) {
  DCHECK(connections_.find(connection) != connections_.end());

  delete connection;
  connections_.erase(connection);
}

}  // namespace devtools_service
