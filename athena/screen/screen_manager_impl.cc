// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/public/screen_manager.h"

#include "athena/screen/background_controller.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "ui/aura/client/window_tree_client.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"

namespace athena {
namespace {

ScreenManager* instance = NULL;

// TODO(oshima): There seems to be a couple of private implementation which does
// the same.
// Consider consolidating and reuse it.
class FillLayoutManager : public aura::LayoutManager {
 public:
  explicit FillLayoutManager(aura::Window* container) : container_(container) {
    DCHECK(container_);
  }

  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE {
    gfx::Rect full_bounds = gfx::Rect(container_->bounds().size());
    for (aura::Window::Windows::const_iterator iter =
             container_->children().begin();
         iter != container_->children().end();
         ++iter) {
      SetChildBoundsDirect(*iter, full_bounds);
    }
  }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    SetChildBoundsDirect(child, (gfx::Rect(container_->bounds().size())));
  }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {}
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    // Ignore SetBounds request.
  }

 private:
  aura::Window* container_;

  DISALLOW_COPY_AND_ASSIGN(FillLayoutManager);
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

  DISALLOW_COPY_AND_ASSIGN(ScreenManagerImpl);
};

void ScreenManagerImpl::Init() {
  root_window_->SetLayoutManager(new FillLayoutManager(root_window_));
  background_window_ =
      CreateContainerInternal(root_window_, "AthenaBackground");
  background_window_->SetLayoutManager(
      new FillLayoutManager(background_window_));
  background_controller_.reset(new BackgroundController(background_window_));
}

aura::Window* ScreenManagerImpl::CreateDefaultContainer(
    const std::string& name) {
  aura::Window* container = CreateContainerInternal(root_window_, name);
  window_tree_client_.reset(new AthenaWindowTreeClient(container));
  aura::client::SetWindowTreeClient(root_window_, window_tree_client_.get());
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
