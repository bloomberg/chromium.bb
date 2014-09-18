// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/shell/athena_shell_apps_client.h"

#include "athena/extensions/shell/athena_shell_app_delegate.h"
#include "extensions/browser/app_window/app_window.h"

namespace athena {

AthenaShellAppsClient::AthenaShellAppsClient(content::BrowserContext* context)
    : context_(context) {
  DCHECK(context_);
}

AthenaShellAppsClient::~AthenaShellAppsClient() {
}

std::vector<content::BrowserContext*>
AthenaShellAppsClient::GetLoadedBrowserContexts() {
  std::vector<content::BrowserContext*> contexts(1, context_);
  return contexts;
}

extensions::AppWindow* AthenaShellAppsClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  return new extensions::AppWindow(
      context, new AthenaShellAppDelegate, extension);
}

void AthenaShellAppsClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    const base::Closure& callback) {
  // TODO(oshima): Figure out what to do.
}

bool AthenaShellAppsClient::IsCurrentChannelOlderThanDev() {
  return false;
}

}  // namespace athena
