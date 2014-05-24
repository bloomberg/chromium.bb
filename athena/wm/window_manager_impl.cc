// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/public/window_manager.h"

#include "athena/screen/public/screen_manager.h"
#include "base/logging.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"

namespace athena {
namespace {

class WindowManagerImpl : public WindowManager, public aura::WindowObserver {
 public:
  WindowManagerImpl();
  virtual ~WindowManagerImpl();

  void Layout();

 private:
  // aura::WindowObserver
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    if (window == container_)
      container_.reset();
  }

  scoped_ptr<aura::Window> container_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

class WindowManagerImpl* instance = NULL;

class AthenaContainerLayoutManager : public aura::LayoutManager {
 public:
  AthenaContainerLayoutManager() {}
  virtual ~AthenaContainerLayoutManager() {}

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE { instance->Layout(); }
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE {
    instance->Layout();
  }
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE {}
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE {
    instance->Layout();
  }
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE {
    instance->Layout();
  }
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE {
    if (!requested_bounds.IsEmpty())
      SetChildBoundsDirect(child, requested_bounds);
  }

  DISALLOW_COPY_AND_ASSIGN(AthenaContainerLayoutManager);
};

WindowManagerImpl::WindowManagerImpl()
    : container_(
          ScreenManager::Get()->CreateDefaultContainer("MainContainer")) {
  container_->SetLayoutManager(new AthenaContainerLayoutManager);
  container_->AddObserver(this);
  instance = this;
}

WindowManagerImpl::~WindowManagerImpl() {
  if (container_)
    container_->RemoveObserver(this);
  container_.reset();
  instance = NULL;
}

void WindowManagerImpl::Layout() {
  if (!container_)
    return;
  gfx::Rect bounds = gfx::Rect(container_->bounds().size());
  // Just make it small a bit so that the background is visible.
  bounds.Inset(10, 10, 10, 10);
  const aura::Window::Windows& children = container_->children();
  for (aura::Window::Windows::const_iterator iter = children.begin();
       iter != children.end();
       ++iter) {
    (*iter)->SetBounds(bounds);
  }
}

}  // namespace

// static
WindowManager* WindowManager::Create() {
  DCHECK(!instance);
  new WindowManagerImpl;
  DCHECK(instance);
  return instance;
}

// static
void WindowManager::Shutdown() {
  DCHECK(instance);
  delete instance;
  DCHECK(!instance);
}

}  // namespace athena
