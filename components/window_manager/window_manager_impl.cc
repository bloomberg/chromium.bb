// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/window_manager/window_manager_impl.h"

#include "components/window_manager/window_manager_app.h"

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

void WindowManagerImpl::Embed(
    const mojo::String& url,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::ServiceProviderPtr exposed_services) {
  window_manager_->Embed(url, services.Pass(), exposed_services.Pass());
}

void WindowManagerImpl::OnConnectionError() {
  delete this;
}

}  // namespace window_manager
