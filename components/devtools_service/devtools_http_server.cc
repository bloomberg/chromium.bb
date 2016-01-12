// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_service/devtools_http_server.h"

#include <stddef.h>
#include <string.h>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/stl_util.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/devtools_service/devtools_agent_host.h"
#include "components/devtools_service/devtools_registry_impl.h"
#include "components/devtools_service/devtools_service.h"
#include "mojo/public/cpp/system/data_pipe.h"
#include "mojo/services/network/public/cpp/web_socket_read_queue.h"
#include "mojo/services/network/public/cpp/web_socket_write_queue.h"
#include "mojo/services/network/public/interfaces/net_address.mojom.h"
#include "mojo/services/network/public/interfaces/network_service.mojom.h"
#include "mojo/services/network/public/interfaces/web_socket.mojom.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace devtools_service {

namespace {

const char kPageUrlPrefix[] = "/devtools/page/";
const char kBrowserUrlPrefix[] = "/devtools/browser";
const char kJsonRequestUrlPrefix[] = "/json";

const char kActivateCommand[] = "activate";
const char kCloseCommand[] = "close";
const char kListCommand[] = "list";
const char kNewCommand[] = "new";
const char kVersionCommand[] = "version";

const char kTargetIdField[] = "id";
const char kTargetTypeField[] = "type";
const char kTargetTitleField[] = "title";
const char kTargetDescriptionField[] = "description";
const char kTargetUrlField[] = "url";
const char kTargetWebSocketDebuggerUrlField[] = "webSocketDebuggerUrl";
const char kTargetDevtoolsFrontendUrlField[] = "devtoolsFrontendUrl";

std::string GetHeaderValue(const mojo::HttpRequest& request,
                           const std::string& name) {
  for (size_t i = 0; i < request.headers.size(); ++i) {
    if (name == request.headers[i]->name)
      return request.headers[i]->value;
  }

  return std::string();
}

bool ParseJsonPath(const std::string& path,
                   std::string* command,
                   std::string* target_id) {
  // Fall back to list in case of empty query.
  if (path.empty()) {
    *command = kListCommand;
    return true;
  }

  if (path.find("/") != 0) {
    // Malformed command.
    return false;
  }
  *command = path.substr(1);

  size_t separator_pos = command->find("/");
  if (separator_pos != std::string::npos) {
    *target_id = command->substr(separator_pos + 1);
    *command = command->substr(0, separator_pos);
  }
  return true;
}

mojo::HttpResponsePtr MakeResponse(uint32_t status_code,
                                   const std::string& content_type,
                                   const std::string& body) {
  mojo::HttpResponsePtr response(mojo::HttpResponse::New());
  response->headers.resize(2);
  response->headers[0] = mojo::HttpHeader::New();
  response->headers[0]->name = "Content-Length";
  response->headers[0]->value =
      base::StringPrintf("%lu", static_cast<unsigned long>(body.size()));
  response->headers[1] = mojo::HttpHeader::New();
  response->headers[1]->name = "Content-Type";
  response->headers[1]->value = content_type;

  if (!body.empty()) {
    uint32_t num_bytes = static_cast<uint32_t>(body.size());
    MojoCreateDataPipeOptions options;
    options.struct_size = sizeof(MojoCreateDataPipeOptions);
    options.flags = MOJO_CREATE_DATA_PIPE_OPTIONS_FLAG_NONE;
    options.element_num_bytes = 1;
    options.capacity_num_bytes = num_bytes;
    mojo::DataPipe data_pipe(options);
    response->body = std::move(data_pipe.consumer_handle);
    MojoResult result =
        WriteDataRaw(data_pipe.producer_handle.get(), body.data(), &num_bytes,
                     MOJO_WRITE_DATA_FLAG_ALL_OR_NONE);
    CHECK_EQ(MOJO_RESULT_OK, result);
  }
  return response;
}

mojo::HttpResponsePtr MakeJsonResponse(uint32_t status_code,
                                       base::Value* value,
                                       const std::string& message) {
  // Serialize value and message.
  std::string json_value;
  if (value) {
    base::JSONWriter::WriteWithOptions(
        *value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json_value);
  }

  return MakeResponse(status_code, "application/json; charset=UTF-8",
                      json_value + message);
}

class WebSocketRelayer : public DevToolsAgentHost::Delegate,
                         public mojo::WebSocketClient {
 public:
  // Creates a WebSocketRelayer instance and sets it as the delegate of
  // |agent_host|.
  //
  // The object destroys itself when either of the following happens:
  // - |agent_host| is dead and the object finishes all pending sends (if any)
  //   to the Web socket; or
  // - the underlying pipe of |web_socket| is closed and the object finishes all
  //   pending receives (if any) from the Web socket.
  static mojo::WebSocketClientPtr SetUp(
      DevToolsAgentHost* agent_host,
      mojo::WebSocketPtr web_socket,
      mojo::ScopedDataPipeProducerHandle send_stream) {
    DCHECK(agent_host);
    DCHECK(web_socket);
    DCHECK(send_stream.is_valid());

    mojo::WebSocketClientPtr web_socket_client;
    new WebSocketRelayer(agent_host, std::move(web_socket),
                         std::move(send_stream), &web_socket_client);
    return web_socket_client;
  }

 private:
  WebSocketRelayer(DevToolsAgentHost* agent_host,
                   mojo::WebSocketPtr web_socket,
                   mojo::ScopedDataPipeProducerHandle send_stream,
                   mojo::WebSocketClientPtr* web_socket_client)
      : agent_host_(agent_host),
        binding_(this, web_socket_client),
        web_socket_(std::move(web_socket)),
        send_stream_(std::move(send_stream)),
        write_send_stream_(new mojo::WebSocketWriteQueue(send_stream_.get())),
        pending_send_count_(0),
        pending_receive_count_(0) {
    web_socket_.set_connection_error_handler([this]() { OnConnectionError(); });
    agent_host->SetDelegate(this);
  }

  ~WebSocketRelayer() override {
    if (agent_host_)
      agent_host_->SetDelegate(nullptr);
  }

  // DevToolsAgentHost::Delegate implementation.
  void DispatchProtocolMessage(DevToolsAgentHost* agent_host,
                               const std::string& message) override {
    if (!web_socket_)
      return;

    // TODO(yzshen): It shouldn't be an issue to pass an empty message. However,
    // WebSocket{Read,Write}Queue doesn't handle that correctly.
    if (message.empty())
      return;

    pending_send_count_++;
    uint32_t size = static_cast<uint32_t>(message.size());
    write_send_stream_->Write(
        &message[0], size,
        base::Bind(&WebSocketRelayer::OnFinishedWritingSendStream,
                   base::Unretained(this), size));
  }

  void OnAgentHostClosed(DevToolsAgentHost* agent_host) override {
    DispatchProtocolMessage(agent_host_,
                            "{ \"method\": \"Inspector.detached\", "
                            "\"params\": { \"reason\": \"target_closed\" } }");

    // No need to call SetDelegate(nullptr) on |agent_host_| because it is going
    // away.
    agent_host_ = nullptr;

    if (ShouldSelfDestruct())
      delete this;
  }

  // WebSocketClient implementation.
  void DidConnect(const mojo::String& selected_subprotocol,
                  const mojo::String& extensions,
                  mojo::ScopedDataPipeConsumerHandle receive_stream) override {
    receive_stream_ = std::move(receive_stream);
    read_receive_stream_.reset(
        new mojo::WebSocketReadQueue(receive_stream_.get()));
  }

  void DidReceiveData(bool fin,
                      mojo::WebSocket::MessageType type,
                      uint32_t num_bytes) override {
    if (!agent_host_)
      return;

    // TODO(yzshen): It shouldn't be an issue to pass an empty message. However,
    // WebSocket{Read,Write}Queue doesn't handle that correctly.
    if (num_bytes == 0)
      return;

    pending_receive_count_++;
    read_receive_stream_->Read(
        num_bytes, base::Bind(&WebSocketRelayer::OnFinishedReadingReceiveStream,
                              base::Unretained(this), num_bytes));
  }

  void DidReceiveFlowControl(int64_t quota) override {}

  void DidFail(const mojo::String& message) override {}

  void DidClose(bool was_clean,
                uint16_t code,
                const mojo::String& reason) override {}

  void OnConnectionError() {
    web_socket_ = nullptr;
    binding_.Close();

    if (ShouldSelfDestruct())
      delete this;
  }

  void OnFinishedWritingSendStream(uint32_t num_bytes, const char* buffer) {
    DCHECK_GT(pending_send_count_, 0u);
    pending_send_count_--;

    if (web_socket_ && buffer)
      web_socket_->Send(true, mojo::WebSocket::MESSAGE_TYPE_TEXT, num_bytes);

    if (ShouldSelfDestruct())
      delete this;
  }

  void OnFinishedReadingReceiveStream(uint32_t num_bytes, const char* data) {
    DCHECK_GT(pending_receive_count_, 0u);
    pending_receive_count_--;

    if (agent_host_ && data)
      agent_host_->SendProtocolMessageToAgent(std::string(data, num_bytes));

    if (ShouldSelfDestruct())
      delete this;
  }

  bool ShouldSelfDestruct() const {
    return (!agent_host_ && pending_send_count_ == 0) ||
           (!web_socket_ && pending_receive_count_ == 0);
  }

  DevToolsAgentHost* agent_host_;
  mojo::Binding<WebSocketClient> binding_;
  mojo::WebSocketPtr web_socket_;

  mojo::ScopedDataPipeProducerHandle send_stream_;
  scoped_ptr<mojo::WebSocketWriteQueue> write_send_stream_;
  size_t pending_send_count_;

  mojo::ScopedDataPipeConsumerHandle receive_stream_;
  scoped_ptr<mojo::WebSocketReadQueue> read_receive_stream_;
  size_t pending_receive_count_;

  DISALLOW_COPY_AND_ASSIGN(WebSocketRelayer);
};

}  // namespace

