// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_root_impl.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/public/cpp/types.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace view_manager {

ViewManagerRootImpl::ViewManagerRootImpl(
    const ViewId& root_view_id,
    ConnectionManager* connection_manager,
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state)
    : connection_manager_(connection_manager),
      root_(connection_manager->CreateServerView(root_view_id)),
      display_manager_(
          DisplayManager::Create(is_headless, app_impl, gpu_state)) {
}

ViewManagerRootImpl::~ViewManagerRootImpl() {
}

void ViewManagerRootImpl::Init() {
  root_->SetBounds(gfx::Rect(800, 600));
  root_->SetVisible(true);
  display_manager_->Init(this);
}

bool ViewManagerRootImpl::IsViewAttachedToRoot(const ServerView* view) const {
  return root_->Contains(view) && view != root_.get();
}

bool ViewManagerRootImpl::SchedulePaintIfInViewport(const ServerView* view,
                                                    const gfx::Rect& bounds) {
  if (root_->Contains(view)) {
    display_manager_->SchedulePaint(view, bounds);
    return true;
  }
  return false;
}

const mojo::ViewportMetrics& ViewManagerRootImpl::GetViewportMetrics() const {
  return display_manager_->GetViewportMetrics();
}

void ViewManagerRootImpl::SetViewManagerRootClient(
    mojo::ViewManagerRootClientPtr client) {
  client_ = client.Pass();
}

void ViewManagerRootImpl::SetViewportSize(mojo::SizePtr size) {
  display_manager_->SetViewportSize(size.To<gfx::Size>());
}

void ViewManagerRootImpl::CloneAndAnimate(mojo::Id transport_view_id) {
  connection_manager_->CloneAndAnimate(
      ViewIdFromTransportId(transport_view_id));
}

void ViewManagerRootImpl::AddAccelerator(mojo::KeyboardCode keyboard_code,
                                         mojo::EventFlags flags) {
  connection_manager_->AddAccelerator(this, keyboard_code, flags);
}

void ViewManagerRootImpl::RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                                            mojo::EventFlags flags) {
  connection_manager_->RemoveAccelerator(this, keyboard_code, flags);
}

ServerView* ViewManagerRootImpl::GetRootView() {
  return root_.get();
}

void ViewManagerRootImpl::OnEvent(mojo::EventPtr event) {
  connection_manager_->OnEvent(this, event.Pass());
}

void ViewManagerRootImpl::OnDisplayClosed() {
  connection_manager_->OnDisplayClosed();
}

void ViewManagerRootImpl::OnViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  // TODO(fsamuel: We shouldn't broadcast this to all connections but only those
  // within a window root.
  connection_manager_->ProcessViewportMetricsChanged(old_metrics, new_metrics);
}

}  // namespace view_manager
