
// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_service2_impl.h"

#include "mojo/services/window_manager/window_manager_app.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// WindowManagerService2Impl, public:

WindowManagerService2Impl::WindowManagerService2Impl(
    WindowManagerApp* window_manager)
    : window_manager_(window_manager) {
  window_manager_->AddConnection(this);
}

WindowManagerService2Impl::~WindowManagerService2Impl() {
  window_manager_->RemoveConnection(this);
}

void WindowManagerService2Impl::NotifyReady() {
  client()->OnWindowManagerReady();
}

void WindowManagerService2Impl::NotifyViewFocused(Id new_focused_id,
                                                  Id old_focused_id) {
  client()->OnFocusChanged(old_focused_id, new_focused_id);
}

void WindowManagerService2Impl::NotifyWindowActivated(Id new_active_id,
                                                      Id old_active_id) {
  client()->OnActiveWindowChanged(old_active_id, new_active_id);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerService2Impl, WindowManager implementation:

void WindowManagerService2Impl::SetCapture(
    Id view,
    const Callback<void(bool)>& callback) {
  bool success = window_manager_->IsReady();
  if (success)
    window_manager_->SetCapture(view);
  callback.Run(success);
}

void WindowManagerService2Impl::FocusWindow(
    Id view,
    const Callback<void(bool)>& callback) {
  bool success = window_manager_->IsReady();
  if (success)
    window_manager_->FocusWindow(view);
  callback.Run(success);
}

void WindowManagerService2Impl::ActivateWindow(
    Id view,
    const Callback<void(bool)>& callback) {
  bool success = window_manager_->IsReady();
  if (success)
    window_manager_->ActivateWindow(view);
  callback.Run(success);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerService2Impl, InterfaceImpl overrides:

void WindowManagerService2Impl::OnConnectionEstablished() {
  // If the connection was established prior to the window manager being
  // embedded by the view manager, |window_manager_|'s ViewManagerDelegate
  // impl will call NotifyReady() when it is.
  if (window_manager_->IsReady())
    NotifyReady();
}

}  // namespace mojo
