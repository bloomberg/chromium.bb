// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/shell_content_browser_client.h"

#include "apps/shell/shell_browser_context.h"
#include "apps/shell/shell_browser_main_parts.h"
#include "content/shell/browser/shell_browser_context.h"

namespace apps {

ShellContentBrowserClient::ShellContentBrowserClient()
    : browser_main_parts_(NULL) {
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
}

content::BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  browser_main_parts_ = new ShellBrowserMainParts(parameters);
  return browser_main_parts_;
}

net::URLRequestContextGetter*
ShellContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers) {
  // TODO(jamescook): Should this be an off-the-record context?
  return browser_main_parts_->browser_context()->CreateRequestContext(
      protocol_handlers);
}

}  // namespace apps
