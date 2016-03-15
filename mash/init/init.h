// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_INIT_INIT_H_
#define MASH_INIT_INIT_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/user_access_manager.mojom.h"
#include "mash/init/public/interfaces/login.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
class Connection;
}

namespace mash {
namespace init{

class Init : public mojo::ShellClient,
             public mojom::Login,
             public mojo::InterfaceFactory<mojom::Login> {
 public:
  Init();
  ~Init() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojom::Login:
  void LoginAs(const mojo::String& user_id) override;
  void Logout() override;
  void SwitchUser() override;

  // mojo::InterfaceFactory<mojom::Login>:
  void Create(mojo::Connection* connection,
              mojom::LoginRequest request) override;

  void StartWindowManager();
  void StartLogin();

  // Starts the application at |url|, running |restart_callback| if the
  // connection to the application is closed.
  void StartRestartableService(mojo::Connector::ConnectParams* params,
                               const base::Closure& restart_callback);

  mojo::Connector* connector_;
  std::map<std::string, scoped_ptr<mojo::Connection>> connections_;
  mojo::BindingSet<mojom::Login> login_bindings_;
  scoped_ptr<mojo::Connection> mus_connection_;
  mus::mojom::UserAccessManagerPtr user_access_manager_;
  const std::string login_user_id_;

  DISALLOW_COPY_AND_ASSIGN(Init);
};

}  // namespace init
}  // namespace mash

#endif  // MASH_INIT_INIT_H_
