// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/app_shell_content_browser_client.h"

#include "apps/shell/app_shell_browser_main_parts.h"
#include "content/shell/browser/shell_browser_context.h"

namespace apps {

AppShellContentBrowserClient::AppShellContentBrowserClient()
    : browser_main_parts_(NULL) {
}

AppShellContentBrowserClient::~AppShellContentBrowserClient() {
}

content::BrowserMainParts* AppShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new AppShellBrowserMainParts(parameters);
  return browser_main_parts_;
}

net::URLRequestContextGetter*
AppShellContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers) {
  // TODO(jamescook): Should this be an off-the-record context?
  content::ShellBrowserContext* shell_browser_context =
      browser_main_parts_->browser_context();
  return shell_browser_context->CreateRequestContext(protocol_handlers);
}

}  // namespace apps
