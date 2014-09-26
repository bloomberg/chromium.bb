// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CLIENT_H_
#define EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CLIENT_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/browser/app_window/app_window_client.h"

namespace extensions {

// app_shell's AppWindowClient implementation.
class ShellAppWindowClient : public AppWindowClient {
 public:
  ShellAppWindowClient();
  virtual ~ShellAppWindowClient();

  // AppWindowClient overrides:
  virtual AppWindow* CreateAppWindow(content::BrowserContext* context,
                                     const Extension* extension) OVERRIDE;
  virtual NativeAppWindow* CreateNativeAppWindow(
      AppWindow* window,
      const AppWindow::CreateParams& params) OVERRIDE;
  virtual void OpenDevToolsWindow(content::WebContents* web_contents,
                                  const base::Closure& callback) OVERRIDE;
  virtual bool IsCurrentChannelOlderThanDev() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(ShellAppWindowClient);
};

}  // namespace extensions

#endif  // EXTENSIONS_SHELL_BROWSER_SHELL_APP_WINDOW_CLIENT_H_
