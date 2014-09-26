// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/athena_chrome_app_window_client.h"

#include "athena/extensions/chrome/athena_chrome_app_delegate.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/browser/app_window/app_window.h"

namespace athena {

AthenaChromeAppWindowClient::AthenaChromeAppWindowClient() {
}

AthenaChromeAppWindowClient::~AthenaChromeAppWindowClient() {
}

extensions::AppWindow* AthenaChromeAppWindowClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  return new extensions::AppWindow(
      context, new AthenaChromeAppDelegate, extension);
}

void AthenaChromeAppWindowClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    const base::Closure& callback) {
  // TODO(oshima): Figure out what to do.
  DevToolsWindow* devtools_window = DevToolsWindow::OpenDevToolsWindow(
      web_contents, DevToolsToggleAction::ShowConsole());
  devtools_window->SetLoadCompletedCallback(callback);
}

bool AthenaChromeAppWindowClient::IsCurrentChannelOlderThanDev() {
  return extensions::GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV;
}

}  // namespace athena
