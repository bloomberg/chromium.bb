// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/apps/chrome_app_window_client.h"

#include "base/memory/singleton.h"
#include "chrome/browser/apps/scoped_keep_alive.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/common/extensions/features/feature_channel.h"
#include "extensions/browser/app_window/app_window.h"
#include "extensions/common/extension.h"

// TODO(jamescook): We probably shouldn't compile this class at all on Android.
// See http://crbug.com/343612
#if !defined(OS_ANDROID)
#include "chrome/browser/ui/apps/chrome_app_delegate.h"
#endif

ChromeAppWindowClient::ChromeAppWindowClient() {
}

ChromeAppWindowClient::~ChromeAppWindowClient() {
}

// static
ChromeAppWindowClient* ChromeAppWindowClient::GetInstance() {
  return Singleton<ChromeAppWindowClient,
                   LeakySingletonTraits<ChromeAppWindowClient> >::get();
}

extensions::AppWindow* ChromeAppWindowClient::CreateAppWindow(
    content::BrowserContext* context,
    const extensions::Extension* extension) {
#if defined(OS_ANDROID)
  return NULL;
#else
  return new extensions::AppWindow(
      context,
      new ChromeAppDelegate(make_scoped_ptr(new ScopedKeepAlive)),
      extension);
#endif
}

extensions::NativeAppWindow* ChromeAppWindowClient::CreateNativeAppWindow(
    extensions::AppWindow* window,
    const extensions::AppWindow::CreateParams& params) {
#if defined(OS_ANDROID)
  return NULL;
#else
  return CreateNativeAppWindowImpl(window, params);
#endif
}

void ChromeAppWindowClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    const base::Closure& callback) {
  DevToolsWindow* devtools_window = DevToolsWindow::OpenDevToolsWindow(
      web_contents, DevToolsToggleAction::ShowConsole());
  devtools_window->SetLoadCompletedCallback(callback);
}

bool ChromeAppWindowClient::IsCurrentChannelOlderThanDev() {
  return extensions::GetCurrentChannel() > chrome::VersionInfo::CHANNEL_DEV;
}
