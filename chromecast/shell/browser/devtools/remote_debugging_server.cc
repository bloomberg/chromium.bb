// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/browser/devtools/remote_debugging_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "chromecast/common/chromecast_config.h"
#include "chromecast/common/pref_names.h"
#include "chromecast/shell/browser/devtools/cast_dev_tools_delegate.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/user_agent.h"
#include "net/socket/tcp_listen_socket.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_socket_posix.h"
#endif  // defined(OS_ANDROID)

namespace chromecast {
namespace shell {

namespace {

#if defined(OS_ANDROID)
const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/devtools.html";
#endif  // defined(OS_ANDROID)
const int kDefaultRemoteDebuggingPort = 9222;

net::StreamListenSocketFactory* CreateSocketFactory(int port) {
#if defined(OS_ANDROID)
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  std::string socket_name = "content_shell_devtools_remote";
  if (command_line->HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line->GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return new net::UnixDomainSocketWithAbstractNamespaceFactory(
      socket_name, "", base::Bind(&content::CanUserConnectToDevTools));
#else
  return new net::TCPListenSocketFactory("0.0.0.0", port);
#endif  // defined(OS_ANDROID)
}

std::string GetFrontendUrl() {
#if defined(OS_ANDROID)
  return base::StringPrintf(kFrontEndURL, content::GetWebKitRevision().c_str());
#else
  return std::string();
#endif  // defined(OS_ANDROID)
}

}  // namespace

RemoteDebuggingServer::RemoteDebuggingServer()
    : devtools_http_handler_(NULL),
      port_(0) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));
  pref_port_.Init(prefs::kRemoteDebuggingPort,
                  ChromecastConfig::GetInstance()->pref_service(),
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
  int new_port = *pref_port_;
  if (new_port < 0) {
    new_port = 0;
  }
  VLOG(1) << "OnPortChanged called: old_port=" << port_
          << ", new_port=" << new_port;

  if (new_port == port_) {
    VLOG(1) << "Port has not been changed. Ignore silently.";
    return;
  }

  if (devtools_http_handler_) {
    LOG(INFO) << "Stop old devtools: port=" << port_;
    // Note: Stop destroys devtools_http_handler_.
    devtools_http_handler_->Stop();
    devtools_http_handler_ = NULL;
  }

  port_ = new_port;
  if (port_ > 0) {
    devtools_http_handler_ = content::DevToolsHttpHandler::Start(
        CreateSocketFactory(port_),
        GetFrontendUrl(),
        new CastDevToolsDelegate(),
        base::FilePath());
    LOG(INFO) << "Devtools started: port=" << port_;
  }
}

}  // namespace shell
}  // namespace chromecast
