// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/surfaces_app/child_impl.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/string.h"

namespace mojo {
namespace examples {

class ChildApp : public ApplicationDelegate, public ChildImpl::Context {
 public:
  ChildApp() {}
  virtual ~ChildApp() {}

  virtual void Initialize(ApplicationImpl* app) OVERRIDE { app_ = app; }

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    connection->AddService<ChildImpl, ChildImpl::Context>(this);
    return true;
  }

  // ChildImpl::Context implementation.
  virtual ApplicationConnection* ShellConnection(
      const mojo::String& application_url) OVERRIDE {
    return app_->ConnectToApplication(application_url);
  }

 private:
  ApplicationImpl* app_;

  DISALLOW_COPY_AND_ASSIGN(ChildApp);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::ChildApp();
}

}  // namespace mojo
