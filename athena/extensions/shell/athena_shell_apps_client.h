// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_APPS_CLIENT_H_
#define ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_APPS_CLIENT_H_

#include "athena/extensions/athena_apps_client_base.h"
#include "base/macros.h"

namespace athena {

class AthenaShellAppsClient : public AthenaAppsClientBase {
 public:
  AthenaShellAppsClient(content::BrowserContext* context);
  virtual ~AthenaShellAppsClient();

 private:
  // extensions::AppsClient
  virtual std::vector<content::BrowserContext*> GetLoadedBrowserContexts()
      OVERRIDE;
  virtual extensions::AppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE;
  virtual void OpenDevToolsWindow(content::WebContents* web_contents,
                                  const base::Closure& callback) OVERRIDE;
  virtual bool IsCurrentChannelOlderThanDev() OVERRIDE;

  content::BrowserContext* context_;

  DISALLOW_COPY_AND_ASSIGN(AthenaShellAppsClient);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_SHELL_ATHENA_SHELL_APPS_CLIENT_H_
