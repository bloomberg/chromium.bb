// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"

#include "athena/common/fill_layout_manager.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/background_controller.h"
#include "athena/screen/screen_accelerator_handler.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/capture_controller.h"

namespace athena {
namespace {

ScreenManager* instance = NULL;

class AthenaWindowTreeClient : public aura::client::WindowTreeClient {
 public:
  explicit AthenaWindowTreeClient(aura::Window* container)
      : container_(container) {}

 private:
  virtual ~AthenaWindowTreeClient() {}

  // aura::client::WindowTreeClient:
  virtual aura::Window* GetDefaultParent(aura::Window* context,
                                         aura::Window* window,
                                         const gfx::Rect& bounds) OVERRIDE {
    return container_;
  }

  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(AthenaWindowTreeClient);
};

class AthenaScreenPositionClient : public aura::client::ScreenPositionClient {
 public:
  AthenaScreenPositionClient() {
  }
  virtual ~AthenaScreenPositionClient() {
  }

 private:
  // aura::client::ScreenPositionClient:
  virtual void ConvertPointToScreen(const aura::Window* window,
                                    gfx::Point* point) OVERRIDE {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(window, root, point);
  }

  virtual void ConvertPointFromScreen(const aura::Window* window,
                                      gfx::Point* point) OVERRIDE {
    const aura::Window* root = window->GetRootWindow();
    aura::Window::ConvertPointToTarget(root, window, point);
  }

  virtual void ConvertHostPointToScreen(aura::Window* window,
                                        gfx::Point* point) OVERRIDE {
    // TODO(oshima): Implement this when adding multiple display support.
    NOTREACHED();
  }

  virtual void SetBounds(aura::Window* window,
                         const gfx::Rect& bounds,
                         const gfx::Display& display) OVERRIDE {
    window->SetBounds(bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaScreenPositionClient);
};

aura::Window* CreateContainerInternal(aura::Window* parent,
                                      const std::string& name) {
  aura::Window* container = new aura::Window(NULL);
  container->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  container->SetName(name);
  parent->AddChild(container);
  container->Show();
  return container;
}

class ScreenManagerImpl : public ScreenManager {
 public:
  explicit ScreenManagerImpl(aura::Window* root_window);
  virtual ~ScreenManagerImpl();

  void Init();

 private:
  // ScreenManager:
  virtual aura::Window* CreateDefaultContainer(
      const std::string& name) OVERRIDE;
  virtual aura::Window* CreateContainer(const std::string& name) OVERRIDE;
  virtual aura::Window* GetContext() OVERRIDE { return root_window_; }
  virtual void SetBackgroundImage(const gfx::ImageSkia& image) OVERRIDE;

  aura::Window* root_window_;
  aura::Window* background_window_;

  scoped_ptr<BackgroundController> background_controller_;
  scoped_ptr<aura::client::WindowTreeClient> window_tree_client_;
  scoped_ptr<AcceleratorHandler> accelerator_handler_;
  scoped_ptr< ::wm::ScopedCaptureClient> capture_client_;
  scoped_ptr<aura::client::ScreenPositionClient> screen_position_client_;

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerImpl);
};

void ScreenManagerImpl::Init() {
  root_window_->SetLayoutManager(new FillLayoutManager(root_window_));
  background_window_ =
      CreateContainerInternal(root_window_, "AthenaBackground");
  background_window_->SetLayoutManager(
      new FillLayoutManager(background_window_));
  background_controller_.reset(new BackgroundController(background_window_));

  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window_));
  accelerator_handler_.reset(new ScreenAcceleratorHandler(root_window_));
}

aura::Window* ScreenManagerImpl::CreateDefaultContainer(
    const std::string& name) {
  aura::Window* container = CreateContainerInternal(root_window_, name);
  window_tree_client_.reset(new AthenaWindowTreeClient(container));
  aura::client::SetWindowTreeClient(root_window_, window_tree_client_.get());

  screen_position_client_.reset(new AthenaScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_,
                                        screen_position_client_.get());

  return container;
}

aura::Window* ScreenManagerImpl::CreateContainer(const std::string& name) {
  return CreateContainerInternal(root_window_, name);
}

void ScreenManagerImpl::SetBackgroundImage(const gfx::ImageSkia& image) {
  background_controller_->SetImage(image);
}

ScreenManagerImpl::ScreenManagerImpl(aura::Window* root_window)
    : root_window_(root_window) {
  DCHECK(root_window_);
  DCHECK(!instance);
  instance = this;
}

ScreenManagerImpl::~ScreenManagerImpl() {
  aura::client::SetScreenPositionClient(root_window_, NULL);
  aura::client::SetWindowTreeClient(root_window_, NULL);
  instance = NULL;
}

}  // namespace

// static
ScreenManager* ScreenManager::Create(aura::Window* root_window) {
  (new ScreenManagerImpl(root_window))->Init();
  DCHECK(instance);
  return instance;
}

// static
ScreenManager* ScreenManager::Get() {
  DCHECK(instance);
  return instance;
}

// static
void ScreenManager::Shutdown() {
  DCHECK(instance);
  delete instance;
  DCHECK(!instance);
}

}  // namespace athena
