// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/view_manager/view_manager_root_impl.h"

#include "components/view_manager/connection_manager.h"
#include "components/view_manager/display_manager.h"
#include "components/view_manager/public/cpp/types.h"
#include "components/view_manager/view_manager_root_delegate.h"
#include "mojo/converters/geometry/geometry_type_converters.h"

namespace view_manager {

ViewManagerRootImpl::ViewManagerRootImpl(
    ConnectionManager* connection_manager,
    bool is_headless,
    mojo::ApplicationImpl* app_impl,
    const scoped_refptr<gles2::GpuState>& gpu_state)
    : delegate_(nullptr),
      connection_manager_(connection_manager),
      display_manager_(
          DisplayManager::Create(is_headless, app_impl, gpu_state)) {
  display_manager_->Init(this);
}

ViewManagerRootImpl::~ViewManagerRootImpl() {
}

void ViewManagerRootImpl::Init(ViewManagerRootDelegate* delegate) {
  delegate_ = delegate;
  if (delegate_ && root_)
    delegate_->OnDisplayInitialized();
}

ViewManagerServiceImpl* ViewManagerRootImpl::GetViewManagerService() {
  return delegate_ ? delegate_->GetViewManagerService() : nullptr;
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

void ViewManagerRootImpl::UpdateTextInputState(
    const ui::TextInputState& state) {
  display_manager_->UpdateTextInputState(state);
}

void ViewManagerRootImpl::SetImeVisibility(bool visible) {
  display_manager_->SetImeVisibility(visible);
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
  if (delegate_)
    delegate_->OnDisplayClosed();
}

void ViewManagerRootImpl::OnViewportMetricsChanged(
    const mojo::ViewportMetrics& old_metrics,
    const mojo::ViewportMetrics& new_metrics) {
  if (!root_) {
    root_.reset(connection_manager_->CreateServerView(
        RootViewId(connection_manager_->GetAndAdvanceNextRootId())));
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
