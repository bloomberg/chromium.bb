// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mash/init/init.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "mojo/shell/public/cpp/connection.h"
#include "mojo/shell/public/cpp/connector.h"

namespace mash {
namespace init {

Init::Init() : connector_(nullptr) {}
Init::~Init() {}

void Init::Initialize(mojo::Connector* connector, const std::string& url,
                      const std::string& user_id, uint32_t id) {
  connector_ = connector;
  mus_connection_ = connector_->Connect("mojo:mus");
  StartWindowManager();
  StartLogin();
}

void Init::LoginAs(const mojo::String& user_id) {
  connections_["mojo:mash_login"].reset();
  connections_["mojo:desktop_wm"].reset();
  mojo::Connector::ConnectParams params("mojo:mash_shell");
  params.set_user_id(user_id);
  connector_->Connect(&params);
}

void Init::Create(mojo::Connection* connection, mojom::LoginRequest request) {
  login_bindings_.AddBinding(this, std::move(request));
}

void Init::StartWindowManager() {
  mojo::Connector::ConnectParams params("mojo:desktop_wm");
  params.set_user_id("2");
  StartRestartableService(
      &params,
      base::Bind(&Init::StartWindowManager, base::Unretained(this)));
}

void Init::StartLogin() {
  mojo::Connector::ConnectParams params("mojo:mash_login");
  params.set_user_id("2");
  StartRestartableService(
      &params,
      base::Bind(&Init::StartLogin, base::Unretained(this)));
}

void Init::StartRestartableService(mojo::Connector::ConnectParams* params,
                                   const base::Closure& restart_callback) {
  // TODO(beng): This would be the place to insert logic that counted restarts
  //             to avoid infinite crash-restart loops.
  scoped_ptr<mojo::Connection> connection = connector_->Connect(params);
  // Note: |connection| may be null if we've lost our connection to the shell.
  if (connection) {
    connection->SetConnectionLostClosure(restart_callback);
    connection->AddInterface<mojom::Login>(this);
    connections_[params->name()] = std::move(connection);
  }
}

}  // namespace init
}  // namespace main
