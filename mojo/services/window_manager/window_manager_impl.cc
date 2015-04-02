// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_impl.h"

#include "mojo/services/window_manager/capture_controller.h"
#include "mojo/services/window_manager/focus_controller.h"
#include "mojo/services/window_manager/window_manager_app.h"
#include "third_party/mojo_services/src/view_manager/public/cpp/view.h"

using mojo::Callback;
using mojo::Id;

namespace window_manager {

WindowManagerImpl::WindowManagerImpl(WindowManagerApp* window_manager,
                                     bool from_vm)
    : window_manager_(window_manager), from_vm_(from_vm), binding_(this) {
  window_manager_->AddConnection(this);
  binding_.set_error_handler(this);
}

WindowManagerImpl::~WindowManagerImpl() {
  window_manager_->RemoveConnection(this);
}

void WindowManagerImpl::Bind(
    mojo::ScopedMessagePipeHandle window_manager_pipe) {
  binding_.Bind(window_manager_pipe.Pass());
}

void WindowManagerImpl::NotifyViewFocused(Id focused_id) {
  if (from_vm_ && observer_)
    observer_->OnFocusChanged(focused_id);
}

void WindowManagerImpl::NotifyWindowActivated(Id active_id) {
  if (from_vm_ && observer_)
    observer_->OnActiveWindowChanged(active_id);
}

void WindowManagerImpl::NotifyCaptureChanged(Id capture_id) {
  if (from_vm_ && observer_)
    observer_->OnCaptureChanged(capture_id);
}

void WindowManagerImpl::Embed(
    const mojo::String& url,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  window_manager_->Embed(url, services.Pass(), exposed_services.Pass());
}

void WindowManagerImpl::SetCapture(Id view,
                                   const Callback<void(bool)>& callback) {
  callback.Run(from_vm_ && window_manager_->IsReady() &&
               window_manager_->SetCapture(view));
}

void WindowManagerImpl::FocusWindow(Id view,
                                    const Callback<void(bool)>& callback) {
  callback.Run(from_vm_ && window_manager_->IsReady() &&
               window_manager_->FocusWindow(view));
}

void WindowManagerImpl::ActivateWindow(Id view,
                                       const Callback<void(bool)>& callback) {
  callback.Run(from_vm_ && window_manager_->IsReady() &&
               window_manager_->ActivateWindow(view));
}

void WindowManagerImpl::GetFocusedAndActiveViews(
    mojo::WindowManagerObserverPtr observer,
    const mojo::WindowManager::GetFocusedAndActiveViewsCallback& callback) {
  observer_ = observer.Pass();
  if (!window_manager_->focus_controller()) {
    // TODO(sky): add typedef for 0.
    callback.Run(0, 0, 0);
    return;
  }
  mojo::View* capture_view =
      window_manager_->capture_controller()->GetCapture();
  mojo::View* active_view =
      window_manager_->focus_controller()->GetActiveView();
  mojo::View* focused_view =
      window_manager_->focus_controller()->GetFocusedView();
  // TODO(sky): sanitize ids for client.
  callback.Run(capture_view ? capture_view->id() : 0,
               focused_view ? focused_view->id() : 0,
               active_view ? active_view->id() : 0);
}

void WindowManagerImpl::OnConnectionError() {
  delete this;
}

}  // namespace window_manager
