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
#include "ui/aura/window_property.h"
#include "ui/aura/window_tree_host.h"
#include "ui/wm/core/base_focus_rules.h"
#include "ui/wm/core/capture_controller.h"

namespace athena {
namespace {

DEFINE_OWNED_WINDOW_PROPERTY_KEY(ScreenManager::ContainerParams,
                                 kContainerParamsKey,
                                 NULL);

ScreenManager* instance = NULL;

class AthenaFocusRules : public wm::BaseFocusRules {
 public:
  AthenaFocusRules() {}
  virtual ~AthenaFocusRules() {}

  // wm::BaseFocusRules:
  virtual bool SupportsChildActivation(aura::Window* window) const OVERRIDE {
    ScreenManager::ContainerParams* params =
        window->GetProperty(kContainerParamsKey);
    return params && params->can_activate_children;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaFocusRules);
};

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

class ScreenManagerImpl : public ScreenManager {
 public:
  explicit ScreenManagerImpl(aura::Window* root_window);
  virtual ~ScreenManagerImpl();

  void Init();

 private:
  // ScreenManager:
  virtual aura::Window* CreateDefaultContainer(
      const ContainerParams& params) OVERRIDE;
  virtual aura::Window* CreateContainer(const ContainerParams& params) OVERRIDE;
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
  // TODO(oshima): Move the background out from ScreenManager.
  root_window_->SetLayoutManager(new FillLayoutManager(root_window_));
  background_window_ = CreateContainer(ContainerParams("AthenaBackground"));

  background_window_->SetLayoutManager(
      new FillLayoutManager(background_window_));
  background_controller_.reset(new BackgroundController(background_window_));

  capture_client_.reset(new ::wm::ScopedCaptureClient(root_window_));
  accelerator_handler_.reset(new ScreenAcceleratorHandler(root_window_));
}

aura::Window* ScreenManagerImpl::CreateDefaultContainer(
    const ContainerParams& params) {
  aura::Window* container = CreateContainer(params);
  window_tree_client_.reset(new AthenaWindowTreeClient(container));
  aura::client::SetWindowTreeClient(root_window_, window_tree_client_.get());

  screen_position_client_.reset(new AthenaScreenPositionClient());
  aura::client::SetScreenPositionClient(root_window_,
                                        screen_position_client_.get());

  return container;
}

aura::Window* ScreenManagerImpl::CreateContainer(
    const ContainerParams& params) {
  aura::Window* container = new aura::Window(NULL);
  container->Init(aura::WINDOW_LAYER_NOT_DRAWN);
  container->SetName(params.name);
  root_window_->AddChild(container);
  container->Show();
  container->SetProperty(kContainerParamsKey, new ContainerParams(params));
  return container;
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

ScreenManager::ContainerParams::ContainerParams(const std::string& n)
    : name(n),
      can_activate_children(false) {
}

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

// static
wm::FocusRules* ScreenManager::CreateFocusRules() {
  return new AthenaFocusRules();
}

}  // namespace athena
