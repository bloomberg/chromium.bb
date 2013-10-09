// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_APP_SHELL_CONTENT_BROWSER_CLIENT_H_
#define APPS_SHELL_APP_SHELL_CONTENT_BROWSER_CLIENT_H_

#include "base/compiler_specific.h"
#include "content/public/browser/content_browser_client.h"

namespace apps {
class AppShellBrowserMainParts;

class AppShellContentBrowserClient : public content::ContentBrowserClient {
 public:
  AppShellContentBrowserClient();
  virtual ~AppShellContentBrowserClient();

  // content::ContentBrowserClient overrides.
  virtual content::BrowserMainParts* CreateBrowserMainParts(
      const content::MainFunctionParams& parameters) OVERRIDE;
  virtual net::URLRequestContextGetter* CreateRequestContext(
      content::BrowserContext* browser_context,
      content::ProtocolHandlerMap* protocol_handlers) OVERRIDE;
  // TODO(jamescook): Quota management?
  // TODO(jamescook): Speech recognition?

 private:
  // Owned by content::BrowserMainLoop.
  AppShellBrowserMainParts* browser_main_parts_;

  DISALLOW_COPY_AND_ASSIGN(AppShellContentBrowserClient);
};

}  // namespace apps

#endif  // APPS_SHELL_APP_SHELL_CONTENT_BROWSER_CLIENT_H_
