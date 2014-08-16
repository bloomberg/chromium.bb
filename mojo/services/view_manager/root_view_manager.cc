// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_view_manager.h"

#include "base/auto_reset.h"
#include "base/scoped_observer.h"
#include "mojo/public/cpp/application/application_connection.h"
#include "mojo/services/public/interfaces/gpu/gpu.mojom.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "mojo/services/view_manager/root_view_manager_delegate.h"
#include "mojo/services/view_manager/screen_impl.h"
#include "mojo/services/view_manager/window_tree_host_impl.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace mojo {
namespace service {

// TODO(sky): Remove once aura is removed from the service.
class FocusClientImpl : public aura::client::FocusClient {
 public:
  FocusClientImpl() {}
  virtual ~FocusClientImpl() {}

 private:
  // Overridden from aura::client::FocusClient:
  virtual void AddObserver(
      aura::client::FocusChangeObserver* observer) OVERRIDE {}
  virtual void RemoveObserver(
      aura::client::FocusChangeObserver* observer) OVERRIDE {}
  virtual void FocusWindow(aura::Window* window) OVERRIDE {}
  virtual void ResetFocusWithinActiveWindow(aura::Window* window) OVERRIDE {}
  virtual aura::Window* GetFocusedWindow() OVERRIDE { return NULL; }

  DISALLOW_COPY_AND_ASSIGN(FocusClientImpl);
};

class WindowTreeClientImpl : public aura::client::WindowTreeClient {
 public:
  explicit WindowTreeClientImpl(aura::Window* window) : window_(window) {
    aura::client::SetWindowTreeClient(window_, this);
  }

  virtual ~WindowTreeClientImpl() {
    aura::client::SetWindowTreeClient(window_, NULL);
  }

  // Overridden from aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    if (!capture_client_) {
      capture_client_.reset(
          new aura::client::DefaultCaptureClient(window_->GetRootWindow()));
    }
    return window_;
  }

 private:
  aura::Window* window_;

  scoped_ptr<aura::client::DefaultCaptureClient> capture_client_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientImpl);
};

RootViewManager::RootViewManager(
    ApplicationConnection* app_connection,
    RootNodeManager* root_node,
    RootViewManagerDelegate* delegate,
    const Callback<void()>& native_viewport_closed_callback)
    : delegate_(delegate),
      root_node_manager_(root_node),
      in_setup_(false) {
  screen_.reset(ScreenImpl::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  NativeViewportPtr viewport;
  app_connection->ConnectToService(
      "mojo:mojo_native_viewport_service", &viewport);
  GpuPtr gpu_service;
  // TODO(jamesr): Should be mojo:mojo_gpu_service
  app_connection->ConnectToService("mojo:mojo_native_viewport_service",
                                   &gpu_service);
  window_tree_host_.reset(new WindowTreeHostImpl(
      viewport.Pass(),
      gpu_service.Pass(),
      gfx::Rect(800, 600),
      base::Bind(&RootViewManager::OnCompositorCreated, base::Unretained(this)),
      native_viewport_closed_callback,
      base::Bind(&RootNodeManager::DispatchNodeInputEventToWindowManager,
                 base::Unretained(root_node_manager_))));
}

RootViewManager::~RootViewManager() {
  window_tree_client_.reset();
  window_tree_host_.reset();
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, NULL);
}

void RootViewManager::OnCompositorCreated() {
  base::AutoReset<bool> resetter(&in_setup_, true);
  window_tree_host_->InitHost();

  aura::Window* root = root_node_manager_->root()->window();
  window_tree_host_->window()->AddChild(root);
  root->SetBounds(gfx::Rect(window_tree_host_->window()->bounds().size()));
  root_node_manager_->root()->window()->Show();

  window_tree_client_.reset(
      new WindowTreeClientImpl(window_tree_host_->window()));

  focus_client_.reset(new FocusClientImpl);
  aura::client::SetFocusClient(window_tree_host_->window(),
                               focus_client_.get());

  window_tree_host_->Show();

  delegate_->OnRootViewManagerWindowTreeHostCreated();
}

}  // namespace service
}  // namespace mojo
