// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/browser/shell_devtools_manager_delegate.h"

#include <stdint.h>

#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "components/devtools_discovery/basic_target_descriptor.h"
#include "components/devtools_discovery/devtools_discovery_manager.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/devtools_agent_host.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "content/public/browser/favicon_status.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/user_agent.h"
#include "content/shell/browser/shell.h"
#include "content/shell/common/shell_content_client.h"
#include "grit/shell_resources.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

#if defined(OS_ANDROID)
#include "content/public/browser/android/devtools_auth.h"
#include "net/socket/unix_domain_server_socket_posix.h"
#endif

using devtools_http_handler::DevToolsHttpHandler;

namespace content {

namespace {

#if defined(OS_ANDROID)
const char kFrontEndURL[] =
    "http://chrome-devtools-frontend.appspot.com/serve_rev/%s/inspector.html";
#endif

const int kBackLog = 10;

#if defined(OS_ANDROID)
class UnixDomainServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  explicit UnixDomainServerSocketFactory(const std::string& socket_name)
      : socket_name_(socket_name) {}

 private:
  // DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::UnixDomainServerSocket> socket(
        new net::UnixDomainServerSocket(base::Bind(&CanUserConnectToDevTools),
                                        true /* use_abstract_namespace */));
    if (socket->BindAndListen(socket_name_, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return std::move(socket);
  }

  std::string socket_name_;

  DISALLOW_COPY_AND_ASSIGN(UnixDomainServerSocketFactory);
};
#else
class TCPServerSocketFactory
    : public DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address), port_(port) {}

 private:
  // DevToolsHttpHandler::ServerSocketFactory.
  scoped_ptr<net::ServerSocket> CreateForHttpServer() override {
    scoped_ptr<net::ServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->ListenWithAddressAndPort(address_, port_, kBackLog) != net::OK)
      return scoped_ptr<net::ServerSocket>();

    return socket;
  }

  std::string address_;
  uint16_t port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};
#endif

scoped_ptr<DevToolsHttpHandler::ServerSocketFactory>
CreateSocketFactory() {
  const base::CommandLine& command_line =
      *base::CommandLine::ForCurrentProcess();
#if defined(OS_ANDROID)
  std::string socket_name = "content_shell_devtools_remote";
  if (command_line.HasSwitch(switches::kRemoteDebuggingSocketName)) {
    socket_name = command_line.GetSwitchValueASCII(
        switches::kRemoteDebuggingSocketName);
  }
  return scoped_ptr<DevToolsHttpHandler::ServerSocketFactory>(
      new UnixDomainServerSocketFactory(socket_name));
#else
  // See if the user specified a port on the command line (useful for
  // automation). If not, use an ephemeral port by specifying 0.
  uint16_t port = 0;
  if (command_line.HasSwitch(switches::kRemoteDebuggingPort)) {
    int temp_port;
    std::string port_str =
        command_line.GetSwitchValueASCII(switches::kRemoteDebuggingPort);
    if (base::StringToInt(port_str, &temp_port) &&
        temp_port >= 0 && temp_port < 65535) {
      port = static_cast<uint16_t>(temp_port);
    } else {
      DLOG(WARNING) << "Invalid http debugger port number " << temp_port;
    }
  }
  return scoped_ptr<DevToolsHttpHandler::ServerSocketFactory>(
      new TCPServerSocketFactory("127.0.0.1", port));
#endif
}

scoped_ptr<devtools_discovery::DevToolsTargetDescriptor>
CreateNewShellTarget(BrowserContext* browser_context, const GURL& url) {
  Shell* shell = Shell::CreateNewWindow(browser_context,
                                        url,
                                        nullptr,
                                        gfx::Size());
  return make_scoped_ptr(new devtools_discovery::BasicTargetDescriptor(
      DevToolsAgentHost::GetOrCreateFor(shell->web_contents())));
}

// ShellDevToolsDelegate ----------------------------------------------------

class ShellDevToolsDelegate :
    public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  explicit ShellDevToolsDelegate(BrowserContext* browser_context);
  ~ShellDevToolsDelegate() override;

  // devtools_http_handler::DevToolsHttpHandlerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;
  std::string GetPageThumbnailData(const GURL& url) override;
  DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellDevToolsDelegate);
};

ShellDevToolsDelegate::ShellDevToolsDelegate(BrowserContext* browser_context) {
  devtools_discovery::DevToolsDiscoveryManager::GetInstance()->
      SetCreateCallback(base::Bind(&CreateNewShellTarget,
                                   base::Unretained(browser_context)));
}

ShellDevToolsDelegate::~ShellDevToolsDelegate() {
  devtools_discovery::DevToolsDiscoveryManager::GetInstance()->
      SetCreateCallback(
          devtools_discovery::DevToolsDiscoveryManager::CreateCallback());
}

std::string ShellDevToolsDelegate::GetDiscoveryPageHTML() {
#if defined(OS_ANDROID)
  return std::string();
#else
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CONTENT_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
#endif
}

std::string ShellDevToolsDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

std::string ShellDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return std::string();
}

DevToolsExternalAgentProxyDelegate*
ShellDevToolsDelegate::HandleWebSocketConnection(const std::string& path) {
  return nullptr;
}

}  // namespace

// ShellDevToolsManagerDelegate ----------------------------------------------

// static
DevToolsHttpHandler*
ShellDevToolsManagerDelegate::CreateHttpHandler(
    BrowserContext* browser_context) {
  std::string frontend_url;
#if defined(OS_ANDROID)
  frontend_url = base::StringPrintf(kFrontEndURL, GetWebKitRevision().c_str());
#endif
  return new DevToolsHttpHandler(
      CreateSocketFactory(),
      frontend_url,
      new ShellDevToolsDelegate(browser_context),
      browser_context->GetPath(),
      base::FilePath(),
      std::string(),
      GetShellUserAgent());
}

ShellDevToolsManagerDelegate::ShellDevToolsManagerDelegate() {
}

ShellDevToolsManagerDelegate::~ShellDevToolsManagerDelegate() {
}

base::DictionaryValue* ShellDevToolsManagerDelegate::HandleCommand(
    DevToolsAgentHost* agent_host,
    base::DictionaryValue* command) {
  return NULL;
}

}  // namespace content
