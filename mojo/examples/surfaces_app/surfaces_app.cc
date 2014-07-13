// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/examples/surfaces_app/child.mojom.h"
#include "mojo/examples/surfaces_app/embedder.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/public/interfaces/service_provider/service_provider.mojom.h"
#include "mojo/services/gles2/command_buffer.mojom.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace examples {

class SurfacesApp : public ApplicationDelegate,
                    public surfaces::SurfaceClient,
                    public NativeViewportClient {
 public:
  SurfacesApp() {}
  virtual ~SurfacesApp() {}

  // ApplicationDelegate implementation
  virtual bool ConfigureIncomingConnection(
      ApplicationConnection* connection) OVERRIDE {
    connection->ConnectToService("mojo:mojo_native_viewport_service",
                                 &viewport_);
    viewport_.set_client(this);

    connection->ConnectToService("mojo:mojo_surfaces_service", &surfaces_);
    surfaces_.set_client(this);

    size_ = gfx::Size(800, 600);

    viewport_->Create(Rect::From(gfx::Rect(gfx::Point(10, 10), size_)));
    viewport_->Show();

    child_size_ = gfx::Size(size_.width() / 3, size_.height() / 2);
    connection->ConnectToService("mojo:mojo_surfaces_child_app", &child_one_);
    connection->ConnectToService("mojo:mojo_surfaces_child_app", &child_two_);
    child_one_->ProduceFrame(surfaces::Color::From(SK_ColorBLUE),
                             Size::From(child_size_),
                             base::Bind(&SurfacesApp::ChildOneProducedFrame,
                                        base::Unretained(this)));
    child_two_->ProduceFrame(surfaces::Color::From(SK_ColorGREEN),
                             Size::From(child_size_),
                             base::Bind(&SurfacesApp::ChildTwoProducedFrame,
                                        base::Unretained(this)));
    embedder_.reset(new Embedder(surfaces_.get()));
    return true;
  }

  void ChildOneProducedFrame(surfaces::SurfaceIdPtr id) {
    child_one_id_ = id.To<cc::SurfaceId>();
    Draw(45.0);
  }

  void ChildTwoProducedFrame(surfaces::SurfaceIdPtr id) {
    child_two_id_ = id.To<cc::SurfaceId>();
    Draw(45.0);
  }

  void Draw(double rotation_degrees) {
    if (onscreen_id_.is_null()) {
      onscreen_id_ = allocator_->GenerateId();
      CommandBufferPtr cb;
      viewport_->CreateGLES2Context(Get(&cb));
      surfaces_->CreateGLES2BoundSurface(
          cb.Pass(),
          surfaces::SurfaceId::From(onscreen_id_),
          Size::From(size_));
      embedder_->SetSurfaceId(onscreen_id_);
    }
    if (child_one_id_.is_null() || child_two_id_.is_null())
      return;
    embedder_->ProduceFrame(
        child_one_id_, child_two_id_, child_size_, size_, rotation_degrees);
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &SurfacesApp::Draw, base::Unretained(this), rotation_degrees + 2.0),
        base::TimeDelta::FromMilliseconds(17));
  }

  // surfaces::SurfaceClient implementation.
  virtual void SetIdNamespace(uint32_t id_namespace) OVERRIDE {
    allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
    Draw(45.0);
  }
  virtual void ReturnResources(
      Array<surfaces::ReturnedResourcePtr> resources) OVERRIDE {
    DCHECK(!resources.size());
  }

  // NativeViewportClient implementation
  virtual void OnCreated() OVERRIDE {}
  virtual void OnBoundsChanged(mojo::RectPtr bounds) OVERRIDE {}
  virtual void OnDestroyed(const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }
  virtual void OnEvent(mojo::EventPtr event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

 private:
  surfaces::SurfacePtr surfaces_;
  cc::SurfaceId onscreen_id_;
  scoped_ptr<cc::SurfaceIdAllocator> allocator_;
  scoped_ptr<Embedder> embedder_;
  ChildPtr child_one_;
  cc::SurfaceId child_one_id_;
  ChildPtr child_two_;
  cc::SurfaceId child_two_id_;
  gfx::Size size_;
  gfx::Size child_size_;

  NativeViewportPtr viewport_;

  DISALLOW_COPY_AND_ASSIGN(SurfacesApp);
};

}  // namespace examples

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new examples::SurfacesApp();
}

}  // namespace mojo
