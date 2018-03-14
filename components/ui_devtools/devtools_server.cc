// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/ui_devtools/devtools_server.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/format_macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/server/http_server_request_info.h"
#include "net/socket/server_socket.h"
#include "net/socket/tcp_server_socket.h"

namespace ui_devtools {

namespace {
const char kChromeDeveloperToolsPrefix[] =
    "chrome-devtools://devtools/bundled/inspector.html?ws=";

bool IsDevToolsEnabled(const char* enable_devtools_flag) {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      enable_devtools_flag);
}

int GetUiDevToolsPort(const char* enable_devtools_flag, int default_port) {
  DCHECK(IsDevToolsEnabled(enable_devtools_flag));
  // This value is duplicated in the chrome://flags description.
  int port;
  if (!base::StringToInt(
          base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
              enable_devtools_flag),
          &port))
    port = default_port;
  return port;
}

constexpr net::NetworkTrafficAnnotationTag kUIDevtoolsServer =
    net::DefineNetworkTrafficAnnotation("ui_devtools_server", R"(
      semantics {
        sender: "UI Devtools Server"
        description:
          "Backend for UI DevTools, to inspect Aura/Views UI."
        trigger:
          "Run with '--enable-ui-devtools' switch."
        data: "Debugging data, including any data on the open pages."
        destination: OTHER
        destination_other: "The data can be sent to any destination."
      }
      policy {
        cookies_allowed: NO
        setting:
          "This request cannot be disabled in settings. However it will never "
          "be made if user does not run with '--enable-ui-devtools' switch."
        policy_exception_justification:
          "Not implemented, only used in Devtools and is behind a switch."
      })");

}  // namespace

UiDevToolsServer* UiDevToolsServer::devtools_server_ = nullptr;

UiDevToolsServer::UiDevToolsServer(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    const char* enable_devtools_flag,
    int default_port)
    : io_thread_task_runner_(io_thread_task_runner),
      port_(GetUiDevToolsPort(enable_devtools_flag, default_port)) {
  DCHECK(!devtools_server_);
  main_thread_task_runner_ = base::ThreadTaskRunnerHandle::Get();
  devtools_server_ = this;
  if (io_thread_task_runner_)
    return;
  // If io_thread_task_runner not passed in, create an I/O thread
  thread_.reset(new base::Thread("UiDevToolsServerThread"));
  base::Thread::Options options;
  options.message_loop_type = base::MessageLoop::TYPE_IO;
  CHECK(thread_->StartWithOptions(options));
  io_thread_task_runner_ = thread_->task_runner();
}

UiDevToolsServer::~UiDevToolsServer() {
  if (io_thread_task_runner_)
    io_thread_task_runner_->DeleteSoon(FROM_HERE, server_.release());
  if (thread_ && thread_->IsRunning())
    thread_->Stop();
  devtools_server_ = nullptr;
}

// static
std::unique_ptr<UiDevToolsServer> UiDevToolsServer::Create(
    scoped_refptr<base::SingleThreadTaskRunner> io_thread_task_runner,
    const char* enable_devtools_flag,
    int default_port) {
  std::unique_ptr<UiDevToolsServer> server;
  if (IsDevToolsEnabled(enable_devtools_flag) && !devtools_server_) {
    // TODO(mhashmi): Change port if more than one inspectable clients
    server.reset(new UiDevToolsServer(io_thread_task_runner,
                                      enable_devtools_flag, default_port));
    server->Start("0.0.0.0");
  }
  return server;
}

// static
std::vector<UiDevToolsServer::NameUrlPair>
UiDevToolsServer::GetClientNamesAndUrls() {
  std::vector<NameUrlPair> pairs;
  if (!devtools_server_)
    return pairs;

  for (ClientsList::size_type i = 0; i != devtools_server_->clients_.size();
       i++) {
    pairs.push_back(std::pair<std::string, std::string>(
        devtools_server_->clients_[i]->name(),
        base::StringPrintf("%s0.0.0.0:%d/%" PRIuS, kChromeDeveloperToolsPrefix,
                           devtools_server_->port(), i)));
  }
  return pairs;
}

void UiDevToolsServer::AttachClient(std::unique_ptr<UiDevToolsClient> client) {
  clients_.push_back(std::move(client));
}

void UiDevToolsServer::SendOverWebSocket(int connection_id,
                                         const String& message) {
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&net::HttpServer::SendOverWebSocket,
                            base::Unretained(server_.get()), connection_id,
                            message, kUIDevtoolsServer));
}

void UiDevToolsServer::Start(const std::string& address_string) {
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&UiDevToolsServer::StartServer,
                            base::Unretained(this), address_string));
}

void UiDevToolsServer::StartServer(const std::string& address_string) {
  DCHECK(!server_);
  std::unique_ptr<net::ServerSocket> socket(
      new net::TCPServerSocket(nullptr, net::NetLogSource()));
  constexpr int kBacklog = 1;
  if (socket->ListenWithAddressAndPort(address_string, port_, kBacklog) !=
      net::OK)
    return;
  server_ = std::make_unique<net::HttpServer>(std::move(socket), this);
}

// HttpServer::Delegate Implementation
void UiDevToolsServer::OnConnect(int connection_id) {
  NOTIMPLEMENTED();
}

void UiDevToolsServer::OnHttpRequest(int connection_id,
                                     const net::HttpServerRequestInfo& info) {
  NOTIMPLEMENTED();
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
  io_thread_task_runner_->PostTask(
      FROM_HERE, base::Bind(&net::HttpServer::AcceptWebSocket,
                            base::Unretained(server_.get()), connection_id,
                            info, kUIDevtoolsServer));
}

void UiDevToolsServer::OnWebSocketMessage(int connection_id,
                                          const std::string& data) {
  ConnectionsMap::iterator it = connections_.find(connection_id);
  DCHECK(it != connections_.end());
  UiDevToolsClient* client = it->second;
  DCHECK(client);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UiDevToolsClient::Dispatch, base::Unretained(client), data));
}

void UiDevToolsServer::OnClose(int connection_id) {
  ConnectionsMap::iterator it = connections_.find(connection_id);
  if (it == connections_.end())
    return;
  UiDevToolsClient* client = it->second;
  DCHECK(client);
  main_thread_task_runner_->PostTask(
      FROM_HERE,
      base::Bind(&UiDevToolsClient::Disconnect, base::Unretained(client)));
  connections_.erase(it);
}

}  // namespace ui_devtools
