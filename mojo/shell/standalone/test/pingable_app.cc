// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/bindings/callback.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "mojo/shell/public/cpp/application_delegate.h"
#include "mojo/shell/public/cpp/application_impl.h"
#include "mojo/shell/public/cpp/application_runner.h"
#include "mojo/shell/public/cpp/interface_factory.h"
#include "mojo/shell/standalone/test/pingable.mojom.h"

namespace mojo {

class PingableImpl : public Pingable {
 public:
  PingableImpl(InterfaceRequest<Pingable> request,
               const std::string& app_url,
               const std::string& connection_url)
      : binding_(this, std::move(request)),
        app_url_(app_url),
        connection_url_(connection_url) {}

  ~PingableImpl() override {}

 private:
  void Ping(const String& message,
            const Callback<void(String, String, String)>& callback) override {
    callback.Run(app_url_, connection_url_, message);
  }

  StrongBinding<Pingable> binding_;
  std::string app_url_;
  std::string connection_url_;
};

class PingableApp : public mojo::ApplicationDelegate,
                    public mojo::InterfaceFactory<Pingable> {
 public:
  PingableApp() {}
  ~PingableApp() override {}

 private:
  // ApplicationDelegate:
  void Initialize(ApplicationImpl* impl) override { app_url_ = impl->url(); }

  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory<Pingable>:
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<Pingable> request) override {
    new PingableImpl(std::move(request), app_url_,
                     connection->GetConnectionURL());
  }

  std::string app_url_;
};

}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunner runner(new mojo::PingableApp);
  return runner.Run(shell_handle);
}
