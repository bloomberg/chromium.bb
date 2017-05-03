// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/navigation_entry.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/public/headless_browser.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

namespace headless {

namespace {

const int kBackLog = 10;

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(const net::IPEndPoint& endpoint)
      : endpoint_(endpoint), socket_fd_(0) {
    DCHECK(endpoint_.address().IsValid());
  }

  explicit TCPServerSocketFactory(const size_t socket_fd)
      : socket_fd_(socket_fd) {}

 private:
  // content::DevToolsSocketFactory implementation:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    if (!socket_fd_) {
      std::unique_ptr<net::ServerSocket> socket(
          new net::TCPServerSocket(nullptr, net::NetLogSource()));
      if (socket->Listen(endpoint_, kBackLog) != net::OK)
        return std::unique_ptr<net::ServerSocket>();
      return socket;
    }
#if defined(OS_POSIX)
    std::unique_ptr<net::TCPServerSocket> tsock(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (tsock->AdoptSocket(socket_fd_) != net::OK) {
      LOG(ERROR) << "Failed to adopt open socket";
      return std::unique_ptr<net::ServerSocket>();
    }
    return std::unique_ptr<net::ServerSocket>(tsock.release());
#else
    LOG(ERROR) << "Can't inherit an open socket on non-Posix systems";
    return std::unique_ptr<net::ServerSocket>();
#endif
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  net::IPEndPoint endpoint_;
  size_t socket_fd_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

}  // namespace

void StartLocalDevToolsHttpHandler(HeadlessBrowser::Options* options) {
  std::unique_ptr<content::DevToolsSocketFactory> socket_factory;
  if (options->devtools_socket_fd == 0) {
    const net::IPEndPoint& endpoint = options->devtools_endpoint;
    socket_factory.reset(new TCPServerSocketFactory(endpoint));
  } else {
    const uint16_t socket_fd = options->devtools_socket_fd;
    socket_factory.reset(new TCPServerSocketFactory(socket_fd));
  }
  content::DevToolsAgentHost::StartRemoteDebuggingServer(
      std::move(socket_factory), std::string(),
      options->user_data_dir,  // TODO(altimin): Figure a proper value for this.
      base::FilePath(), options->product_name_and_version, options->user_agent);
}

void StopLocalDevToolsHttpHandler() {
  content::DevToolsAgentHost::StopRemoteDebuggingServer();
}

}  // namespace headless
