// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "components/ui_devtools/switches.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/server_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace ui {
namespace devtools {

namespace {
const char kChromeDeveloperToolsPrefix[] =
    "chrome-devtools://devtools/bundled/inspector.html?ws=";
}  // namespace

UiDevToolsServer::UiDevToolsServer()
    : thread_(new base::Thread("UiDevToolsServerThread")) {}

UiDevToolsServer::~UiDevToolsServer() {}

// static
std::unique_ptr<UiDevToolsServer> UiDevToolsServer::Create() {
  std::unique_ptr<UiDevToolsServer> server;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(kEnableUiDevTools)) {
    // TODO(mhashmi): Change port if more than one inspectable clients
    int port = 9223;  // Default port is 9223
    base::StringToInt(
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kEnableUiDevTools),
        &port);
    server.reset(new UiDevToolsServer());
    server->Start("127.0.0.1", port);
  }
  return server;
}

void UiDevToolsServer::AttachClient(std::unique_ptr<UiDevToolsClient> client) {
  clients_.push_back(std::move(client));
}

void UiDevToolsServer::SendOverWebSocket(int connection_id,
                                         const String& message) {
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::SendOverWebSocket,
                 base::Unretained(server_.get()), connection_id, message));
}

void UiDevToolsServer::Start(const std::string& address_string, uint16_t port) {
  if (thread_ && thread_->IsRunning())
    return;

  // Start IO thread upon which all the methods will run
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  if (thread_->StartWithOptions(options)) {
    thread_->task_runner()->PostTask(
        FROM_HERE, base::Bind(&UiDevToolsServer::StartServer,
                              base::Unretained(this), address_string, port));
  }
}

void UiDevToolsServer::StartServer(const std::string& address_string,
                                   uint16_t port) {
  std::unique_ptr<net::ServerSocket> socket(
      new net::TCPServerSocket(nullptr, net::NetLogSource()));
  constexpr int kBacklog = 1;
  if (socket->ListenWithAddressAndPort(address_string, port, kBacklog) !=
      net::OK)
    return;
  server_ = base::MakeUnique<net::HttpServer>(std::move(socket), this);
}

// HttpServer::Delegate Implementation
void UiDevToolsServer::OnConnect(int connection_id) {
  NOTIMPLEMENTED();
}

void UiDevToolsServer::OnHttpRequest(int connection_id,
                                     const net::HttpServerRequestInfo& info) {
  // Display a simple html page with all the clients and the corresponding
  // devtools links
  // TODO(mhashmi): Remove and display all clients under chrome://inspect/#other
  if (info.path.empty() || info.path == "/") {
    std::string clientHTML = "<html>";
    clientHTML +=
        "<h3>Copy paste the corresponding links in your browser to inspect "
        "them:</h3>";
    net::IPEndPoint ip;
    server_->GetLocalAddress(&ip);
    for (ClientsList::size_type i = 0; i != clients_.size(); i++) {
      clientHTML += base::StringPrintf(
          "<p><strong>%s</strong> (%s%s/%" PRIuS ")</p>",
          clients_[i]->name().c_str(), kChromeDeveloperToolsPrefix,
          ip.ToString().c_str(), i);
    }
    clientHTML += "</html>";
    thread_->task_runner()->PostTask(
        FROM_HERE,
        base::Bind(&net::HttpServer::Send200, base::Unretained(server_.get()),
                   connection_id, clientHTML, "text/html"));
  }
}

void UiDevToolsServer::OnWebSocketRequest(
    int connection_id,
    const net::HttpServerRequestInfo& info) {
  size_t target_id = 0;
  if (info.path.empty() ||
      !base::StringToSizeT(info.path.substr(1), &target_id) ||
      target_id > clients_.size())
    return;

  UiDevToolsClient* client = clients_[target_id].get();
  // Only one user can inspect the client at a time
  if (client->connected())
    return;
  client->set_connection_id(connection_id);
  connections_[connection_id] = client;
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&net::HttpServer::AcceptWebSocket,
                 base::Unretained(server_.get()), connection_id, info));
}

void UiDevToolsServer::OnWebSocketMessage(int connection_id,
                                          const std::string& data) {
  ConnectionsMap::iterator it = connections_.find(connection_id);
  DCHECK(it != connections_.end());
  UiDevToolsClient* client = it->second;
  DCHECK(client);
  thread_->task_runner()->PostTask(
      FROM_HERE,
      base::Bind(&UiDevToolsClient::Dispatch, base::Unretained(client), data));
}

void UiDevToolsServer::OnClose(int connection_id) {
  ConnectionsMap::iterator it = connections_.find(connection_id);
  DCHECK(it != connections_.end());
  UiDevToolsClient* client = it->second;
  DCHECK(client);
  client->set_connection_id(UiDevToolsClient::kNotConnected);
  connections_.erase(it);
}

}  // namespace devtools
}  // namespace ui
