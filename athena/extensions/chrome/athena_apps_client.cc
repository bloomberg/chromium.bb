// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/athena_apps_client.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"

namespace athena {

AthenaAppsClient::AthenaAppsClient() {
}

AthenaAppsClient::~AthenaAppsClient() {
}

std::vector<content::BrowserContext*>
AthenaAppsClient::GetLoadedBrowserContexts() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  return std::vector<content::BrowserContext*>(profiles.begin(),
                                               profiles.end());
}

extensions::AppWindow* AthenaAppsClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
  return new extensions::AppWindow(context, new ChromeAppDelegate, extension);
}

extensions::NativeAppWindow* AthenaAppsClient::CreateNativeAppWindow(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  // TODO(oshima): Implement athena's native appwindow.
  ChromeNativeAppWindowViews* window = new ChromeNativeAppWindowViews;
  window->Init(app_window, params);
  athena::ActivityManager::Get()->AddActivity(
      athena::ActivityFactory::Get()->CreateAppActivity(app_window));
  return window;
}

void AthenaAppsClient::IncrementKeepAliveCount() {
  // No need to keep track of KeepAlive count on ChromeOS.
}

void AthenaAppsClient::DecrementKeepAliveCount() {
  // No need to keep track of KeepAlive count on ChromeOS.
}

void AthenaAppsClient::OpenDevToolsWindow(content::WebContents* web_contents,
                                          const base::Closure& callback) {
  // TODO(oshima): Figure out what to do.
  DevToolsWindow* devtools_window = DevToolsWindow::OpenDevToolsWindow(
      web_contents, DevToolsToggleAction::ShowConsole());
  devtools_window->SetLoadCompletedCallback(callback);
}

bool AthenaAppsClient::IsCurrentChannelOlderThanDev() {
  return extensions::GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV;
}

}  // namespace athena
