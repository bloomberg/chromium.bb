// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "fuchsia/engine/browser/web_engine_devtools_socket_factory.h"
#include "fuchsia/engine/browser/web_engine_browser_context.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"

namespace {

const int kTcpListenBackLog = 5;

}  //  namespace

WebEngineDevToolsSocketFactory::WebEngineDevToolsSocketFactory(
    OnPortOpenedCallback callback)
    : callback_(std::move(callback)) {}

WebEngineDevToolsSocketFactory::~WebEngineDevToolsSocketFactory() = default;

std::unique_ptr<net::ServerSocket>
WebEngineDevToolsSocketFactory::CreateForHttpServer() {
  std::unique_ptr<net::ServerSocket> socket(
      new net::TCPServerSocket(nullptr, net::NetLogSource()));
  if (socket->Listen(net::IPEndPoint(net::IPAddress::IPv4Localhost(), 0),
                     kTcpListenBackLog) == net::OK) {
    net::IPEndPoint end_point;
    socket->GetLocalAddress(&end_point);
    callback_.Run(end_point.port());
    return socket;
  }
  int error = socket->Listen(
      net::IPEndPoint(net::IPAddress::IPv6Localhost(), 0), kTcpListenBackLog);
  if (error == net::OK) {
    net::IPEndPoint end_point;
    socket->GetLocalAddress(&end_point);
    callback_.Run(end_point.port());
    return socket;
  }
  LOG(WARNING) << "Failed to start the HTTP debugger service. "
               << net::ErrorToString(error);
  return nullptr;
}

std::unique_ptr<net::ServerSocket>
WebEngineDevToolsSocketFactory::CreateForTethering(std::string* out_name) {
  return nullptr;
}
