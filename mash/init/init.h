// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_INIT_INIT_H_
#define MASH_INIT_INIT_H_

#include <map>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mash/init/public/interfaces/init.mojom.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/shell/public/cpp/connector.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace mojo {
class Connection;
}

namespace mash {
namespace init {

class Init : public mojo::ShellClient,
             public mojo::InterfaceFactory<mojom::Init>,
             public mojom::Init {
 public:
  Init();
  ~Init() override;

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const mojo::Identity& identity,
                  uint32_t id) override;

  // mojo::InterfaceFactory<mojom::Login>:
  void Create(mojo::Connection* connection,
              mojom::InitRequest request) override;

  // mojom::Init:
  void StartService(const mojo::String& name,
                    const mojo::String& user_id) override;
  void StopServicesForUser(const mojo::String& user_id) override;

  void StartLogin();

  mojo::Connector* connector_;
  scoped_ptr<mojo::Connection> login_connection_;
  mojo::BindingSet<mojom::Init> init_bindings_;
  std::map<std::string, scoped_ptr<mojo::Connection>> user_services_;
  const std::string login_user_id_;

  DISALLOW_COPY_AND_ASSIGN(Init);
};

}  // namespace init
}  // namespace mash

#endif  // MASH_INIT_INIT_H_
