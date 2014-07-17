// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/window_manager/window_manager_app.h"

#include "base/message_loop/message_loop.h"
#include "mojo/aura/aura_init.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/cpp/view_manager/node.h"
#include "mojo/services/public/cpp/view_manager/view_manager.h"
#include "mojo/services/window_manager/window_manager_service_impl.h"
#include "ui/wm/core/capture_controller.h"

namespace mojo {

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, public:

WindowManagerApp::WindowManagerApp() : view_manager_(NULL), root_(NULL) {}
WindowManagerApp::~WindowManagerApp() {}

void WindowManagerApp::AddConnection(WindowManagerServiceImpl* connection) {
  DCHECK(connections_.find(connection) == connections_.end());
  connections_.insert(connection);
}

void WindowManagerApp::RemoveConnection(WindowManagerServiceImpl* connection) {
  DCHECK(connections_.find(connection) != connections_.end());
  connections_.erase(connection);
}

view_manager::Id WindowManagerApp::OpenWindow() {
  view_manager::Node* node = view_manager::Node::Create(view_manager_);
  root_->AddChild(node);
  return node->id();
}

void WindowManagerApp::SetCapture(view_manager::Id node) {
  capture_client_->capture_client()->SetCapture(GetWindowForNodeId(node));
  // TODO(beng): notify connected clients that capture has changed, probably
  //             by implementing some capture-client observer.
}

bool WindowManagerApp::IsReady() const {
  return view_manager_ && root_;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, ApplicationDelegate implementation:

void WindowManagerApp::Initialize(ApplicationImpl* impl) {
  aura_init_.reset(new AuraInit);
}

bool WindowManagerApp::ConfigureIncomingConnection(
    ApplicationConnection* connection) {
  connection->AddService<WindowManagerServiceImpl>(this);
  view_manager::ViewManager::ConfigureIncomingConnection(connection, this);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, view_manager::ViewManagerDelegate implementation:

void WindowManagerApp::OnRootAdded(view_manager::ViewManager* view_manager,
                                   view_manager::Node* root) {
  DCHECK(!view_manager_ && !root_);
  view_manager_ = view_manager;
  root_ = root;

  window_tree_host_.reset(new WindowTreeHostMojo(root_, this));

  capture_client_.reset(
      new wm::ScopedCaptureClient(window_tree_host_->window()));

  // TODO(beng): Create the universe.

  for (Connections::const_iterator it = connections_.begin();
       it != connections_.end(); ++it) {
    (*it)->NotifyReady();
  }
}

void WindowManagerApp::OnViewManagerDisconnected(
    view_manager::ViewManager* view_manager) {
  DCHECK_EQ(view_manager_, view_manager);
  view_manager_ = NULL;
  base::MessageLoop::current()->Quit();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, WindowTreeHostMojoDelegate implementation:

void WindowManagerApp::CompositorContentsChanged(const SkBitmap& bitmap) {
  // We draw nothing.
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// WindowManagerApp, private:

aura::Window* WindowManagerApp::GetWindowForNodeId(
    view_manager::Id node) const {
  NOTIMPLEMENTED();
  return NULL;
}

////////////////////////////////////////////////////////////////////////////////
// ApplicationDelegate, public:

// static
ApplicationDelegate* ApplicationDelegate::Create() {
  return new WindowManagerApp;
}

}  // namespace mojo
