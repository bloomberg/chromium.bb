// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_app_window_client.h"

#include <vector>

#include "extensions/browser/app_window/app_window.h"
#include "extensions/shell/browser/desktop_controller.h"
#include "extensions/shell/browser/shell_native_app_window.h"

namespace extensions {

ShellAppWindowClient::ShellAppWindowClient() {
}

ShellAppWindowClient::~ShellAppWindowClient() {
}

std::vector<content::BrowserContext*>
ShellAppWindowClient::GetLoadedBrowserContexts() {
  NOTIMPLEMENTED();
  return std::vector<content::BrowserContext*>();
}

AppWindow* ShellAppWindowClient::CreateAppWindow(
    content::BrowserContext* context,
    const Extension* extension) {
  return DesktopController::instance()->CreateAppWindow(context, extension);
}

NativeAppWindow* ShellAppWindowClient::CreateNativeAppWindow(
      AppWindow* window,
      const AppWindow::CreateParams& params) {
  ShellNativeAppWindow* native_app_window =
      new ShellNativeAppWindow(window, params);
  DesktopController::instance()->AddAppWindow(
      native_app_window->GetNativeWindow());
  return native_app_window;
}

void ShellAppWindowClient::IncrementKeepAliveCount() {
  // app_shell runs until the system powers off, so it doesn't need to track
  // open apps or windows to keep itself alive.
}

void ShellAppWindowClient::DecrementKeepAliveCount() {
  // See IncrementKeepAliveCount().
}

void ShellAppWindowClient::OpenDevToolsWindow(
    content::WebContents* web_contents,
    const base::Closure& callback) {
  NOTIMPLEMENTED();
}

bool ShellAppWindowClient::IsCurrentChannelOlderThanDev() {
  return false;
}

}  // namespace extensions
