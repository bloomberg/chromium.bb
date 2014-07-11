// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "base/macros.h"
#include "mojo/examples/compositor_app/compositor_host.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
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

    MessagePipe pipe;
    viewport_->CreateGLES2Context(
        MakeRequest<CommandBuffer>(pipe.handle0.Pass()));
    host_.reset(new CompositorHost(pipe.handle1.Pass()));
  }

  virtual void OnCreated() OVERRIDE {
  }

  virtual void OnDestroyed(const mojo::Callback<void()>& callback) OVERRIDE {
    base::MessageLoop::current()->Quit();
    callback.Run();
  }

  virtual void OnBoundsChanged(RectPtr bounds) OVERRIDE {
    host_->SetSize(gfx::Size(bounds->width, bounds->height));
  }

  virtual void OnEvent(EventPtr event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

 private:
  mojo::GLES2Initializer gles2;
  NativeViewportPtr viewport_;
  scoped_ptr<CompositorHost> host_;
  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::SampleApp();
}

}  // namespace mojo
