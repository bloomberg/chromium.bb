// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/devtools/remote_debugging_server.h"

#include <utility>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/history/top_sites_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/chrome_paths.h"
#include "components/devtools_http_handler/devtools_http_handler.h"
#include "components/devtools_http_handler/devtools_http_handler_delegate.h"
#include "components/history/core/browser/top_sites.h"
#include "components/version_info/version_info.h"
#include "content/public/browser/devtools_frontend_host.h"
#include "grit/browser_resources.h"
#include "net/base/net_errors.h"
#include "net/socket/tcp_server_socket.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

base::LazyInstance<bool>::Leaky g_tethering_enabled = LAZY_INSTANCE_INITIALIZER;

const uint16_t kMinTetheringPort = 9333;
const uint16_t kMaxTetheringPort = 9444;
const int kBackLog = 10;

class TCPServerSocketFactory
    : public devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory {
 public:
  TCPServerSocketFactory(const std::string& address, uint16_t port)
      : address_(address),
        port_(port),
        last_tethering_port_(kMinTetheringPort) {}

 private:
  // devtools_http_handler::DevToolsHttpHandler::ServerSocketFactory.
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
    uint16_t port = ++last_tethering_port_;
    *name = base::UintToString(port);
    scoped_ptr<net::TCPServerSocket> socket(
        new net::TCPServerSocket(nullptr, net::NetLog::Source()));
    if (socket->ListenWithAddressAndPort("127.0.0.1", port, kBackLog) !=
        net::OK) {
      return scoped_ptr<net::ServerSocket>();
    }
    return std::move(socket);
  }

  std::string address_;
  uint16_t port_;
  uint16_t last_tethering_port_;

  DISALLOW_COPY_AND_ASSIGN(TCPServerSocketFactory);
};

class ChromeDevToolsHttpHandlerDelegate
    : public devtools_http_handler::DevToolsHttpHandlerDelegate {
 public:
  ChromeDevToolsHttpHandlerDelegate();
  ~ChromeDevToolsHttpHandlerDelegate() override;

  // devtools_http_handler::DevToolsHttpHandlerDelegate implementation.
  std::string GetDiscoveryPageHTML() override;
  std::string GetFrontendResource(const std::string& path) override;
  std::string GetPageThumbnailData(const GURL& url) override;
  content::DevToolsExternalAgentProxyDelegate*
      HandleWebSocketConnection(const std::string& path) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ChromeDevToolsHttpHandlerDelegate);
};

ChromeDevToolsHttpHandlerDelegate::ChromeDevToolsHttpHandlerDelegate() {
}

ChromeDevToolsHttpHandlerDelegate::~ChromeDevToolsHttpHandlerDelegate() {
}

std::string ChromeDevToolsHttpHandlerDelegate::GetDiscoveryPageHTML() {
  std::set<Profile*> profiles;
  for (auto* browser : *BrowserList::GetInstance())
    profiles.insert(browser->profile());

  for (std::set<Profile*>::iterator it = profiles.begin();
       it != profiles.end(); ++it) {
    scoped_refptr<history::TopSites> ts = TopSitesFactory::GetForProfile(*it);
    if (ts) {
      // TopSites updates itself after a delay. Ask TopSites to update itself
      // when we're about to show the remote debugging landing page.
      ts->SyncWithHistory();
    }
  }
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_DEVTOOLS_DISCOVERY_PAGE_HTML).as_string();
}

std::string ChromeDevToolsHttpHandlerDelegate::GetFrontendResource(
    const std::string& path) {
  return content::DevToolsFrontendHost::GetFrontendResource(path).as_string();
}

std::string ChromeDevToolsHttpHandlerDelegate::GetPageThumbnailData(
    const GURL& url) {
  for (auto* browser : *BrowserList::GetInstance()) {
    Profile* profile = browser->profile();
    scoped_refptr<history::TopSites> top_sites =
        TopSitesFactory::GetForProfile(profile);
    if (!top_sites)
      continue;
    scoped_refptr<base::RefCountedMemory> data;
    if (top_sites->GetPageThumbnail(url, false, &data))
      return std::string(data->front_as<char>(), data->size());
  }
  return std::string();
}

content::DevToolsExternalAgentProxyDelegate*
ChromeDevToolsHttpHandlerDelegate::HandleWebSocketConnection(
    const std::string& path) {
  return DevToolsWindow::CreateWebSocketAPIChannel(path);
}

}  // namespace

// static
void RemoteDebuggingServer::EnableTetheringForDebug() {
  g_tethering_enabled.Get() = true;
}

RemoteDebuggingServer::RemoteDebuggingServer(const std::string& ip,
                                             uint16_t port) {
  base::FilePath output_dir;
  if (!port) {
    // The client requested an ephemeral port. Must write the selected
    // port to a well-known location in the profile directory to
    // bootstrap the connection process.
    bool result = PathService::Get(chrome::DIR_USER_DATA, &output_dir);
    DCHECK(result);
  }

  base::FilePath debug_frontend_dir;
#if defined(DEBUG_DEVTOOLS)
  PathService::Get(chrome::DIR_INSPECTOR, &debug_frontend_dir);
#endif

  devtools_http_handler_.reset(new devtools_http_handler::DevToolsHttpHandler(
      make_scoped_ptr(new TCPServerSocketFactory(ip, port)),
      std::string(),
      new ChromeDevToolsHttpHandlerDelegate(),
      output_dir,
      debug_frontend_dir,
      version_info::GetProductNameAndVersionForUserAgent(),
      ::GetUserAgent()));
}

RemoteDebuggingServer::~RemoteDebuggingServer() {
  // Ensure Profile is alive, because the whole DevTools subsystem
  // accesses it during shutdown.
  DCHECK(g_browser_process->profile_manager());
}
