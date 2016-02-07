// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shell/shell_application_delegate.h"

#include "base/bind.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/shell.h"

namespace mash {
namespace shell {

ShellApplicationDelegate::ShellApplicationDelegate()
    : shell_(nullptr), screen_locked_(false) {}

ShellApplicationDelegate::~ShellApplicationDelegate() {}

void ShellApplicationDelegate::Initialize(mojo::Shell* shell,
                                          const std::string& url,
                                          uint32_t id) {
  shell_ = shell;
  StartBrowserDriver();
  StartWindowManager();
  StartWallpaper();
  StartShelf();
  StartQuickLaunch();
}

bool ShellApplicationDelegate::AcceptConnection(mojo::Connection* connection) {
  connection->AddService<mash::shell::mojom::Shell>(this);
  return true;
}

void ShellApplicationDelegate::AddScreenlockStateListener(
    mojom::ScreenlockStateListenerPtr listener) {
  listener->ScreenlockStateChanged(screen_locked_);
  screenlock_listeners_.AddInterfacePtr(std::move(listener));
}

void ShellApplicationDelegate::LockScreen() {
  if (screen_locked_)
    return;
  screen_locked_ = true;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(true);
      });
  StartScreenlock();
}
void ShellApplicationDelegate::UnlockScreen() {
  if (!screen_locked_)
    return;
  screen_locked_ = false;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(false);
      });
  StopScreenlock();
}

void ShellApplicationDelegate::Create(
    mojo::Connection* connection,
    mojo::InterfaceRequest<mash::shell::mojom::Shell> r) {
  bindings_.AddBinding(this, std::move(r));
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

void ShellApplicationDelegate::StartScreenlock() {
  StartRestartableService(
      "mojo:screenlock",
      base::Bind(&ShellApplicationDelegate::StartScreenlock,
                 base::Unretained(this)));
}

void ShellApplicationDelegate::StopScreenlock() {
  auto connection = connections_.find("mojo:screenlock");
  DCHECK(connections_.end() != connection);
  connections_.erase(connection);
}

void ShellApplicationDelegate::StartRestartableService(
    const std::string& url,
    const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  scoped_ptr<mojo::Connection> connection = shell_->Connect(url);
  connection->SetRemoteServiceProviderConnectionErrorHandler(restart_callback);
  connections_[url] = std::move(connection);
}

}  // namespace shell
}  // namespace main
