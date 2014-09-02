// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ATHENA_EXTENSIONS_CHROME_ATHENA_APPS_CLIENT_H_
#define ATHENA_EXTENSIONS_CHROME_ATHENA_APPS_CLIENT_H_

#include "extensions/browser/app_window/apps_client.h"
#include "base/macros.h"

namespace athena {

// The implementation of AppsClient for Athena.
class AthenaAppsClient : public extensions::AppsClient {
 public:
  AthenaAppsClient();
  virtual ~AthenaAppsClient();

 private:
  // extensions::AppsClient
  virtual std::vector<content::BrowserContext*> GetLoadedBrowserContexts()
      OVERRIDE;
  virtual extensions::AppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE;
  virtual extensions::NativeAppWindow* CreateNativeAppWindow(
      extensions::AppWindow* window,
      const extensions::AppWindow::CreateParams& params) OVERRIDE;
  virtual void IncrementKeepAliveCount() OVERRIDE;
  virtual void DecrementKeepAliveCount() OVERRIDE;
  virtual void OpenDevToolsWindow(content::WebContents* web_contents,
                                  const base::Closure& callback) OVERRIDE;
  virtual bool IsCurrentChannelOlderThanDev() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaAppsClient);
};

}  // namespace athena

#endif  // ATHENA_EXTENSIONS_CHROME_ATHENA_APPS_CLIENT_H_
