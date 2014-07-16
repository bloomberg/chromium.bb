// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <string>

#include "mojo/examples/sample_app/gles2_client_impl.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/application/application_impl.h"
#include "mojo/public/cpp/gles2/gles2.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/cpp/system/macros.h"
#include "mojo/public/cpp/utility/run_loop.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"

namespace examples {

class SampleApp : public mojo::ApplicationDelegate,
                  public mojo::NativeViewportClient {
 public:
  SampleApp() {}

  virtual ~SampleApp() {
    // TODO(darin): Fix shutdown so we don't need to leak this.
    MOJO_ALLOW_UNUSED GLES2ClientImpl* leaked = gles2_client_.release();
  }

  virtual void Initialize(mojo::ApplicationImpl* app) MOJO_OVERRIDE {
    app->ConnectToService("mojo:mojo_native_viewport_service", &viewport_);
    viewport_.set_client(this);

    mojo::RectPtr rect(mojo::Rect::New());
    rect->x = 10;
    rect->y = 10;
    rect->width = 800;
    rect->height = 600;
    viewport_->Create(rect.Pass());
    viewport_->Show();

    mojo::CommandBufferPtr command_buffer;
    viewport_->CreateGLES2Context(Get(&command_buffer));
    gles2_client_.reset(new GLES2ClientImpl(command_buffer.Pass()));
  }

  virtual void OnCreated() MOJO_OVERRIDE {
  }

  virtual void OnDestroyed(
      const mojo::Callback<void()>& callback) MOJO_OVERRIDE {
    mojo::RunLoop::current()->Quit();
    callback.Run();
  }

  virtual void OnBoundsChanged(mojo::RectPtr bounds) MOJO_OVERRIDE {
    assert(bounds);
    mojo::SizePtr size(mojo::Size::New());
    size->width = bounds->width;
    size->height = bounds->height;
    gles2_client_->SetSize(*size);
  }

  virtual void OnEvent(mojo::EventPtr event,
                       const mojo::Callback<void()>& callback) MOJO_OVERRIDE {
    assert(event);
    if (event->location)
      gles2_client_->HandleInputEvent(*event);
    callback.Run();
  }

 private:
  mojo::GLES2Initializer gles2;
  scoped_ptr<GLES2ClientImpl> gles2_client_;
  mojo::NativeViewportPtr viewport_;

  DISALLOW_COPY_AND_ASSIGN(SampleApp);
};

}  // namespace examples

namespace mojo {

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::SampleApp();
}

}  // namespace mojo
