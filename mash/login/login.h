// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_LOGIN_LOGIN_H_
#define MASH_LOGIN_LOGIN_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mash/init/public/interfaces/init.mojom.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace login {

class LoginController : public mojo::ShellClient,
                        public mojo::InterfaceFactory<mojom::Login> {
 public:
  LoginController();
  ~LoginController() override;

  init::mojom::Init* init() { return init_.get(); }
  const std::string& login_user_id() const { return login_user_id_; }

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  // mojo::InterfaceFactory<mojom::Login>:
  void Create(mojo::Connection* connection,
              mojom::LoginRequest request) override;

  mojo::Connector* connector_;
  std::string login_user_id_;
  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;
  init::mojom::InitPtr init_;

  DISALLOW_COPY_AND_ASSIGN(LoginController);
};

}  // namespace login
}  // namespace mash

#endif  // MASH_LOGIN_LOGIN_H_
