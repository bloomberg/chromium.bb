// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_devtools_delegate.h"

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/shell/shell.h"
#include "grit/shell_resources.h"
#include "net/socket/tcp_listen_socket.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_socket_posix.h"
#endif

namespace {

net::StreamListenSocketFactory* CreateSocketFactory() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
#if defined(OS_ANDROID)
  std::string socket_name = "content_shell_devtools_remote";
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return new net::UnixDomainSocketWithAbstractNamespaceFactory(
      socket_name, base::Bind(&content::CanUserConnectToDevTools));
#else
  // See if the user specified a port on the command line (useful for
  // automation). If not, use an ephemeral port by specifying 0.
  int port = 0;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int temp_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) &&
        temp_port > 0 && temp_port < 65535) {
      port = temp_port;
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
  }
  return new net::TCPListenSocketFactory("127.0.0.1", port);
#endif
}
}  // namespace

namespace content {

ShellDevToolsDelegate::ShellDevToolsDelegate(BrowserContext* browser_context)
    : browser_context_(browser_context) {
  devtools_http_handler_ =
      DevToolsHttpHandler::Start(CreateSocketFactory(), std::string(), this);
}

ShellDevToolsDelegate::~ShellDevToolsDelegate() {
}

void ShellDevToolsDelegate::Stop() {
  // The call below destroys this.
  devtools_http_handler_->Stop();
}

std::string ShellDevToolsDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CONTENT_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
}

bool ShellDevToolsDelegate::BundlesFrontendResources() {
  return true;
}

base::FilePath ShellDevToolsDelegate::GetDebugFrontendDir() {
  return base::FilePath();
}

std::string ShellDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return std::string();
}

RenderViewHost* ShellDevToolsDelegate::CreateNewTarget() {
  Shell* shell = Shell::CreateNewWindow(browser_context_,
                                        GURL(chrome::kAboutBlankURL),
                                        NULL,
                                        MSG_ROUTING_NONE,
                                        gfx::Size());
  return shell->web_contents()->GetRenderViewHost();
}

DevToolsHttpHandlerDelegate::TargetType
ShellDevToolsDelegate::GetTargetType(RenderViewHost*) {
  return kTargetTypeTab;
}

std::string ShellDevToolsDelegate::GetViewDescription(
    content::RenderViewHost*) {
  return std::string();
}

scoped_refptr<net::StreamListenSocket>
ShellDevToolsDelegate::CreateSocketForTethering(
    net::StreamListenSocket::Delegate* delegate,
    std::string* name) {
  return NULL;
}

}  // namespace content
