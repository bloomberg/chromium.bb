// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/surfaces/onscreen_display_client.h"

#include "base/trace_event/trace_event.h"
#include "cc/output/output_surface.h"
#include "cc/surfaces/surface_display_output_surface.h"
#include "cc/surfaces/surface_factory.h"
#include "cc/surfaces/surface_manager.h"

namespace cc {

OnscreenDisplayClient::OnscreenDisplayClient(
    scoped_ptr<OutputSurface> output_surface,
    SurfaceManager* manager,
    SharedBitmapManager* bitmap_manager,
    gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
    const RendererSettings& settings,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner)
    : output_surface_(output_surface.Pass()),
      display_(new Display(this,
                           manager,
                           bitmap_manager,
                           gpu_memory_buffer_manager,
                           settings)),
      task_runner_(task_runner),
      scheduled_draw_(false),
      output_surface_lost_(false),
      deferred_draw_(false),
      pending_frames_(0),
      weak_ptr_factory_(this) {
}

OnscreenDisplayClient::~OnscreenDisplayClient() {
}

bool OnscreenDisplayClient::Initialize() {
  return display_->Initialize(output_surface_.Pass());
}

void OnscreenDisplayClient::CommitVSyncParameters(base::TimeTicks timebase,
                                                  base::TimeDelta interval) {
  surface_display_output_surface_->ReceivedVSyncParameters(timebase, interval);
}

void OnscreenDisplayClient::DisplayDamaged() {
  if (scheduled_draw_ || deferred_draw_)
    return;
  TRACE_EVENT0("content", "OnscreenDisplayClient::DisplayDamaged");
  if (pending_frames_ >= display_->GetMaxFramesPending()) {
    deferred_draw_ = true;
  } else {
    ScheduleDraw();
  }
}

void OnscreenDisplayClient::ScheduleDraw() {
  DCHECK(!deferred_draw_);
  DCHECK(!scheduled_draw_);
  scheduled_draw_ = true;
  task_runner_->PostTask(FROM_HERE, base::Bind(&OnscreenDisplayClient::Draw,
                                               weak_ptr_factory_.GetWeakPtr()));
}

void OnscreenDisplayClient::OutputSurfaceLost() {
  output_surface_lost_ = true;
  surface_display_output_surface_->DidLoseOutputSurface();
}

void OnscreenDisplayClient::Draw() {
  TRACE_EVENT0("content", "OnscreenDisplayClient::Draw");
  if (output_surface_lost_)
    return;
  scheduled_draw_ = false;
  display_->Draw();
}

void OnscreenDisplayClient::DidSwapBuffers() {
  pending_frames_++;
}

void OnscreenDisplayClient::DidSwapBuffersComplete() {
  pending_frames_--;
  if ((pending_frames_ < display_->GetMaxFramesPending()) && deferred_draw_) {
    deferred_draw_ = false;
    ScheduleDraw();
  }
}

void OnscreenDisplayClient::SetMemoryPolicy(const ManagedMemoryPolicy& policy) {
  surface_display_output_surface_->SetMemoryPolicy(policy);
}

}  // namespace cc
