// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/surfaces/display_impl.h"

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/display.h"
#include "components/view_manager/surfaces/surfaces_context_provider.h"
#include "components/view_manager/surfaces/surfaces_output_surface.h"
#include "components/view_manager/surfaces/surfaces_scheduler.h"
#include "components/view_manager/surfaces/surfaces_service_application.h"
#include "mojo/converters/geometry/geometry_type_converters.h"
#include "mojo/converters/surfaces/surfaces_type_converters.h"

namespace surfaces {
namespace {
void CallCallback(const mojo::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}
}

DisplayImpl::DisplayImpl(DisplayDelegate* display_delegate,
                         const scoped_refptr<SurfacesState>& surfaces_state,
                         cc::SurfaceId cc_id,
                         mojo::ContextProviderPtr context_provider,
                         mojo::ResourceReturnerPtr returner,
                         mojo::InterfaceRequest<mojo::Display> display_request)
    : delegate_(display_delegate),
      surfaces_state_(surfaces_state),
      factory_(surfaces_state->manager(), this),
      cc_id_(cc_id),
      context_provider_(context_provider.Pass()),
      returner_(returner.Pass()),
      viewport_param_binding_(this),
      display_binding_(this, display_request.Pass()),
      connection_closed_(false) {
  delegate_->OnDisplayCreated(this);
  mojo::ViewportParameterListenerPtr viewport_parameter_listener;
  viewport_param_binding_.Bind(GetProxy(&viewport_parameter_listener));
  context_provider_->Create(
      viewport_parameter_listener.Pass(),
      base::Bind(&DisplayImpl::OnContextCreated, base::Unretained(this)));
  factory_.Create(cc_id_);
  display_binding_.set_connection_error_handler(
      base::Bind(&DisplayImpl::CloseConnection, base::Unretained(this)));
}

void DisplayImpl::CloseConnection() {
  if (connection_closed_)
    return;
  connection_closed_ = true;
  delegate_->OnDisplayConnectionClosed(this);
  delete this;
}

DisplayImpl::~DisplayImpl() {
  DCHECK(connection_closed_);
  if (display_) {
    factory_.Destroy(cc_id_);
    surfaces_state_->scheduler()->RemoveDisplay(display_.get());
    // By deleting the object after display_ is reset, OutputSurfaceLost can
    // know not to do anything (which would result in double delete).
    delete display_.release();
  }
}

void DisplayImpl::OnContextCreated(mojo::CommandBufferPtr gles2_client) {
  DCHECK(!display_);

  cc::RendererSettings settings;
  display_.reset(new cc::Display(this, surfaces_state_->manager(),
                                 nullptr, nullptr, settings));
  surfaces_state_->scheduler()->AddDisplay(display_.get());

  // TODO(brianderson): Reconcile with SurfacesScheduler crbug.com/476676
  cc::DisplayScheduler* null_display_scheduler = nullptr;
  display_->Initialize(make_scoped_ptr(new surfaces::DirectOutputSurface(
                           new surfaces::SurfacesContextProvider(
                               gles2_client.PassInterface().PassHandle()))),
                       null_display_scheduler);
  display_->Resize(last_submitted_frame_size_);

  display_->SetSurfaceId(cc_id_, 1.f);
  if (pending_frame_)
    Draw();
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
  last_submitted_frame_size_ = frame_size;
  display_->Resize(frame_size);
  factory_.SubmitFrame(cc_id_,
                       pending_frame_.To<scoped_ptr<cc::CompositorFrame>>(),
                       base::Bind(&CallCallback, pending_callback_));
  pending_frame_.reset();
  surfaces_state_->scheduler()->SetNeedsDraw();
  pending_callback_.reset();
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
  surfaces_state_->scheduler()->RemoveDisplay(display_.get());
  display_.reset();
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
  surfaces_state_->scheduler()->OnVSyncParametersUpdated(
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
