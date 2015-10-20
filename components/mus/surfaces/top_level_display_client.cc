// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/surfaces/top_level_display_client.h"

#include "cc/output/compositor_frame.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/surface.h"
#include "components/mus/gles2/gpu_state.h"
#include "components/mus/surfaces/direct_output_surface.h"
#include "components/mus/surfaces/surfaces_context_provider.h"
#include "components/mus/surfaces/surfaces_scheduler.h"
#include "components/mus/surfaces/surfaces_state.h"

namespace mus {
namespace {
void CallCallback(const base::Closure& callback, cc::SurfaceDrawStatus status) {
  callback.Run();
}
}

TopLevelDisplayClient::TopLevelDisplayClient(
    gfx::AcceleratedWidget widget,
    const scoped_refptr<GpuState>& gpu_state,
    const scoped_refptr<SurfacesState>& surfaces_state)
    : surfaces_state_(surfaces_state),
      factory_(surfaces_state->manager(), this),
      cc_id_(static_cast<uint64_t>(surfaces_state->next_id_namespace()) << 32) {
  factory_.Create(cc_id_);

  display_.reset(new cc::Display(this, surfaces_state_->manager(), nullptr,
                                 nullptr, cc::RendererSettings()));
  surfaces_state_->scheduler()->AddDisplay(display_.get());

  // TODO(brianderson): Reconcile with SurfacesScheduler crbug.com/476676
  cc::DisplayScheduler* null_display_scheduler = nullptr;
  display_->Initialize(
      make_scoped_ptr(new DirectOutputSurface(
          new SurfacesContextProvider(this, widget, gpu_state))),
      null_display_scheduler);

  display_->Resize(last_submitted_frame_size_);

  // TODO(fsamuel): Plumb the proper device scale factor.
  display_->SetSurfaceId(cc_id_, 1.f /* device_scale_factor */);
}

TopLevelDisplayClient::~TopLevelDisplayClient() {
  factory_.Destroy(cc_id_);
  surfaces_state_->scheduler()->RemoveDisplay(display_.get());
  // By deleting the object after display_ is reset, OutputSurfaceLost can
  // know not to do anything (which would result in double delete).
  delete display_.release();
}

void TopLevelDisplayClient::SubmitCompositorFrame(
    scoped_ptr<cc::CompositorFrame> frame,
    const base::Closure& callback) {
  pending_frame_ = frame.Pass();

  last_submitted_frame_size_ =
      pending_frame_->delegated_frame_data->render_pass_list.back()
          ->output_rect.size();
  display_->Resize(last_submitted_frame_size_);
  factory_.SubmitCompositorFrame(cc_id_, pending_frame_.Pass(),
                                 base::Bind(&CallCallback, callback));
  surfaces_state_->scheduler()->SetNeedsDraw();
}

void TopLevelDisplayClient::CommitVSyncParameters(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {}

void TopLevelDisplayClient::OutputSurfaceLost() {
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
}

void TopLevelDisplayClient::SetMemoryPolicy(
    const cc::ManagedMemoryPolicy& policy) {}

void TopLevelDisplayClient::OnVSyncParametersUpdated(int64_t timebase,
                                                     int64_t interval) {
  surfaces_state_->scheduler()->OnVSyncParametersUpdated(
      base::TimeTicks::FromInternalValue(timebase),
      base::TimeDelta::FromInternalValue(interval));
}

void TopLevelDisplayClient::ReturnResources(
    const cc::ReturnedResourceArray& resources) {
  // TODO(fsamuel): Implement this.
}

void TopLevelDisplayClient::SetBeginFrameSource(
    cc::SurfaceId surface_id,
    cc::BeginFrameSource* begin_frame_source) {
  // TODO(tansell): Implement this.
  NOTIMPLEMENTED();
}

}  // namespace mus
