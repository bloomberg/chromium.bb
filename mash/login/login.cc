// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/login/login.h"

#include "base/macros.h"
#include "mash/login/ui.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/public/cpp/connector.h"
#include "ui/views/mus/aura_init.h"
#include "ui/views/mus/window_manager_connection.h"

namespace mash {
namespace login {
namespace {

class Login : public mojom::Login {
 public:
   Login(mojo::Connector* connector,
         LoginController* controller,
         const std::string& user_id,
         mojom::LoginRequest request)
      : connector_(connector),
        controller_(controller),
        user_id_(user_id),
        binding_(this, std::move(request)) {}
  ~Login() override {}

 private:
  // mojom::Login:
  void ShowLoginUI() override {
    UI::Show(connector_, controller_);
  }
  void Logout() override {
    controller_->init()->StopServicesForUser(user_id_);
    UI::Show(connector_, controller_);
  }
  void SwitchUser() override {
    UI::Show(connector_, controller_);
  }

  mojo::Connector* connector_;
  LoginController* controller_;
  const std::string user_id_;
  mojo::StrongBinding<mojom::Login> binding_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

}  // namespace

LoginController::LoginController() {}
LoginController::~LoginController() {}

void LoginController::Initialize(mojo::Connector* connector,
                                 const mojo::Identity& identity,
                                 uint32_t id) {
  connector_ = connector;
  login_user_id_ = identity.user_id();
  tracing_.Initialize(connector, identity.name());

  aura_init_.reset(new views::AuraInit(connector, "views_mus_resources.pak"));
}

bool LoginController::AcceptConnection(mojo::Connection* connection) {
  if (connection->GetRemoteIdentity().name() == "mojo:mash_init")
    connection->GetInterface(&init_);
  connection->AddInterface<mojom::Login>(this);
  return true;
}

void LoginController::Create(mojo::Connection* connection,
                             mojom::LoginRequest request) {
  new Login(connector_, this, connection->GetRemoteIdentity().user_id(),
            std::move(request));
}

}  // namespace login
}  // namespace main
