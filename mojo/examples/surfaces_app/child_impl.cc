// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/surfaces_app/child_impl.h"

#include "base/bind.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/delegated_frame_data.h"
#include "cc/quads/render_pass.h"
#include "cc/quads/solid_color_draw_quad.h"
#include "mojo/examples/surfaces_app/surfaces_util.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/cpp/geometry/geometry_type_converters.h"
#include "mojo/services/public/cpp/surfaces/surfaces_type_converters.h"
#include "mojo/services/public/interfaces/surfaces/surface_id.mojom.h"
#include "mojo/services/public/interfaces/surfaces/surfaces.mojom.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace mojo {
namespace examples {

using cc::RenderPass;
using cc::DrawQuad;
using cc::SolidColorDrawQuad;
using cc::DelegatedFrameData;
using cc::CompositorFrame;

class SurfaceClientImpl : public surfaces::SurfaceClient {
 public:
  explicit SurfaceClientImpl(ChildImpl* impl) : impl_(impl) {}
  virtual ~SurfaceClientImpl() {}

  // surfaces::SurfaceClient implementation
  virtual void SetIdNamespace(uint32_t id_namespace) OVERRIDE {
    impl_->SetIdNamespace(id_namespace);
  }

  virtual void ReturnResources(
      Array<surfaces::ReturnedResourcePtr> resources) OVERRIDE {
    DCHECK(!resources.size());
  }

 private:
  ChildImpl* impl_;
};

ChildImpl::ChildImpl(ApplicationConnection* connection, Context* context)
    : surface_client_(new SurfaceClientImpl(this)) {
  context->ShellConnection("mojo:mojo_surfaces_service")
      ->ConnectToService(&surface_);
  surface_.set_client(surface_client_.get());
}

ChildImpl::~ChildImpl() {
  surface_->DestroySurface(mojo::surfaces::SurfaceId::From(id_));
}

void ChildImpl::ProduceFrame(
    surfaces::ColorPtr color,
    SizePtr size,
    const mojo::Callback<void(surfaces::SurfaceIdPtr id)>& callback) {
  color_ = color.To<SkColor>();
  size_ = size.To<gfx::Size>();
  produce_callback_ = callback;
  if (allocator_)
    Draw();
}

void ChildImpl::SetIdNamespace(uint32_t id_namespace) {
  allocator_.reset(new cc::SurfaceIdAllocator(id_namespace));
  if (!produce_callback_.is_null())
    Draw();
}

void ChildImpl::Draw() {
  id_ = allocator_->GenerateId();
  surface_->CreateSurface(mojo::surfaces::SurfaceId::From(id_),
                          mojo::Size::From(size_));
  gfx::Rect rect(size_);
  RenderPass::Id id(1, 1);
  scoped_ptr<RenderPass> pass = RenderPass::Create();
  pass->SetNew(id, rect, rect, gfx::Transform());

  CreateAndAppendSimpleSharedQuadState(pass.get(), gfx::Transform(), size_);

  SolidColorDrawQuad* color_quad =
      pass->CreateAndAppendDrawQuad<SolidColorDrawQuad>();
  bool force_anti_aliasing_off = false;
  color_quad->SetNew(pass->shared_quad_state_list.back(),
                     rect,
                     rect,
                     color_,
                     force_anti_aliasing_off);

  scoped_ptr<DelegatedFrameData> delegated_frame_data(new DelegatedFrameData);
  delegated_frame_data->render_pass_list.push_back(pass.Pass());

  scoped_ptr<CompositorFrame> frame(new CompositorFrame);
  frame->delegated_frame_data = delegated_frame_data.Pass();

  surface_->SubmitFrame(mojo::surfaces::SurfaceId::From(id_),
                        mojo::surfaces::Frame::From(*frame));
  produce_callback_.Run(surfaces::SurfaceId::From(id_));
}

}  // namespace examples
}  // namespace mojo
