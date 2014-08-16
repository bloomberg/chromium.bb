// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/platform_thread.h"
#include "mojo/examples/surfaces_app/child_gl_impl.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/bindings/string.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"

namespace mojo {
namespace examples {

class ChildGLApp : public ApplicationDelegate, public InterfaceFactory<Child> {
 public:
  ChildGLApp() {}
  virtual ~ChildGLApp() {}

  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    surfaces_service_connection_ =
        app->ConnectToApplication("mojo:mojo_surfaces_service");
    // TODO(jamesr): Should be mojo:mojo_gpu_service
    app->ConnectToService("mojo:mojo_native_viewport_service", &gpu_service_);
  }

  // ApplicationDelegate implementation.
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    connection->AddService(this);
    return true;
  }

  // InterfaceFactory<Child> implementation.
  virtual void Create(ApplicationConnection* connection,
                      InterfaceRequest<Child> request) OVERRIDE {
    CommandBufferPtr command_buffer;
    gpu_service_->CreateOffscreenGLES2Context(Get(&command_buffer));
    BindToRequest(
        new ChildGLImpl(surfaces_service_connection_, command_buffer.Pass()),
        &request);
  }

 private:
  ApplicationConnection* surfaces_service_connection_;
  GpuPtr gpu_service_;

  DISALLOW_COPY_AND_ASSIGN(ChildGLApp);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::ChildGLApp();
}

}  // namespace mojo
