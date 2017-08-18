// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/remote_debugging_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/cast_devtools_manager_delegate.h"
#include "chromecast/common/cast_content_client.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_socket_factory.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/net_errors.h"
#include "net/log/net_log_source.h"
#include "net/socket/tcp_server_socket.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#endif  // defined(OS_ANDROID)

namespace chromecast {
namespace shell {

namespace {

const char kFrontEndURL[] =
    "https://chrome-devtools-frontend.appspot.com/serve_rev/%s/inspector.html";
const uint16_t kDefaultRemoteDebuggingPort = 9222;

const int kBackLog = 10;

#if defined(OS_ANDROID)
class UnixDomainServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    return nullptr;
  }

  std::string socket_name_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};
#else
class TCPServerSocketFactory : public content::DevToolsSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}

 private:
  // content::DevToolsSocketFactory.
  std::unique_ptr<net::ServerSocket> CreateForHttpServer() override {
    std::unique_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLogSource()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return std::unique_ptr<net::ServerSocket>();

    return socket;
  }

  std::unique_ptr<net::ServerSocket> CreateForTethering(
      std::string* name) override {
    return nullptr;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};
#endif

std::unique_ptr<content::DevToolsSocketFactory> CreateSocketFactory(
    uint16_t port) {
#if defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string socket_name = "cast_shell_devtools_remote";
  if (command_line->HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line->GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new UnixDomainServerSocketFactory(socket_name));
#else
  return std::unique_ptr<content::DevToolsSocketFactory>(
      new TCPServerSocketFactory("0.0.0.0", port));
#endif
}

std::string GetFrontendUrl() {
  return base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str());
}

uint16_t GetPort() {
  std::string port_str =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kRemoteDebuggingPort);

  if (port_str.empty())
    return kDefaultRemoteDebuggingPort;

  int port;
  if (base::StringToInt(port_str, &port))
    return port;

  return kDefaultRemoteDebuggingPort;
}

}  // namespace

RemoteDebuggingServer::RemoteDebuggingServer(bool start_immediately)
    : port_(GetPort()), is_started_(false) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  if (is_started_) {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
  }
}

void RemoteDebuggingServer::EnableWebContentsForDebugging(
    content::WebContents* web_contents) {
  DCHECK(web_contents);

  if (!is_started_) {
    content::DevToolsAgentHost::StartRemoteDebuggingServer(
        CreateSocketFactory(port_), GetFrontendUrl(), base::FilePath(),
        base::FilePath());
    LOG(INFO) << "Devtools started: port=" << port_;
    is_started_ = true;
  }

  auto* dev_tools_delegate =
      chromecast::shell::CastDevToolsManagerDelegate::GetInstance();
  DCHECK(dev_tools_delegate);
  dev_tools_delegate->EnableWebContentsForDebugging(web_contents);
}

void RemoteDebuggingServer::DisableWebContentsForDebugging(
    content::WebContents* web_contents) {
  DCHECK(web_contents);
  auto* dev_tools_delegate =
      chromecast::shell::CastDevToolsManagerDelegate::GetInstance();
  DCHECK(dev_tools_delegate);
  dev_tools_delegate->DisableWebContentsForDebugging(web_contents);

  if (is_started_ && !dev_tools_delegate->HasEnabledWebContents()) {
    content::DevToolsAgentHost::StopRemoteDebuggingServer();
    is_started_ = false;
  }
}

}  // namespace shell
}  // namespace chromecast
