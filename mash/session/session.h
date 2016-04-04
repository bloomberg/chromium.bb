// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_SESSION_SESSION_H_
#define MASH_SESSION_SESSION_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/session/public/interfaces/session.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
class Connection;
}

namespace mash {
namespace session {

class Session : public mojo::ShellClient,
                public mojom::Session,
                public mojo::InterfaceFactory<mojom::Session> {
 public:
  Session();
  ~Session() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector,
                  const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojom::Session:
  void Logout() override;
  void SwitchUser() override;
  void AddScreenlockStateListener(
      mojom::ScreenlockStateListenerPtr listener) override;
  void LockScreen() override;
  void UnlockScreen() override;

  // mojo::InterfaceFactory<mojom::Session>:
  void Create(mojo::Connection* connection,
              mojom::SessionRequest request) override;

  void StartWindowManager();
  void StartSystemUI();
  void StartBrowserDriver();
  void StartQuickLaunch();

  void StartScreenlock();
  void StopScreenlock();

  // Starts the application at |url|, running |restart_callback| if the
  // connection to the application is closed.
  void StartRestartableService(const std::string& url,
                               const base::Closure& restart_callback);

  mojo::Connector* connector_;
  std::map<std::string, std::unique_ptr<mojo::Connection>> connections_;
  bool screen_locked_;
  mojo::BindingSet<mojom::Session> bindings_;
  mojo::InterfacePtrSet<mojom::ScreenlockStateListener> screenlock_listeners_;

  DISALLOW_COPY_AND_ASSIGN(Session);
};

}  // namespace session
}  // namespace mash

#endif  // MASH_SESSION_SESSION_H_
