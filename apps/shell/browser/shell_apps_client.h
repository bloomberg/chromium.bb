// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_SHELL_BROWSER_SHELL_APPS_CLIENT_H_
#define APPS_SHELL_BROWSER_SHELL_APPS_CLIENT_H_

#include "apps/apps_client.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

namespace content {
class BrowserContext;
}

namespace apps {

// The implementation of AppsClient for app_shell.
class ShellAppsClient : public AppsClient {
 public:
  // For app_shell only a single browser context is supported.
  explicit ShellAppsClient(content::BrowserContext* browser_context);
  virtual ~ShellAppsClient();

 private:
  // apps::AppsClient implementation:
  virtual std::vector<content::BrowserContext*> GetLoadedBrowserContexts()
      OVERRIDE;
  virtual AppWindow* CreateAppWindow(content::BrowserContext* context,
                                     const extensions::Extension* extension)
      OVERRIDE;
  virtual void IncrementKeepAliveCount() OVERRIDE;
  virtual void DecrementKeepAliveCount() OVERRIDE;

  content::BrowserContext* browser_context_;

  DISALLOW_COPY_AND_ASSIGN(ShellAppsClient);
};

}  // namespace apps

#endif  // APPS_SHELL_BROWSER_SHELL_APPS_CLIENT_H_
