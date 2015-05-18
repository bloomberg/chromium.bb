// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/surfaces/display_impl.h"

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/display.h"
#include "components/surfaces/surfaces_context_provider.h"
#include "components/surfaces/surfaces_output_surface.h"
#include "components/surfaces/surfaces_scheduler.h"
#include "components/surfaces/surfaces_service_application.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace surfaces {
namespace {
void CallCallback(const mojo::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}
}

DisplayImpl::DisplayImpl(SurfacesServiceApplication* application,
                         cc::SurfaceManager* manager,
                         cc::SurfaceId cc_id,
                         SurfacesScheduler* scheduler,
                         mojo::ContextProviderPtr context_provider,
                         mojo::ResourceReturnerPtr returner,
                         mojo::InterfaceRequest<mojo::Display> display_request)
    : application_(application),
      manager_(manager),
      factory_(manager, this),
      cc_id_(cc_id),
      scheduler_(scheduler),
      context_provider_(context_provider.Pass()),
      returner_(returner.Pass()),
      viewport_param_binding_(this),
      display_binding_(this, display_request.Pass()) {
  application_->DisplayCreated(this);
  mojo::ViewportParameterListenerPtr viewport_parameter_listener;
  viewport_param_binding_.Bind(GetProxy(&viewport_parameter_listener));
  context_provider_->Create(
      viewport_parameter_listener.Pass(),
      base::Bind(&DisplayImpl::OnContextCreated, base::Unretained(this)));
}

void DisplayImpl::OnContextCreated(mojo::CommandBufferPtr gles2_client) {
  DCHECK(!display_);

  cc::RendererSettings settings;
  display_.reset(new cc::Display(this, manager_, nullptr, nullptr, settings));
  scheduler_->AddDisplay(display_.get());
  display_->Initialize(make_scoped_ptr(
      new surfaces::DirectOutputSurface(new surfaces::SurfacesContextProvider(
          gles2_client.PassInterface().PassHandle()))));

  factory_.Create(cc_id_);
  display_->SetSurfaceId(cc_id_, 1.f);
  if (pending_frame_)
    Draw();
}

DisplayImpl::~DisplayImpl() {
  if (display_) {
    factory_.Destroy(cc_id_);
    scheduler_->RemoveDisplay(display_.get());
    // By deleting the object after display_ is reset, OutputSurfaceLost can
    // know not to do anything (which would result in double delete).
    delete display_.release();
  }
  application_->DisplayDestroyed(this);
}

void DisplayImpl::SubmitFrame(mojo::FramePtr frame,
                              const SubmitFrameCallback& callback) {
  DCHECK(pending_callback_.is_null());
  pending_frame_ = frame.Pass();
  pending_callback_ = callback;
  if (display_)
    Draw();
}

void DisplayImpl::Draw() {
  gfx::Size frame_size =
      pending_frame_->passes[0]->output_rect.To<gfx::Rect>().size();
  display_->Resize(frame_size);
  factory_.SubmitFrame(cc_id_,
                       pending_frame_.To<scoped_ptr<cc::CompositorFrame>>(),
                       base::Bind(&CallCallback, pending_callback_));
  scheduler_->SetNeedsDraw();
  pending_callback_.reset();
}

void DisplayImpl::DisplayDamaged() {
}

void DisplayImpl::DidSwapBuffers() {
}

void DisplayImpl::DidSwapBuffersComplete() {
}

void DisplayImpl::CommitVSyncParameters(base::TimeTicks timebase,
                                        base::TimeDelta interval) {
}

void DisplayImpl::OutputSurfaceLost() {
  if (!display_)  // Shutdown case
    return;

  // If our OutputSurface is lost we can't draw until we get a new one. For now,
  // destroy the display and create a new one when our ContextProvider provides
  // a new one.
  // TODO: This is more violent than necessary - we could simply remove this
  // display from the scheduler's set and pass a new context in to the
  // OutputSurface. It should be able to reinitialize properly.
  scheduler_->RemoveDisplay(display_.get());
  display_.reset();
  factory_.Destroy(cc_id_);
  viewport_param_binding_.Close();
  mojo::ViewportParameterListenerPtr viewport_parameter_listener;
  viewport_param_binding_.Bind(GetProxy(&viewport_parameter_listener));
  context_provider_->Create(
      viewport_parameter_listener.Pass(),
      base::Bind(&DisplayImpl::OnContextCreated, base::Unretained(this)));
}

void DisplayImpl::SetMemoryPolicy(const cc::ManagedMemoryPolicy& policy) {
}

void DisplayImpl::OnVSyncParametersUpdated(int64_t timebase, int64_t interval) {
  scheduler_->OnVSyncParametersUpdated(
      base::TimeTicks::FromInternalValue(timebase),
      base::TimeDelta::FromInternalValue(interval));
}

void DisplayImpl::ReturnResources(const cc::ReturnedResourceArray& resources) {
  if (resources.empty())
    return;
  DCHECK(returner_);

  mojo::Array<mojo::ReturnedResourcePtr> ret(resources.size());
  for (size_t i = 0; i < resources.size(); ++i) {
    ret[i] = mojo::ReturnedResource::From(resources[i]);
  }
  returner_->ReturnResources(ret.Pass());
}

}  // namespace surfaces
