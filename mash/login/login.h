// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MASH_LOGIN_LOGIN_H_
#define MASH_LOGIN_LOGIN_H_

#include <map>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "mash/init/public/interfaces/login.mojom.h"
#include "mojo/services/tracing/public/cpp/tracing_impl.h"
#include "mojo/shell/public/cpp/shell_client.h"

namespace views {
class AuraInit;
}

namespace mash {
namespace login {

class Login : public mojo::ShellClient {
 public:
  Login();
  ~Login() override;

  init::mojom::Login* login() { return login_.get(); }

 private:
  // mojo::ShellClient:
  void Initialize(mojo::Connector* connector, const std::string& url,
                  const std::string& user_id, uint32_t id) override;
  bool AcceptConnection(mojo::Connection* connection) override;

  mojo::TracingImpl tracing_;
  scoped_ptr<views::AuraInit> aura_init_;
  init::mojom::LoginPtr login_;

  DISALLOW_COPY_AND_ASSIGN(Login);
};

}  // namespace login
}  // namespace mash

#endif  // MASH_LOGIN_LOGIN_H_