class DevToolsHttpServer::HttpConnectionDelegateImpl
    : public mojo::HttpConnectionDelegate {
 public:
  HttpConnectionDelegateImpl(
      DevToolsHttpServer* owner,
      mojo::HttpConnectionPtr connection,
      mojo::InterfaceRequest<HttpConnectionDelegate> delegate_request)
      : owner_(owner),
        connection_(std::move(connection)),
        binding_(this, std::move(delegate_request)) {
    DCHECK(owner_);
    DCHECK(connection_);
    DCHECK(binding_.is_bound());

    auto error_handler = [this]() { owner_->OnConnectionClosed(this); };
    connection_.set_connection_error_handler(error_handler);
    binding_.set_connection_error_handler(error_handler);
  }

  mojo::HttpConnection* connection() { return connection_.get(); }

 private:
  // mojo::HttpConnectionDelegate implementation:
  void OnReceivedRequest(mojo::HttpRequestPtr request,
                         const OnReceivedRequestCallback& callback) override {
    owner_->OnReceivedRequest(this, std::move(request), callback);
  }

  void OnReceivedWebSocketRequest(
      mojo::HttpRequestPtr request,
      const OnReceivedWebSocketRequestCallback& callback) override {
    owner_->OnReceivedWebSocketRequest(this, std::move(request), callback);
  }

  DevToolsHttpServer* const owner_;
  mojo::HttpConnectionPtr connection_;
  mojo::Binding<HttpConnectionDelegate> binding_;

  DISALLOW_COPY_AND_ASSIGN(HttpConnectionDelegateImpl);
};

