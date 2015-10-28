// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/mus/example/wm/window_manager_application.h"

#include "components/mus/example/wm/container.h"
#include "components/mus/example/wm/window_layout.h"
#include "components/mus/example/wm/window_manager_impl.h"
#include "components/mus/public/cpp/util.h"
#include "components/mus/public/cpp/window.h"
#include "components/mus/public/cpp/window_tree_connection.h"
#include "components/mus/public/cpp/window_tree_host_factory.h"
#include "mojo/application/public/cpp/application_connection.h"

WindowManagerApplication::WindowManagerApplication()
    : root_(nullptr), window_count_(0) {}
WindowManagerApplication::~WindowManagerApplication() {}

mus::Window* WindowManagerApplication::GetWindowForContainer(
    Container container) {
  const mus::Id window_id = root_->connection()->GetConnectionId() << 16 |
                            static_cast<uint16_t>(container);
  return root_->GetChildById(window_id);
}

mus::Window* WindowManagerApplication::GetWindowById(mus::Id id) {
  return root_->GetChildById(id);
}

void WindowManagerApplication::Initialize(mojo::ApplicationImpl* app) {
  mus::mojom::WindowManagerPtr window_manager;
  requests_.push_back(new mojo::InterfaceRequest<mus::mojom::WindowManager>(
      mojo::GetProxy(&window_manager)));
  mus::CreateSingleWindowTreeHost(app, this, &host_, window_manager.Pass());
}

bool WindowManagerApplication::ConfigureIncomingConnection(
    mojo::ApplicationConnection* connection) {
  connection->AddService(this);
  return true;
}

void WindowManagerApplication::OnEmbed(mus::Window* root) {
  root_ = root;
  root_->AddObserver(this);
  CreateContainers();
  layout_.reset(
      new WindowLayout(GetWindowForContainer(Container::USER_WINDOWS)));

  window_manager_.reset(new WindowManagerImpl(this));
  for (auto request : requests_)
    window_manager_binding_.AddBinding(window_manager_.get(), request->Pass());
  requests_.clear();
}

void WindowManagerApplication::OnConnectionLost(
    mus::WindowTreeConnection* connection) {
  // TODO(sky): shutdown.
  NOTIMPLEMENTED();
}

void WindowManagerApplication::Create(
    mojo::ApplicationConnection* connection,
    mojo::InterfaceRequest<mus::mojom::WindowManager> request) {
  if (root_) {
    window_manager_binding_.AddBinding(window_manager_.get(), request.Pass());
  } else {
    requests_.push_back(
        new mojo::InterfaceRequest<mus::mojom::WindowManager>(request.Pass()));
  }
}

void WindowManagerApplication::OnWindowDestroyed(mus::Window* window) {
  DCHECK_EQ(window, root_);
  root_->RemoveObserver(this);
  // Delete the |window_manager_| here so that WindowManager doesn't have to
  // worry about the possibility of |root_| being null.
  window_manager_.reset();
  root_ = nullptr;
}

void WindowManagerApplication::CreateContainers() {
  for (uint16_t container =
           static_cast<uint16_t>(Container::ALL_USER_BACKGROUND);
       container < static_cast<uint16_t>(Container::COUNT); ++container) {
    mus::Window* window = root_->connection()->NewWindow();
    DCHECK_EQ(mus::LoWord(window->id()), container)
        << "Containers must be created before other windows!";
    window->SetBounds(root_->bounds());
    window->SetVisible(true);
    root_->AddChild(window);
  }
}

