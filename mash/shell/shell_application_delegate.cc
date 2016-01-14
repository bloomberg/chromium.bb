// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shell/shell_application_delegate.h"

#include "base/bind.h"
#include "mojo/shell/public/cpp/application_connection.h"
#include "mojo/shell/public/cpp/application_impl.h"

namespace mash {
namespace shell {

ShellApplicationDelegate::ShellApplicationDelegate() : app_(nullptr) {}

ShellApplicationDelegate::~ShellApplicationDelegate() {}

void ShellApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  app_ = app;
  StartBrowserDriver();
  StartWindowManager();
  StartWallpaper();
  StartShelf();
  StartQuickLaunch();
}

bool ShellApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  return false;
}

void ShellApplicationDelegate::StartWindowManager() {
  StartRestartableService(
      "mojo:desktop_wm",
      base::Bind(&ShellApplicationDelegate::StartWindowManager,
                 base::Unretained(this)));
}

void ShellApplicationDelegate::StartWallpaper() {
  StartRestartableService("mojo:wallpaper",
                          base::Bind(&ShellApplicationDelegate::StartWallpaper,
                                     base::Unretained(this)));
}

void ShellApplicationDelegate::StartShelf() {
  StartRestartableService("mojo:shelf",
                          base::Bind(&ShellApplicationDelegate::StartShelf,
                                     base::Unretained(this)));
}

void ShellApplicationDelegate::StartBrowserDriver() {
  StartRestartableService(
      "mojo:browser_driver",
      base::Bind(&ShellApplicationDelegate::StartBrowserDriver,
                 base::Unretained(this)));
}

void ShellApplicationDelegate::StartQuickLaunch() {
  StartRestartableService(
      "mojo:quick_launch",
      base::Bind(&ShellApplicationDelegate::StartQuickLaunch,
                 base::Unretained(this)));
}

void ShellApplicationDelegate::StartRestartableService(
    const std::string& url,
    const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  scoped_ptr<mojo::ApplicationConnection> connection =
      app_->ConnectToApplication(url);
  connection->SetRemoteServiceProviderConnectionErrorHandler(restart_callback);
  connections_[url] = std::move(connection);
}

}  // namespace shell
}  // namespace main
