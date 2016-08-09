// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_INIT_INIT_H_
#define MASH_INIT_INIT_H_

#include <map>
#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "mash/init/public/interfaces/init.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "services/shell/public/cpp/connector.h"
#include "services/shell/public/cpp/service.h"

namespace mojo {
class Connection;
}

namespace mash {
namespace init {

class Init : public shell::Service,
             public shell::InterfaceFactory<mojom::Init>,
             public mojom::Init {
 public:
  Init();
  ~Init() override;

 private:
  // shell::Service:
  void OnStart(const shell::Identity& identity) override;
  bool OnConnect(const shell::Identity& remote_identity,
                 shell::InterfaceRegistry* registry) override;

  // shell::InterfaceFactory<mojom::Login>:
  void Create(const shell::Identity& remote_identity,
              mojom::InitRequest request) override;

  // mojom::Init:
  void StartService(const mojo::String& name,
                    const mojo::String& user_id) override;
  void StopServicesForUser(const mojo::String& user_id) override;

  void UserServiceQuit(const std::string& user_id);

  void StartTracing();
  void StartLogin();

  std::unique_ptr<shell::Connection> login_connection_;
  mojo::BindingSet<mojom::Init> init_bindings_;
  std::map<std::string, std::unique_ptr<shell::Connection>> user_services_;

  DISALLOW_COPY_AND_ASSIGN(Init);
};

}  // namespace init
}  // namespace mash

#endif  // MASH_INIT_INIT_H_
