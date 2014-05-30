// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell/content_client/shell_content_browser_client.h"

#include "ash/shell/content_client/shell_browser_main_parts.h"
#include "content/shell/browser/shell_browser_context.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace ash {
namespace shell {

ShellContentBrowserClient::ShellContentBrowserClient()
    : shell_browser_main_parts_(NULL) {
}

ShellContentBrowserClient::~ShellContentBrowserClient() {
}

content::BrowserMainParts* ShellContentBrowserClient::CreateBrowserMainParts(
    const content::MainFunctionParams& parameters) {
  shell_browser_main_parts_ =  new ShellBrowserMainParts(parameters);
  return shell_browser_main_parts_;
}

net::URLRequestContextGetter* ShellContentBrowserClient::CreateRequestContext(
    content::BrowserContext* content_browser_context,
    content::ProtocolHandlerMap* protocol_handlers,
    content::URLRequestInterceptorScopedVector request_interceptors) {
  content::ShellBrowserContext* shell_context =
      static_cast<content::ShellBrowserContext*>(content_browser_context);
  return shell_context->CreateRequestContext(protocol_handlers,
                                             request_interceptors.Pass());
}

content::ShellBrowserContext* ShellContentBrowserClient::browser_context() {
  return shell_browser_main_parts_->browser_context();
}

}  // namespace examples
}  // namespace views
