// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/devtools/remote_debugging_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chromecast/browser/cast_browser_process.h"
#include "chromecast/browser/devtools/cast_dev_tools_delegate.h"
#include "chromecast/common/pref_names.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/base/net_errors.h"
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
const uint16 kDefaultRemoteDebuggingPort = 9222;

const int kBackLog = 10;

#if defined(OS_ANDROID)
class UnixDomainServerSocketFactory
    : public content::DevToolsHttpHandler::ServerSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

 private:
  // content::DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::ServerSocket> socket(
        new net::UnixDomainServerSocket(
            base::Bind(&content::CanUserConnectToDevTools),
            true /* use_abstract_namespace */));
    if (socket->ListenWithAddressAndPort(socket_name_, 0, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return socket;
  }

  std::string socket_name_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};
#else
class TCPServerSocketFactory
    : public content::DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16 port)
      : address_(address), port_(port) {
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

  std::string address_;
  uint16 port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};
#endif

scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory>
CreateSocketFactory(uint16 port) {
#if defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string socket_name = "cast_shell_devtools_remote";
  if (command_line->HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line->GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory>(
      new UnixDomainServerSocketFactory(socket_name));
#else
  return scoped_ptr<content::DevToolsHttpHandler::ServerSocketFactory>(
      new TCPServerSocketFactory("0.0.0.0", port));
#endif
}

std::string GetFrontendUrl() {
  return base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str());
}

}  // namespace

RemoteDebuggingServer::RemoteDebuggingServer() : port_(0) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  pref_port_.Init(prefs::kRemoteDebuggingPort,
                  CastBrowserProcess::GetInstance()->pref_service(),
                  base::Bind(&RemoteDebuggingServer::OnPortChanged,
                             base::Unretained(this)));

  // Starts new dev tools, clearing port number saved in config.
  // Remote debugging in production must be triggered only by config server.
  pref_port_.SetValue(ShouldStartImmediately() ?
                      kDefaultRemoteDebuggingPort : 0);
  OnPortChanged();
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  pref_port_.SetValue(0);
  OnPortChanged();
}

void RemoteDebuggingServer::OnPortChanged() {
  uint16 new_port = static_cast<uint16>(std::max(*pref_port_, 0));
  VLOG(1) << "OnPortChanged called: old_port=" << port_
          << ", new_port=" << new_port;

  if (new_port == port_) {
    VLOG(1) << "Port has not been changed. Ignore silently.";
    return;
  }

  if (devtools_http_handler_) {
    LOG(INFO) << "Stop old devtools: port=" << port_;
    devtools_http_handler_.reset();
  }

  port_ = new_port;
  if (port_ > 0) {
    devtools_http_handler_.reset(content::DevToolsHttpHandler::Start(
        CreateSocketFactory(port_),
        GetFrontendUrl(),
        new CastDevToolsDelegate(),
        base::FilePath()));
    LOG(INFO) << "Devtools started: port=" << port_;
  }
}

}  // namespace shell
}  // namespace chromecast
