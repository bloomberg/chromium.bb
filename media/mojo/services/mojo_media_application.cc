// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_renderer_service.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/interface_factory_impl.h"

namespace media {

class MojoMediaApplication
    : public mojo::ApplicationDelegate,
      public mojo::InterfaceFactory<mojo::MediaRenderer> {
 public:
  // mojo::ApplicationDelegate implementation.
  bool ConfigureIncomingConnection(
      mojo::ApplicationConnection* connection) override {
    connection->AddService(this);
    return true;
  }

  // mojo::InterfaceFactory<mojo::MediaRenderer> implementation.
  void Create(mojo::ApplicationConnection* connection,
              mojo::InterfaceRequest<mojo::MediaRenderer> request) override {
    mojo::BindToRequest(new MojoRendererService(), &request);
  }
};

}  // namespace media

MojoResult MojoMain(MojoHandle mojo_handle) {
  mojo::ApplicationRunnerChromium runner(new media::MojoMediaApplication);
  return runner.Run(mojo_handle);
}
