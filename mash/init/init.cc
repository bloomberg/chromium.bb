// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/init/init.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/guid.h"
#include "mash/login/public/interfaces/login.mojom.h"
#include "services/service_manager/public/cpp/connection.h"
#include "services/service_manager/public/cpp/connector.h"

namespace mash {
namespace init {

Init::Init() {}
Init::~Init() {}

void Init::OnStart(const service_manager::ServiceInfo& info) {
  connector()->Connect("service:ui");
  StartTracing();
  StartLogin();
}

bool Init::OnConnect(const service_manager::ServiceInfo& remote_info,
                     service_manager::InterfaceRegistry* registry) {
  registry->AddInterface<mojom::Init>(this);
  return true;
}

void Init::StartService(const mojo::String& name,
                        const mojo::String& user_id) {
  if (user_services_.find(user_id) == user_services_.end()) {
    service_manager::Connector::ConnectParams params(
        service_manager::Identity(name, user_id));
    std::unique_ptr<service_manager::Connection> connection =
        connector()->Connect(&params);
    connection->SetConnectionLostClosure(
        base::Bind(&Init::UserServiceQuit, base::Unretained(this), user_id));
    user_services_[user_id] = std::move(connection);
  }
}

void Init::StopServicesForUser(const mojo::String& user_id) {
  auto it = user_services_.find(user_id);
  if (it != user_services_.end())
    user_services_.erase(it);
}

void Init::Create(const service_manager::Identity& remote_identity,
                  mojom::InitRequest request) {
  init_bindings_.AddBinding(this, std::move(request));
}

void Init::UserServiceQuit(const std::string& user_id) {
  auto it = user_services_.find(user_id);
  DCHECK(it != user_services_.end());
  user_services_.erase(it);
}

void Init::StartTracing() {
  connector()->Connect("service:tracing");
}

void Init::StartLogin() {
  login_connection_ = connector()->Connect("service:login");
  mash::login::mojom::LoginPtr login;
  login_connection_->GetInterface(&login);
  login->ShowLoginUI();
}

}  // namespace init
}  // namespace main
