// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/init/init.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"

namespace mash {
namespace init {

Init::Init() : connector_(nullptr), login_user_id_(base::GenerateGUID()) {}
Init::~Init() {}

void Init::Initialize(mojo::Connector* connector,
                      const mojo::Identity& identity,
                      uint32_t id) {
  connector_ = connector;
  connector_->Connect("mojo:mus");
  StartLogin();
}

void Init::StartService(const mojo::String& name,
                        const mojo::String& user_id) {
  DCHECK(user_services_.find(user_id) == user_services_.end());
  mojo::Connector::ConnectParams params(mojo::Identity(name, user_id));
  user_services_[user_id] = connector_->Connect(&params);
}

void Init::StopServicesForUser(const mojo::String& user_id) {
  // TODO(beng): Make shell cascade shutdown of services.
  auto it = user_services_.find(user_id);
  if (it != user_services_.end())
    user_services_.erase(it);
}

void Init::Create(mojo::Connection* connection, mojom::InitRequest request) {
  init_bindings_.AddBinding(this, std::move(request));
}

void Init::StartLogin() {
  mojo::Connector::ConnectParams params(
      mojo::Identity("mojo:login", login_user_id_));
  login_connection_ = connector_->Connect(&params);
  login_connection_->AddInterface<mojom::Init>(this);
  login_connection_->SetConnectionLostClosure(
      base::Bind(&Init::StartLogin, base::Unretained(this)));
  mash::login::mojom::LoginPtr login;
  login_connection_->GetInterface(&login);
  login->ShowLoginUI();
}

}  // namespace init
}  // namespace main
