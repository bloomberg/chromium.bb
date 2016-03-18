// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/session/session.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"

namespace mash {
namespace session {

Session::Session() : connector_(nullptr), screen_locked_(false) {}
Session::~Session() {}

void Session::Initialize(mojo::Connector* connector,
                         const mojo::Identity& identity,
                         uint32_t id) {
  connector_ = connector;
  StartBrowserDriver();
  StartWindowManager();
  StartSystemUI();
  StartQuickLaunch();
}

bool Session::AcceptConnection(mojo::Connection* connection) {
  connection->AddInterface<mojom::Session>(this);
  return true;
}

void Session::Logout() {
  // TODO(beng): Notify connected listeners that login is happening, potentially
  // give them the option to stop it.
  mash::login::mojom::LoginPtr login;
  connector_->ConnectToInterface("mojo:login", &login);
  login->ShowLoginUI();
  // This kills the user environment.
  base::MessageLoop::current()->QuitWhenIdle();
}

void Session::SwitchUser() {
  mash::login::mojom::LoginPtr login;
  connector_->ConnectToInterface("mojo:login", &login);
  login->SwitchUser();
}

void Session::AddScreenlockStateListener(
    mojom::ScreenlockStateListenerPtr listener) {
  listener->ScreenlockStateChanged(screen_locked_);
  screenlock_listeners_.AddInterfacePtr(std::move(listener));
}

void Session::LockScreen() {
  if (screen_locked_)
    return;
  screen_locked_ = true;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(true);
      });
  StartScreenlock();
}
void Session::UnlockScreen() {
  if (!screen_locked_)
    return;
  screen_locked_ = false;
  screenlock_listeners_.ForAllPtrs(
      [](mojom::ScreenlockStateListener* listener) {
        listener->ScreenlockStateChanged(false);
      });
  StopScreenlock();
}

void Session::Create(mojo::Connection* connection,
                     mojom::SessionRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void Session::StartWindowManager() {
  StartRestartableService(
      "mojo:desktop_wm",
      base::Bind(&Session::StartWindowManager,
                 base::Unretained(this)));
}

void Session::StartSystemUI() {
  StartRestartableService("mojo:ash_sysui",
                          base::Bind(&Session::StartSystemUI,
                                     base::Unretained(this)));
}

void Session::StartBrowserDriver() {
  StartRestartableService(
      "mojo:browser_driver",
      base::Bind(&Session::StartBrowserDriver,
                 base::Unretained(this)));
}

void Session::StartQuickLaunch() {
  StartRestartableService(
      "mojo:quick_launch",
      base::Bind(&Session::StartQuickLaunch,
                 base::Unretained(this)));
}

void Session::StartScreenlock() {
  StartRestartableService(
      "mojo:screenlock",
      base::Bind(&Session::StartScreenlock,
                 base::Unretained(this)));
}

void Session::StopScreenlock() {
  auto connection = connections_.find("mojo:screenlock");
  DCHECK(connections_.end() != connection);
  connections_.erase(connection);
}

void Session::StartRestartableService(
    const std::string& url,
    const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  scoped_ptr<mojo::Connection> connection = connector_->Connect(url);
  // Note: |connection| may be null if we've lost our connection to the shell.
  if (connection) {
    connection->SetConnectionLostClosure(restart_callback);
    connection->AddInterface<mojom::Session>(this);
    connections_[url] = std::move(connection);
  }
}

}  // namespace session
}  // namespace main
