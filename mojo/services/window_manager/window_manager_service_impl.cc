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

void WindowManagerServiceImpl::NotifyNodeFocused(
    view_manager::Id new_focused_id,
    view_manager::Id old_focused_id) {
  client()->OnFocusChanged(old_focused_id, new_focused_id);
}

void WindowManagerServiceImpl::NotifyWindowActivated(
    view_manager::Id new_active_id,
    view_manager::Id old_active_id) {
  client()->OnActiveWindowChanged(old_active_id, new_active_id);
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerServiceImpl, WindowManager implementation:

void WindowManagerServiceImpl::OpenWindow(
    const Callback<void(view_manager::Id)>& callback) {
  bool success = window_manager_->IsReady();
  if (success) {
    view_manager::Id id = window_manager_->OpenWindow();
    callback.Run(id);
  } else {
    // TODO(beng): perhaps should take an error code for this.
    callback.Run(0);
  }
}

void WindowManagerServiceImpl::OpenWindowWithURL(
    const String& url,
    const Callback<void(view_manager::Id)>& callback) {
  bool success = window_manager_->IsReady();
  if (success) {
    view_manager::Id id = window_manager_->OpenWindowWithURL(url);
    callback.Run(id);
  } else {
    // TODO(beng): perhaps should take an error code for this.
    callback.Run(0);
  }
}

void WindowManagerServiceImpl::SetCapture(
    view_manager::Id node,
    const Callback<void(bool)>& callback) {
  bool success = window_manager_->IsReady();
  if (success)
    window_manager_->SetCapture(node);
  callback.Run(success);
}

void WindowManagerServiceImpl::FocusWindow(
    view_manager::Id node,
    const Callback<void(bool)>& callback) {
  bool success = window_manager_->IsReady();
  if (success)
    window_manager_->FocusWindow(node);
  callback.Run(success);
}

void WindowManagerServiceImpl::ActivateWindow(
    view_manager::Id node,
    const Callback<void(bool)>& callback) {
  bool success = window_manager_->IsReady();
  if (success)
    window_manager_->ActivateWindow(node);
  callback.Run(success);
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
