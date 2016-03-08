// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/shell/shell_application_delegate.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"

namespace mash {
namespace shell {

ShellApplicationDelegate::ShellApplicationDelegate()
    : connector_(nullptr), screen_locked_(false) {}

ShellApplicationDelegate::~ShellApplicationDelegate() {}

void ShellApplicationDelegate::Initialize(mojo::Connector* connector,
                                          const mojo::Identity& identity,
                                          uint32_t id) {
  connector_ = connector;
  StartBrowserDriver();
  StartWindowManager();
  StartSystemUI();
  StartQuickLaunch();
}

bool ShellApplicationDelegate::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<mojom::Shell>(this);
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
    mojom::ShellRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void ShellApplicationDelegate::StartWindowManager() {
  StartRestartableService(
      "mojo:desktop_wm",
      base::Bind(&ShellApplicationDelegate::StartWindowManager,
                 base::Unretained(this)));
}

void ShellApplicationDelegate::StartSystemUI() {
  StartRestartableService("mojo:ash_sysui",
                          base::Bind(&ShellApplicationDelegate::StartSystemUI,
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
  scoped_ptr<mojo::Connection> connection = connector_->Connect(url);
  // Note: |connection| may be null if we've lost our connection to the shell.
  if (connection) {
    connection->SetConnectionLostClosure(restart_callback);
    connection->AddInterface<mojom::Shell>(this);
    connections_[url] = std::move(connection);
  }
}

}  // namespace shell
}  // namespace main
