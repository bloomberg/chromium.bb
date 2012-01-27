// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/shell/shell_devtools_delegate.h"

#include <algorithm>

#include "content/public/browser/devtools_http_handler.h"
#include "content/public/browser/web_contents.h"
#include "grit/shell_resources.h"
#include "net/url_request/url_request_context_getter.h"
#include "ui/base/resource/resource_bundle.h"

namespace content {

ShellDevToolsDelegate::ShellDevToolsDelegate(
    int port,
    net::URLRequestContextGetter* context_getter)
    : content::WebContentsObserver(),
      context_getter_(context_getter) {
  devtools_http_handler_ = DevToolsHttpHandler::Start(
      "127.0.0.1",
      port,
      "",
      this);
}

ShellDevToolsDelegate::~ShellDevToolsDelegate() {
}

void ShellDevToolsDelegate::Stop() {
  // The call below destroys this.
  devtools_http_handler_->Stop();
}

void ShellDevToolsDelegate::WebContentsDestroyed(WebContents* contents) {
  std::remove(web_contents_list_.begin(), web_contents_list_.end(), contents);
}

DevToolsHttpHandlerDelegate::InspectableTabs
ShellDevToolsDelegate::GetInspectableTabs() {
  DevToolsHttpHandlerDelegate::InspectableTabs tabs;
  for (std::vector<WebContents*>::iterator it = web_contents_list_.begin();
       it != web_contents_list_.end(); ++it) {
    tabs.push_back(*it);
  }
  return tabs;
}

std::string ShellDevToolsDelegate::GetDiscoveryPageHTML() {
  return ResourceBundle::GetSharedInstance().GetRawDataResource(
      IDR_CONTENT_SHELL_DEVTOOLS_DISCOVERY_PAGE).as_string();
}

net::URLRequestContext*
ShellDevToolsDelegate::GetURLRequestContext() {
  return context_getter_->GetURLRequestContext();
}

bool ShellDevToolsDelegate::BundlesFrontendResources() {
  return true;
}

std::string ShellDevToolsDelegate::GetFrontendResourcesBaseURL() {
  return "";
}

void ShellDevToolsDelegate::AddWebContents(WebContents* web_contents) {
  web_contents_list_.push_back(web_contents);
  Observe(web_contents);
}

}  // namespace content
