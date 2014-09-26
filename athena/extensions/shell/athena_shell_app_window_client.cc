// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/shell/athena_shell_app_window_client.h"

#include "athena/extensions/shell/athena_shell_app_delegate.h"
#include "extensions/browser/app_window/app_window.h"

namespace athena {

AthenaShellAppWindowClient::AthenaShellAppWindowClient() {
}

AthenaShellAppWindowClient::~AthenaShellAppWindowClient() {
}

extensions::AppWindow* AthenaShellAppWindowClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  return new extensions::AppWindow(
      context, new AthenaShellAppDelegate, extension);
}

void AthenaShellAppWindowClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    const base::Closure& callback) {
  // TODO(oshima): Figure out what to do.
}

bool AthenaShellAppWindowClient::IsCurrentChannelOlderThanDev() {
  return false;
}

}  // namespace athena
