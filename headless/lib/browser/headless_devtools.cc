// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "headless/lib/browser/headless_devtools.h"

#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/browser/navigation_entry.h"
#include "headless/grit/headless_lib_resources.h"
#include "headless/public/headless_browser.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

using devtools_http_handler::DevToolsHttpHandler;

namespace headless {

namespace {

const int kBackLog = 10;

class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit TCPServerSocketFactory(const net::IPEndPoint& endpoint)
      : endpoint_(endpoint) {
    DCHECK(endpoint_.address().IsValid());
  }

 private:
  // content::DevToolsSocketFactory implementation:
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->Listen(endpoint_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* out_name) override {
    return nullptr;
  }

  net::IPEndPoint endpoint_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

class HeadlessDevToolsDelegate
    : public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  HeadlessDevToolsDelegate();
  ~HeadlessDevToolsDelegate() override;

  // devtools_http_handler::DevToolsHttpHandlerDelegate implementation:
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(HeadlessDevToolsDelegate);
};

HeadlessDevToolsDelegate::HeadlessDevToolsDelegate() {}

HeadlessDevToolsDelegate::~HeadlessDevToolsDelegate() {}

std::string HeadlessDevToolsDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_HEADLESS_LIB_DEVTOOLS_DISCOVERY_PAGE).as_string();
}

std::string HeadlessDevToolsDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

}  // namespace

std::unique_ptr<DevToolsHttpHandler> CreateLocalDevToolsHttpHandler(
    HeadlessBrowser::Options* options) {
  const net::IPEndPoint& endpoint = options->devtools_endpoint;
  std::unique_ptr<content::DevToolsSocketFactory> socket_factory(
      new TCPServerSocketFactory(endpoint));
  return base::MakeUnique<DevToolsHttpHandler>(
      std::move(socket_factory), std::string(), new HeadlessDevToolsDelegate(),
      options->user_data_dir,  // TODO(altimin): Figure a proper value for this.
      base::FilePath(), std::string(), options->user_agent);
}

}  // namespace headless
