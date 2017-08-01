// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/surface_tree_host.h"

#include <algorithm>

#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "cc/output/compositor_frame.h"
#include "cc/output/layer_tree_frame_sink.h"
#include "components/exo/layer_tree_frame_sink_holder.h"
#include "components/exo/surface.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/path.h"

namespace exo {

namespace {

class CustomWindowTargeter : public aura::WindowTargeter {
 public:
  explicit CustomWindowTargeter(SurfaceTreeHost* surface_tree_host)
      : surface_tree_host_(surface_tree_host) {}
  ~CustomWindowTargeter() override = default;

  // Overridden from aura::WindowTargeter:
  bool EventLocationInsideBounds(aura::Window* window,
                                 const ui::LocatedEvent& event) const override {
    if (window != surface_tree_host_->host_window())
      return aura::WindowTargeter::EventLocationInsideBounds(window, event);

    Surface* surface = surface_tree_host_->root_surface();
    if (!surface)
      return false;

    gfx::Point local_point = event.location();

    if (window->parent())
      aura::Window::ConvertPointToTarget(window->parent(), window,
                                         &local_point);
    aura::Window::ConvertPointToTarget(window, surface->window(), &local_point);
    return surface->HitTestRect(gfx::Rect(local_point, gfx::Size(1, 1)));
  }

  ui::EventTarget* FindTargetForEvent(ui::EventTarget* root,
                                      ui::Event* event) override {
    aura::Window* window = static_cast<aura::Window*>(root);
    if (window != surface_tree_host_->host_window())
      return aura::WindowTargeter::FindTargetForEvent(root, event);
    ui::EventTarget* target =
        aura::WindowTargeter::FindTargetForEvent(root, event);
    // Do not accept events in SurfaceTreeHost window.
    return target != root ? target : nullptr;
  }

 private:
  SurfaceTreeHost* const surface_tree_host_;

  DISALLOW_COPY_AND_ASSIGN(CustomWindowTargeter);
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// SurfaceTreeHost, public:

SurfaceTreeHost::SurfaceTreeHost(const std::string& window_name,
                                 aura::WindowDelegate* window_delegate) {
  host_window_ = base::MakeUnique<aura::Window>(window_delegate);
  host_window_->SetType(aura::client::WINDOW_TYPE_CONTROL);
  host_window_->SetName(window_name);
  host_window_->Init(ui::LAYER_SOLID_COLOR);
  host_window_->set_owned_by_parent(false);
  host_window_->SetEventTargeter(base::MakeUnique<CustomWindowTargeter>(this));
  layer_tree_frame_sink_holder_ = base::MakeUnique<LayerTreeFrameSinkHolder>(
      this, host_window_->CreateLayerTreeFrameSink());
  aura::Env::GetInstance()->context_factory()->AddObserver(this);
}

SurfaceTreeHost::~SurfaceTreeHost() {
  aura::Env::GetInstance()->context_factory()->RemoveObserver(this);
  SetRootSurface(nullptr);
  if (host_window_->layer()->GetCompositor()) {
    host_window_->layer()->GetCompositor()->vsync_manager()->RemoveObserver(
        this);
  }
}

void SurfaceTreeHost::SetRootSurface(Surface* root_surface) {
  if (root_surface == root_surface_)
    return;

  if (root_surface_) {
    root_surface_->window()->Hide();
    host_window_->RemoveChild(root_surface_->window());
    host_window_->SetBounds(
        gfx::Rect(host_window_->bounds().origin(), gfx::Size()));
    root_surface_->SetSurfaceDelegate(nullptr);
    root_surface_ = nullptr;

    active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                   frame_callbacks_);
    // Call all frame callbacks with a null frame time to indicate that they
    // have been cancelled.
    while (!active_frame_callbacks_.empty()) {
      active_frame_callbacks_.front().Run(base::TimeTicks());
      active_frame_callbacks_.pop_front();
    }

    swapping_presentation_callbacks_.splice(
        swapping_presentation_callbacks_.end(), presentation_callbacks_);
    swapped_presentation_callbacks_.splice(
        swapped_presentation_callbacks_.end(),
        swapping_presentation_callbacks_);
    // Call all presentation callbacks with a null presentation time to indicate
    // that they have been cancelled.
    while (!swapped_presentation_callbacks_.empty()) {
      swapped_presentation_callbacks_.front().Run(base::TimeTicks(),
                                                  base::TimeDelta());
      swapped_presentation_callbacks_.pop_front();
    }
  }