DevToolsHttpServer::DevToolsHttpServer(DevToolsService* service,
                                       uint16_t remote_debugging_port)
    : service_(service), remote_debugging_port_(remote_debugging_port) {
  VLOG(1) << "Remote debugging HTTP server is started on port "
          << remote_debugging_port << ".";
  mojo::NetworkServicePtr network_service;
  service_->application()->ConnectToService("mojo:network_service",
                                            &network_service);

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
      std::move(local_address), std::move(http_server_delegate),
      mojo::NetworkService::CreateHttpServerCallback());
}

DevToolsHttpServer::~DevToolsHttpServer() {
  STLDeleteElements(&connections_);
}

void DevToolsHttpServer::OnConnected(
    mojo::HttpConnectionPtr connection,
    mojo::InterfaceRequest<mojo::HttpConnectionDelegate> delegate) {
  connections_.insert(new HttpConnectionDelegateImpl(
      this, std::move(connection), std::move(delegate)));
}

void DevToolsHttpServer::OnReceivedRequest(
    HttpConnectionDelegateImpl* connection,
    mojo::HttpRequestPtr request,
    const OnReceivedRequestCallback& callback) {
  DCHECK(connections_.find(connection) != connections_.end());

  if (request->url.get().find(kJsonRequestUrlPrefix) == 0) {
    mojo::HttpResponsePtr response = ProcessJsonRequest(std::move(request));
    if (response)
      callback.Run(std::move(response));
    else
      OnConnectionClosed(connection);
  } else {
    // TODO(yzshen): Implement it.
    NOTIMPLEMENTED();
    callback.Run(MakeResponse(404, "text/html", "Not implemented yet!"));
  }
}

