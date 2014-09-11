// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/extensions/chrome/athena_apps_client.h"

#include "athena/activity/public/activity_factory.h"
#include "athena/activity/public/activity_manager.h"
#include "athena/extensions/chrome/athena_app_delegate.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"

namespace athena {
namespace {

// A short term hack to get WebView from ChromeNativeAppWindowViews.
// TODO(oshima): Implement athena's NativeAppWindow.
class AthenaNativeAppWindowViews : public ChromeNativeAppWindowViews {
 public:
  AthenaNativeAppWindowViews() {}
  virtual ~AthenaNativeAppWindowViews() {}

  views::WebView* GetWebView() {
    return web_view();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaNativeAppWindowViews);
};

}  // namespace

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
  return new extensions::AppWindow(context, new AthenaAppDelegate, extension);
}

extensions::NativeAppWindow* AthenaAppsClient::CreateNativeAppWindow(
    extensions::AppWindow* app_window,
    const extensions::AppWindow::CreateParams& params) {
  AthenaNativeAppWindowViews* native_window = new AthenaNativeAppWindowViews;
  native_window->Init(app_window, params);
  Activity* app_activity = ActivityFactory::Get()->CreateAppActivity(
      app_window, native_window->GetWebView());
  ActivityManager::Get()->AddActivity(app_activity);
  return native_window;
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
