// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APPS_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APPS_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/app_window/apps_client.h"

namespace extensions {

// app_shell's AppsClient implementation.
class ShellAppsClient : public AppsClient {
 public:
  ShellAppsClient();
  virtual ~ShellAppsClient();

  // AppsClient overrides:
  virtual std::vector<content::BrowserContext*> GetLoadedBrowserContexts()
      OVERRIDE;
  virtual AppWindow* CreateAppWindow(content::BrowserContext* context,
                                     const Extension* extension) OVERRIDE;
  virtual NativeAppWindow* CreateNativeAppWindow(
      AppWindow* window,
      const AppWindow::CreateParams& params) OVERRIDE;
  virtual void IncrementKeepAliveCount() OVERRIDE;
  virtual void DecrementKeepAliveCount() OVERRIDE;
  virtual void OpenDevToolsWindow(content::WebContents* web_contents,
                                  const base::Closure& callback) OVERRIDE;
  virtual bool IsCurrentChannelOlderThanDev() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellAppsClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APPS_CLIENT_H_
