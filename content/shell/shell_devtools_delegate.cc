// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_devtools_delegate.h"

#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/url_constants.h"
#include "content/shell/shell.h"
#include "grit/shell_resources.h"
#include "net/base/tcp_listen_socket.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace content {

ShellDevToolsDelegate::ShellDevToolsDelegate(BrowserContext* browser_context,
                                             int port)
    : browser_context_(browser_context) {
  devtools_http_handler_ = DevToolsHttpHandler::Start(
      new net::TCPListenSocketFactory("127.0.0.1", port),
      "",
      this);
}

ShellDevToolsDelegate::~ShellDevToolsDelegate() {
}

void ShellDevToolsDelegate::Stop() {
  // The call below destroys this.
  devtools_http_handler_->Stop();
}

std::string ShellDevToolsDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CONTENT_SHELL_DEVTOOLS_DISCOVERY_PAGE,
      ui::SCALE_FACTOR_NONE).as_string();
}

bool ShellDevToolsDelegate::BundlesFrontendResources() {
  return true;
}

FilePath ShellDevToolsDelegate::GetDebugFrontendDir() {
  return FilePath();
}

std::string ShellDevToolsDelegate::GetPageThumbnailData(const GURL& url) {
  return "";
}

RenderViewHost* ShellDevToolsDelegate::CreateNewTarget() {
  Shell* shell = Shell::CreateNewWindow(browser_context_,
                                        GURL(chrome::kAboutBlankURL),
                                        NULL,
                                        MSG_ROUTING_NONE,
                                        NULL);
  return shell->web_contents()->GetRenderViewHost();
}

}  // namespace content
