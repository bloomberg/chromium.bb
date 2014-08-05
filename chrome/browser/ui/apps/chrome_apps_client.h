// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_APPS_CHROME_APPS_CLIENT_H_
#define CHROME_BROWSER_UI_APPS_CHROME_APPS_CLIENT_H_

#include "apps/ui/apps_client.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"

template <typename T>
struct DefaultSingletonTraits;

// The implementation of AppsClient for Chrome.
class ChromeAppsClient : public apps::AppsClient {
 public:
  ChromeAppsClient();
  virtual ~ChromeAppsClient();

  // Get the LazyInstance for ChromeAppsClient.
  static ChromeAppsClient* GetInstance();

 private:
  friend struct DefaultSingletonTraits<ChromeAppsClient>;

  // apps::AppsClient
  virtual std::vector<content::BrowserContext*> GetLoadedBrowserContexts()
      OVERRIDE;
  virtual apps::AppWindow* CreateAppWindow(
      content::BrowserContext* context,
      const extensions::Extension* extension) OVERRIDE;
  virtual apps::NativeAppWindow* CreateNativeAppWindow(
      apps::AppWindow* window,
      const apps::AppWindow::CreateParams& params) OVERRIDE;
  virtual void IncrementKeepAliveCount() OVERRIDE;
  virtual void DecrementKeepAliveCount() OVERRIDE;

  // Implemented in platform specific code.
  static apps::NativeAppWindow* CreateNativeAppWindowImpl(
      apps::AppWindow* window,
      const apps::AppWindow::CreateParams& params);

  DISALLOW_COPY_AND_ASSIGN(ChromeAppsClient);
};

#endif  // CHROME_BROWSER_UI_APPS_CHROME_APPS_CLIENT_H_
