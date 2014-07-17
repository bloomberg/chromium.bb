// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_service_impl.h"

#include "mojo/services/window_manager/window_manager_app.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// WindowManagerServiceImpl, public:

WindowManagerServiceImpl::WindowManagerServiceImpl(
    ApplicationConnection* connection,
    WindowManagerApp* window_manager)
    : window_manager_(window_manager) {
  window_manager_->AddConnection(this);
}

WindowManagerServiceImpl::~WindowManagerServiceImpl() {
  window_manager_->RemoveConnection(this);
}

void WindowManagerServiceImpl::NotifyReady() {
  client()->OnWindowManagerReady();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerServiceImpl, WindowManager implementation:

void WindowManagerServiceImpl::OpenWindow(
    const Callback<void(view_manager::Id)>& callback) {
  view_manager::Id id = window_manager_->OpenWindow();
  callback.Run(id);
}

void WindowManagerServiceImpl::SetCapture(
    view_manager::Id node,
    const Callback<void(bool)>& callback) {
  window_manager_->SetCapture(node);
  callback.Run(true);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerServiceImpl, InterfaceImpl overrides:

void WindowManagerServiceImpl::OnConnectionEstablished() {
  // If the connection was established prior to the window manager being
  // embedded by the view manager, |window_manager_|'s ViewManagerDelegate
  // impl will call NotifyReady() when it is.
  if (window_manager_->IsReady())
    NotifyReady();
}

}  // namespace mojo
