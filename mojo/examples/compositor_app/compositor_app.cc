// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/macros.h"
#include "mojo/examples/compositor_app/compositor_host.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/application/application_runner_chromium.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace examples {

class SampleApp : public ApplicationDelegate, public NativeViewportClient {
 public:
  SampleApp() {}
  virtual ~SampleApp() {}

  virtual void Initialize(ApplicationImpl* app) OVERRIDE {
    app->ConnectToService("mojo:mojo_native_viewport_service", &viewport_);
    viewport_.set_client(this);
    viewport_->Create(Rect::From(gfx::Rect(10, 10, 800, 600)));
    viewport_->Show();

    // TODO(jamesr): Should be mojo:mojo_gpu_service
    app->ConnectToService("mojo:mojo_native_viewport_service", &gpu_service_);
  }

  virtual void OnCreated(uint64_t native_viewport_id) OVERRIDE {
    CommandBufferPtr cb;
    gpu_service_->CreateOnscreenGLES2Context(
        native_viewport_id, Size::From(gfx::Size(800, 600)), Get(&cb));
    host_.reset(new CompositorHost(cb.PassMessagePipe()));
  }

  virtual void OnDestroyed() OVERRIDE { base::MessageLoop::current()->Quit(); }

  virtual void OnBoundsChanged(RectPtr bounds) OVERRIDE {
    if (host_)
      host_->SetSize(gfx::Size(bounds->width, bounds->height));
  }

  virtual void OnEvent(EventPtr event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

 private:
  NativeViewportPtr viewport_;
  GpuPtr gpu_service_;
  scoped_ptr<CompositorHost> host_;
  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::SampleApp);
  return runner.Run(shell_handle);
}
