// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_application.h"

#include "components/mus/example/wm/container.h"
#include "components/mus/example/wm/window_manager_impl.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/view.h"
#include "components/mus/public/cpp/view_tree_connection.h"
#include "components/mus/public/cpp/view_tree_host_factory.h"
#include "mojo/application/public/cpp/application_connection.h"

WindowManagerApplication::WindowManagerApplication()
    : root_(nullptr), window_count_(0) {}
WindowManagerApplication::~WindowManagerApplication() {}

void WindowManagerApplication::Initialize(mojo::ApplicationImpl* app) {
  mus::CreateSingleViewTreeHost(app, this, &host_);
}

bool WindowManagerApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void WindowManagerApplication::OnEmbed(mus::View* root) {
  root_ = root;
  CreateContainers();

  for (auto request : requests_)
    new WindowManagerImpl(this, request->Pass());
}

void WindowManagerApplication::OnConnectionLost(
    mus::ViewTreeConnection* connection) {
  // TODO(sky): shutdown.
  NOTIMPLEMENTED();
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mus::mojom::WindowManager> request) {
  if (root_) {
    new WindowManagerImpl(this, request.Pass());
  } else {
    requests_.push_back(
        new mojo::InterfaceRequest<mus::mojom::WindowManager>(request.Pass()));
  }
}

void WindowManagerApplication::CreateContainers() {
  for (uint16 container = static_cast<uint16>(Container::ALL_USER_BACKGROUND);
       container < static_cast<uint16>(Container::COUNT); ++container) {
    mus::View* view = root_->connection()->CreateView();
    DCHECK_EQ(mus::LoWord(view->id()), container)
        << "Containers must be created before other views!";
    view->SetBounds(root_->bounds());
    view->SetVisible(true);
    root_->AddChild(view);
  }
}

