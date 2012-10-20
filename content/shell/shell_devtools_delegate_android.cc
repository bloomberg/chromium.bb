// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_devtools_delegate.h"

#include "base/bind.h"
#include "base/stringprintf.h"
#include "content/public/browser/android/devtools_auth.h"
#include "content/public/browser/devtools_http_handler.h"
#include "grit/shell_resources.h"
#include "net/base/unix_domain_socket_posix.h"
#include "ui/base/layout.h"
#include "ui/base/resource/resource_bundle.h"

namespace {

const char kSocketName[] = "content_shell_devtools_remote";

}

namespace content {

ShellDevToolsDelegate::ShellDevToolsDelegate(BrowserContext* browser_context,
                                             int port)
    : browser_context_(browser_context) {
  devtools_http_handler_ = DevToolsHttpHandler::Start(
      new net::UnixDomainSocketWithAbstractNamespaceFactory(
          kSocketName,
          base::Bind(&CanUserConnectToDevTools)),
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
  return NULL;
}

}  // namespace content
