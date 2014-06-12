// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/public/window_manager.h"

#include "athena/screen/public/screen_manager.h"
#include "athena/wm/window_overview_mode.h"
#include "base/logging.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/wm/public/window_types.h"

namespace athena {
namespace {

class WindowManagerImpl : public WindowManager,
                          public WindowOverviewModeDelegate,
                          public aura::WindowObserver {
 public:
  WindowManagerImpl();
  virtual ~WindowManagerImpl();

  void Layout();

  // WindowManager:
  virtual void ToggleOverview() OVERRIDE {
    if (overview_)
      overview_.reset();
    else
      overview_ = WindowOverviewMode::Create(container_.get(), this);
  }

 private:
  // WindowOverviewModeDelegate:
  virtual void OnSelectWindow(aura::Window* window) OVERRIDE {
    CHECK_EQ(container_.get(), window->parent());
    container_->StackChildAtTop(window);
    overview_.reset();
  }

  // aura::WindowObserver
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    if (window == container_)
      container_.reset();
  }

  scoped_ptr<aura::Window> container_;
  scoped_ptr<ui::EventHandler> temp_handler_;
  scoped_ptr<WindowOverviewMode> overview_;

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

class TempEventHandler : public ui::EventHandler {
 public:
  TempEventHandler() {}
  virtual ~TempEventHandler() {}

 private:
  // ui::EventHandler:
  virtual void OnKeyEvent(ui::KeyEvent* event) OVERRIDE {
    if (event->type() == ui::ET_KEY_PRESSED &&
        event->key_code() == ui::VKEY_F6) {
      instance->ToggleOverview();
    }
  }

  DISALLOW_COPY_AND_ASSIGN(TempEventHandler);
};

WindowManagerImpl::WindowManagerImpl()
    : container_(ScreenManager::Get()->CreateDefaultContainer("MainContainer")),
      temp_handler_(new TempEventHandler()) {
  container_->SetLayoutManager(new AthenaContainerLayoutManager);
  container_->AddObserver(this);
  container_->AddPreTargetHandler(temp_handler_.get());
  instance = this;
}

WindowManagerImpl::~WindowManagerImpl() {
  if (container_) {
    container_->RemovePreTargetHandler(temp_handler_.get());
    container_->RemoveObserver(this);
  }
  container_.reset();
  instance = NULL;
}

void WindowManagerImpl::Layout() {
  if (!container_)
    return;
  gfx::Rect bounds = gfx::Rect(container_->bounds().size());
  const aura::Window::Windows& children = container_->children();
  for (aura::Window::Windows::const_iterator iter = children.begin();
       iter != children.end();
       ++iter) {
    if ((*iter)->type() == ui::wm::WINDOW_TYPE_NORMAL ||
        (*iter)->type() == ui::wm::WINDOW_TYPE_POPUP)
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
