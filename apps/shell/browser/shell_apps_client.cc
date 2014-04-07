// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "apps/shell/browser/shell_apps_client.h"

#include "apps/app_window.h"
#include "apps/shell/browser/shell_app_window_delegate.h"

using content::BrowserContext;

namespace apps {

ShellAppsClient::ShellAppsClient(BrowserContext* browser_context)
    : browser_context_(browser_context) {}

ShellAppsClient::~ShellAppsClient() {}

std::vector<BrowserContext*> ShellAppsClient::GetLoadedBrowserContexts() {
  std::vector<BrowserContext*> browser_contexts;
  browser_contexts.push_back(browser_context_);
  return browser_contexts;
}

AppWindow* ShellAppsClient::CreateAppWindow(
    BrowserContext* context,
    const extensions::Extension* extension) {
  return new AppWindow(context, new ShellAppWindowDelegate, extension);
}

void ShellAppsClient::IncrementKeepAliveCount() {}

void ShellAppsClient::DecrementKeepAliveCount() {}

}  // namespace apps
