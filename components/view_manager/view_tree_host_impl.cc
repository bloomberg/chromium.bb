// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_tree_host_impl.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/view_tree_host_delegate.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace view_manager {

ViewTreeHostImpl::ViewTreeHostImpl(
    mojo::ViewTreeHostClientPtr client,
    ConnectionManager* connection_manager,
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state,
    const scoped_refptr<surfaces::SurfacesState>& surfaces_state)
    : delegate_(nullptr),
      connection_manager_(connection_manager),
      client_(client.Pass()),
      display_manager_(
          DisplayManager::Create(is_headless,
                                 app_impl,
                                 gpu_state,
                                 surfaces_state)) {
  display_manager_->Init(this);
}

ViewTreeHostImpl::~ViewTreeHostImpl() {
}

void ViewTreeHostImpl::Init(ViewTreeHostDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_ && root_)
    delegate_->OnDisplayInitialized();
}

ViewTreeImpl* ViewTreeHostImpl::GetViewTree() {
  return delegate_ ? delegate_->GetViewTree() : nullptr;
}

bool ViewTreeHostImpl::IsViewAttachedToRoot(const ServerView* view) const {
  return root_->Contains(view) && view != root_.get();
}

bool ViewTreeHostImpl::SchedulePaintIfInViewport(const ServerView* view,
                                                 const gfx::Rect& bounds) {
  if (root_->Contains(view)) {
    display_manager_->SchedulePaint(view, bounds);
    return true;
  }
  return false;
}

const mojo::ViewportMetrics& ViewTreeHostImpl::GetViewportMetrics() const {
  return display_manager_->GetViewportMetrics();
}

void ViewTreeHostImpl::UpdateTextInputState(const ui::TextInputState& state) {
  display_manager_->UpdateTextInputState(state);
}

void ViewTreeHostImpl::SetImeVisibility(bool visible) {
  display_manager_->SetImeVisibility(visible);
}

void ViewTreeHostImpl::SetSize(mojo::SizePtr size) {
  display_manager_->SetViewportSize(size.To<gfx::Size>());
}

void ViewTreeHostImpl::AddAccelerator(mojo::KeyboardCode keyboard_code,
                                      mojo::EventFlags flags) {
  connection_manager_->AddAccelerator(this, keyboard_code, flags);
}

void ViewTreeHostImpl::RemoveAccelerator(mojo::KeyboardCode keyboard_code,
                                         mojo::EventFlags flags) {
  connection_manager_->RemoveAccelerator(this, keyboard_code, flags);
}

ServerView* ViewTreeHostImpl::GetRootView() {
  return root_.get();
}

void ViewTreeHostImpl::OnEvent(mojo::EventPtr event) {
  connection_manager_->OnEvent(this, event.Pass());
}

void ViewTreeHostImpl::OnDisplayClosed() {
  if (delegate_)
    delegate_->OnDisplayClosed();
}

void ViewTreeHostImpl::OnViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerView(
        RootViewId(connection_manager_->GetAndAdvanceNextHostId())));
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
    root_->SetVisible(true);
    if (delegate_)
      delegate_->OnDisplayInitialized();
  } else {
    root_->SetBounds(gfx::Rect(new_metrics.size_in_pixels.To<gfx::Size>()));
  }
  // TODO(fsamuel): We shouldn't broadcast this to all connections but only
  // those within a window root.
  connection_manager_->ProcessViewportMetricsChanged(old_metrics, new_metrics);
}

}  // namespace view_manager
