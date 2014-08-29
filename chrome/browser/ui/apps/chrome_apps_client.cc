// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_apps_client.h"

#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"

// TODO(jamescook): We probably shouldn't compile this class at all on Android.
// See http://crbug.com/343612
#if !defined(OS_ANDROID)
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#endif

ChromeAppsClient::ChromeAppsClient() {
}

ChromeAppsClient::~ChromeAppsClient() {
}

// static
ChromeAppsClient* ChromeAppsClient::GetInstance() {
  return Singleton<ChromeAppsClient,
                   LeakySingletonTraits<ChromeAppsClient> >::get();
}

std::vector<content::BrowserContext*>
ChromeAppsClient::GetLoadedBrowserContexts() {
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  return std::vector<content::BrowserContext*>(profiles.begin(),
                                               profiles.end());
}

extensions::AppWindow* ChromeAppsClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
#if defined(OS_ANDROID)
  return NULL;
#else
  return new extensions::AppWindow(context, new ChromeAppDelegate, extension);
#endif
}

extensions::NativeAppWindow* ChromeAppsClient::CreateNativeAppWindow(
    extensions::AppWindow* window,
    const extensions::AppWindow::CreateParams& params) {
#if defined(OS_ANDROID)
  return NULL;
#else
  return CreateNativeAppWindowImpl(window, params);
#endif
}

void ChromeAppsClient::IncrementKeepAliveCount() {
#if !defined(OS_ANDROID)
  chrome::IncrementKeepAliveCount();
#endif
}

void ChromeAppsClient::DecrementKeepAliveCount() {
#if !defined(OS_ANDROID)
  chrome::DecrementKeepAliveCount();
#endif
}

void ChromeAppsClient::OpenDevToolsWindow(content::WebContents* web_contents,
                                          const base::Closure& callback) {
  DevToolsWindow* devtools_window = DevToolsWindow::OpenDevToolsWindow(
      web_contents, DevToolsToggleAction::ShowConsole());
  devtools_window->SetLoadCompletedCallback(callback);
}

bool ChromeAppsClient::IsCurrentChannelOlderThanDev() {
  return extensions::GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV;
}
