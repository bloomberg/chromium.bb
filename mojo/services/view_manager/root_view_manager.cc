// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/services/view_manager/root_view_manager.h"

#include "base/auto_reset.h"
#include "mojo/aura/screen_mojo.h"
#include "mojo/aura/window_tree_host_mojo.h"
#include "mojo/public/cpp/shell/service.h"
#include "mojo/public/interfaces/shell/shell.mojom.h"
#include "mojo/services/view_manager/root_node_manager.h"
#include "ui/aura/client/default_capture_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/window.h"

namespace mojo {
namespace services {
namespace view_manager {

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

RootViewManager::RootViewManager(Shell* shell, RootNodeManager* root_node)
    : shell_(shell),
      root_node_manager_(root_node),
      in_setup_(false) {
  screen_.reset(ScreenMojo::Create());
  gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, screen_.get());
  NativeViewportPtr viewport;
  ConnectTo(shell, "mojo:mojo_native_viewport_service", &viewport);
  window_tree_host_.reset(new WindowTreeHostMojo(
        viewport.Pass(),
        gfx::Rect(800, 600),
        base::Bind(&RootViewManager::OnCompositorCreated,
                   base::Unretained(this))));
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

  window_tree_host_->Show();
}

}  // namespace view_manager
}  // namespace services
}  // namespace mojo