  if (root_surface) {
    root_surface_ = root_surface;
    root_surface_->SetSurfaceDelegate(this);
    host_window_->AddChild(root_surface_->window());
    host_window_->SetBounds(gfx::Rect(host_window_->bounds().origin(),
                                      root_surface_->content_size()));
    root_surface_->window()->Show();
  }
}

bool SurfaceTreeHost::HasHitTestMask() const {
  return root_surface_ ? root_surface_->HasHitTestMask() : false;
}

void SurfaceTreeHost::GetHitTestMask(gfx::Path* mask) const {
  if (root_surface_)
    root_surface_->GetHitTestMask(mask);
}

gfx::Rect SurfaceTreeHost::GetHitTestBounds() const {
  return root_surface_ ? root_surface_->GetHitTestBounds() : gfx::Rect();
}

gfx::NativeCursor SurfaceTreeHost::GetCursor(const gfx::Point& point) const {
  return root_surface_ ? root_surface_->GetCursor() : ui::CursorType::kNull;
}

void SurfaceTreeHost::DidReceiveCompositorFrameAck() {
  active_frame_callbacks_.splice(active_frame_callbacks_.end(),
                                 frame_callbacks_);
  swapping_presentation_callbacks_.splice(
      swapping_presentation_callbacks_.end(), presentation_callbacks_);
  UpdateNeedsBeginFrame();
}

void SurfaceTreeHost::SetBeginFrameSource(
    viz::BeginFrameSource* begin_frame_source) {
  if (needs_begin_frame_) {
    DCHECK(begin_frame_source_);
    begin_frame_source_->RemoveObserver(this);
    needs_begin_frame_ = false;
  }
  begin_frame_source_ = begin_frame_source;
  UpdateNeedsBeginFrame();
}

void SurfaceTreeHost::UpdateNeedsBeginFrame() {
  if (!begin_frame_source_)
    return;
  bool needs_begin_frame = !active_frame_callbacks_.empty();
  if (needs_begin_frame == needs_begin_frame_)
    return;
  needs_begin_frame_ = needs_begin_frame;
  if (needs_begin_frame_)
    begin_frame_source_->AddObserver(this);
  else
    begin_frame_source_->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void SurfaceTreeHost::OnSurfaceCommit() {
  SubmitCompositorFrame(Surface::FRAME_TYPE_COMMIT);
}

bool SurfaceTreeHost::IsSurfaceSynchronized() const {
  // To host a surface tree, the root surface has to be desynchronized.
  DCHECK(root_surface_);
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:

void SurfaceTreeHost::OnWindowAddedToRootWindow(aura::Window* window) {
  DCHECK_EQ(window, host_window());
  window->layer()->GetCompositor()->vsync_manager()->AddObserver(this);
}

void SurfaceTreeHost::OnWindowRemovingFromRootWindow(aura::Window* window,
                                                     aura::Window* new_root) {
  DCHECK_EQ(window, host_window());
  window->layer()->GetCompositor()->vsync_manager()->RemoveObserver(this);
}

void SurfaceTreeHost::OnWindowDestroying(aura::Window* window) {
  DCHECK_EQ(window, host_window());
  window->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// cc::BeginFrameObserverBase overrides:

bool SurfaceTreeHost::OnBeginFrameDerivedImpl(const viz::BeginFrameArgs& args) {
  current_begin_frame_ack_ =
      viz::BeginFrameAck(args.source_id, args.sequence_number, false);
  while (!active_frame_callbacks_.empty()) {
    active_frame_callbacks_.front().Run(args.frame_time);
    active_frame_callbacks_.pop_front();
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// ui::CompositorVSyncManager::Observer overrides:

void SurfaceTreeHost::OnUpdateVSyncParameters(base::TimeTicks timebase,
                                              base::TimeDelta interval) {
  // Use current time if platform doesn't provide an accurate timebase.
  if (timebase.is_null())
    timebase = base::TimeTicks::Now();
  while (!swapped_presentation_callbacks_.empty()) {
    swapped_presentation_callbacks_.front().Run(timebase, interval);
    swapped_presentation_callbacks_.pop_front();
  }
  // VSync parameters updates are generated at the start of a new swap. Move
  // the swapping presentation callbacks to swapped callbacks so they fire
  // at the next VSync parameters update as that will contain the presentation
  // time for the previous frame.
  swapped_presentation_callbacks_.splice(swapped_presentation_callbacks_.end(),
                                         swapping_presentation_callbacks_);
}

////////////////////////////////////////////////////////////////////////////////
// ui::ContextFactoryObserver overrides:

void SurfaceTreeHost::OnLostResources() {
  if (!host_window_->GetSurfaceId().is_valid())
    return;
  root_surface_->RecreateResources(layer_tree_frame_sink_holder_.get());
  SubmitCompositorFrame(Surface::FRAME_TYPE_RECREATED_RESOURCES);
}

void SurfaceTreeHost::SubmitCompositorFrame(Surface::FrameType frame_type) {
  DCHECK(root_surface_);
  cc::CompositorFrame frame;
  // If we commit while we don't have an active BeginFrame, we acknowledge a
  // manual one.
  if (current_begin_frame_ack_.sequence_number ==
      viz::BeginFrameArgs::kInvalidFrameNumber) {
    current_begin_frame_ack_ = viz::BeginFrameAck::CreateManualAckWithDamage();
  } else {
    current_begin_frame_ack_.has_damage = true;
  }

  frame.metadata.begin_frame_ack = current_begin_frame_ack_;
  frame.metadata.device_scale_factor =
      host_window_->layer()->device_scale_factor();
  const int kRenderPassId = 1;
  std::unique_ptr<cc::RenderPass> render_pass = cc::RenderPass::Create();
  render_pass->SetNew(kRenderPassId, gfx::Rect(), gfx::Rect(),
                      gfx::Transform());
  frame.render_pass_list.push_back(std::move(render_pass));
  root_surface_->CommitSurfaceHierarchy(
      gfx::Point(), frame_type, layer_tree_frame_sink_holder_.get(), &frame,
      &frame_callbacks_, &presentation_callbacks_);
  frame.render_pass_list.back()->output_rect =
      gfx::Rect(root_surface_->content_size());
  host_window_->SetBounds(gfx::Rect(host_window_->bounds().origin(),
                                    root_surface_->content_size()));
  host_window_->layer()->SetFillsBoundsOpaquely(
      root_surface_->FillsBoundsOpaquely());
  layer_tree_frame_sink_holder_->frame_sink()->SubmitCompositorFrame(
      std::move(frame));

  if (current_begin_frame_ack_.sequence_number !=
      viz::BeginFrameArgs::kInvalidFrameNumber) {
    if (!current_begin_frame_ack_.has_damage) {
      layer_tree_frame_sink_holder_->frame_sink()->DidNotProduceFrame(
          current_begin_frame_ack_);
    }
    current_begin_frame_ack_.sequence_number =
        viz::BeginFrameArgs::kInvalidFrameNumber;
    if (begin_frame_source_)
      begin_frame_source_->DidFinishFrame(this);
  }
}

}  // namespace exo
