// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/wm_application_delegate.h"

#include "components/mus/example/wm/wm.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "mojo/application/public/cpp/application_connection.h"

WMApplicationDelegate::WMApplicationDelegate() {}

WMApplicationDelegate::~WMApplicationDelegate() {}

void WMApplicationDelegate::Initialize(mojo::ApplicationImpl* app) {
  wm_.reset(new WM);
  mus::CreateSingleViewTreeHost(app, wm_.get(), &host_);
}

bool WMApplicationDelegate::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void WMApplicationDelegate::Create(mojo::ApplicationConnection* connection,
                                   mojo::InterfaceRequest<mojom::WM> request) {
  wm_bindings_.AddBinding(wm_.get(), request.Pass());
}
