// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "cc/surfaces/surface_id_allocator.h"
#include "mojo/application/application_runner_chromium.h"
#include "mojo/examples/surfaces_app/child.mojom.h"
#include "mojo/examples/surfaces_app/embedder.h"
#include "mojo/public/c/system/main.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/public/cpp/application/application_delegate.h"
#include "mojo/public/cpp/system/core.h"
#include "mojo/services/gles2/command_buffer.mojom.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/public/interfaces/native_viewport/native_viewport.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces_service.mojom.h"
#include "ui/gfx/rect.h"

namespace mojo {
namespace examples {

class SurfacesApp : public ApplicationDelegate,
                    public SurfaceClient,
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

    connection->ConnectToService("mojo:mojo_surfaces_service",
                                 &surfaces_service_);
    surfaces_service_->CreateSurfaceConnection(base::Bind(
        &SurfacesApp::SurfaceConnectionCreated, base::Unretained(this)));

    size_ = gfx::Size(800, 600);

    viewport_->Create(Size::From(size_));
    viewport_->Show();

    child_size_ = gfx::Size(size_.width() / 3, size_.height() / 2);
    connection->ConnectToService("mojo:mojo_surfaces_child_app", &child_one_);
    connection->ConnectToService("mojo:mojo_surfaces_child_gl_app",
                                 &child_two_);
    child_one_->ProduceFrame(Color::From(SK_ColorBLUE),
                             Size::From(child_size_),
                             base::Bind(&SurfacesApp::ChildOneProducedFrame,
                                        base::Unretained(this)));
    child_two_->ProduceFrame(Color::From(SK_ColorGREEN),
                             Size::From(child_size_),
                             base::Bind(&SurfacesApp::ChildTwoProducedFrame,
                                        base::Unretained(this)));
    return true;
  }

  void ChildOneProducedFrame(SurfaceIdPtr id) {
    child_one_id_ = id.To<cc::SurfaceId>();
  }

  void ChildTwoProducedFrame(SurfaceIdPtr id) {
    child_two_id_ = id.To<cc::SurfaceId>();
  }

  void Draw(int offset) {
    int bounced_offset = offset;
    if (offset > 200)
      bounced_offset = 400 - offset;
    embedder_->ProduceFrame(
        child_one_id_, child_two_id_, child_size_, size_, bounced_offset);
    base::MessageLoop::current()->PostDelayedTask(
        FROM_HERE,
        base::Bind(
            &SurfacesApp::Draw, base::Unretained(this), (offset + 2) % 400),
        base::TimeDelta::FromMilliseconds(50));
  }

  void SurfaceConnectionCreated(SurfacePtr surface, uint32_t id_namespace) {
    surface_ = surface.Pass();
    surface_.set_client(this);
    embedder_.reset(new Embedder(surface_.get()));
    allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));

    onscreen_id_ = allocator_->GenerateId();
    embedder_->SetSurfaceId(onscreen_id_);
    surface_->CreateSurface(SurfaceId::From(onscreen_id_), Size::From(size_));
    viewport_->SubmittedFrame(SurfaceId::From(onscreen_id_));
    Draw(10);
  }

  // SurfaceClient implementation.
  virtual void ReturnResources(Array<ReturnedResourcePtr> resources) OVERRIDE {
    DCHECK(!resources.size());
  }
  // NativeViewportClient implementation.
  virtual void OnCreated(uint64_t native_viewport_id) OVERRIDE {}
  virtual void OnBoundsChanged(mojo::SizePtr bounds) OVERRIDE {}
  virtual void OnDestroyed() OVERRIDE {}
  virtual void OnEvent(mojo::EventPtr event,
                       const mojo::Callback<void()>& callback) OVERRIDE {
    callback.Run();
  }

 private:
  SurfacesServicePtr surfaces_service_;
  SurfacePtr surface_;
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
}  // namespace mojo

MojoResult MojoMain(MojoHandle shell_handle) {
  mojo::ApplicationRunnerChromium runner(new mojo::examples::SurfacesApp);
  return runner.Run(shell_handle);
}