void DevToolsHttpServer::OnReceivedWebSocketRequest(
    HttpConnectionDelegateImpl* connection,
    mojo::HttpRequestPtr request,
    const OnReceivedWebSocketRequestCallback& callback) {
  DCHECK(connections_.find(connection) != connections_.end());

  std::string path = request->url;
  size_t browser_pos = path.find(kBrowserUrlPrefix);
  if (browser_pos == 0) {
    // TODO(yzshen): Implement it.
    NOTIMPLEMENTED();
    callback.Run(nullptr, mojo::ScopedDataPipeConsumerHandle(), nullptr);
    return;
  }

  size_t pos = path.find(kPageUrlPrefix);
  if (pos != 0) {
    callback.Run(nullptr, mojo::ScopedDataPipeConsumerHandle(), nullptr);
    return;
  }

  std::string target_id = path.substr(strlen(kPageUrlPrefix));
  DevToolsAgentHost* agent = service_->registry()->GetAgentById(target_id);
  if (!agent || agent->IsAttached()) {
    callback.Run(nullptr, mojo::ScopedDataPipeConsumerHandle(), nullptr);
    return;
  }

  mojo::WebSocketPtr web_socket;
  mojo::InterfaceRequest<mojo::WebSocket> web_socket_request =
      mojo::GetProxy(&web_socket);
  mojo::DataPipe data_pipe;
  mojo::WebSocketClientPtr web_socket_client = WebSocketRelayer::SetUp(
      agent, std::move(web_socket), std::move(data_pipe.producer_handle));
  callback.Run(std::move(web_socket_request),
               std::move(data_pipe.consumer_handle),
               std::move(web_socket_client));
}

void DevToolsHttpServer::OnConnectionClosed(
    HttpConnectionDelegateImpl* connection) {
  DCHECK(connections_.find(connection) != connections_.end());

  delete connection;
  connections_.erase(connection);
}

mojo::HttpResponsePtr DevToolsHttpServer::ProcessJsonRequest(
    mojo::HttpRequestPtr request) {
  // Trim "/json".
  std::string path = request->url.get().substr(strlen(kJsonRequestUrlPrefix));

  // Trim query.
  size_t query_pos = path.find("?");
  if (query_pos != std::string::npos)
    path = path.substr(0, query_pos);

  // Trim fragment.
  size_t fragment_pos = path.find("#");
  if (fragment_pos != std::string::npos)
    path = path.substr(0, fragment_pos);

  std::string command;
  std::string target_id;
  if (!ParseJsonPath(path, &command, &target_id))
    return MakeJsonResponse(404, nullptr,
                            "Malformed query: " + request->url.get());

  if (command == kVersionCommand || command == kNewCommand ||
      command == kActivateCommand || command == kCloseCommand) {
    NOTIMPLEMENTED();
    return MakeJsonResponse(404, nullptr,
                            "Not implemented yet: " + request->url.get());
  }

  if (command == kListCommand) {
    DevToolsRegistryImpl::Iterator iter(service_->registry());
    if (iter.IsAtEnd()) {
      // If no agent is available, return a nullptr to indicate that the
      // connection should be closed.
      return nullptr;
    }

    std::string host = GetHeaderValue(*request, "host");
    if (host.empty()) {
      host = base::StringPrintf("127.0.0.1:%u",
                                static_cast<unsigned>(remote_debugging_port_));
    }

    base::ListValue list_value;
    for (; !iter.IsAtEnd(); iter.Advance()) {
      scoped_ptr<base::DictionaryValue> dict_value(new base::DictionaryValue());

      // TODO(yzshen): Add more information.
      dict_value->SetString(kTargetDescriptionField, std::string());
      dict_value->SetString(kTargetDevtoolsFrontendUrlField, std::string());
      dict_value->SetString(kTargetIdField, iter.value()->id());
      dict_value->SetString(kTargetTitleField, std::string());
      dict_value->SetString(kTargetTypeField, "page");
      dict_value->SetString(kTargetUrlField, std::string());
      dict_value->SetString(
          kTargetWebSocketDebuggerUrlField,
          base::StringPrintf("ws://%s%s%s", host.c_str(), kPageUrlPrefix,
                             iter.value()->id().c_str()));
      list_value.Append(std::move(dict_value));
    }
    return MakeJsonResponse(200, &list_value, std::string());
  }

  return MakeJsonResponse(404, nullptr, "Unknown command: " + command);
}

}  // namespace devtools_service
