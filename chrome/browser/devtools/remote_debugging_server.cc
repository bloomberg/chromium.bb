// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/devtools/browser_list_tabcontents_provider.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/browser/devtools_http_handler.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"

namespace {

base::LazyInstance<bool>::Leaky g_tethering_enabled = LAZY_INSTANCE_INITIALIZER;

const uint16 kMinTetheringPort = 9333;
const uint16 kMaxTetheringPort = 9444;
const int kBackLog = 10;

class TCPServerSocketFactory
    : public content::DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16 port)
      : address_(address),
        port_(port),
        last_tethering_port_(kMinTetheringPort) {
  }

 private:
  // content::DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return socket;
  }

  scoped_ptr<net::ServerSocket> CreateForTethering(std::string* name) override {
    if (!g_tethering_enabled.Get())
      return scoped_ptr<net::ServerSocket>();

    if (last_tethering_port_ == kMaxTetheringPort)
      last_tethering_port_ = kMinTetheringPort;
    uint16 port = ++last_tethering_port_;
    *name = base::IntToString(port);
    scoped_ptr<net::TCPServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->ListenWithAddressAndPort("127.0.0.1", port, kBackLog) !=
        net::OK) {
      return scoped_ptr<net::ServerSocket>();
    }
    return socket.Pass();
  }

  std::string address_;
  uint16 port_;
  uint16 last_tethering_port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

}  // namespace

// static
void RemoteDebuggingServer::EnableTetheringForDebug() {
  g_tethering_enabled.Get() = true;
}

RemoteDebuggingServer::RemoteDebuggingServer(
    chrome::HostDesktopType host_desktop_type,
    const std::string& ip,
    uint16 port) {
  base::FilePath output_dir;
  if (!port) {
    // The client requested an ephemeral port. Must write the selected
    // port to a well-known location in the profile directory to
    // bootstrap the connection process.
    bool result = PathService::Get(chrome::DIR_USER_DATA, &output_dir);
    DCHECK(result);
  }

  devtools_http_handler_.reset(content::DevToolsHttpHandler::Start(
      make_scoped_ptr(new TCPServerSocketFactory(ip, port)),
      std::string(),
      new BrowserListTabContentsProvider(host_desktop_type),
      output_dir));
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
}
