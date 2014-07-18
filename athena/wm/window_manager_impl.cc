// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/public/window_manager.h"

#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/public/window_manager_observer.h"
#include "athena/wm/window_overview_mode.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/wm/public/window_types.h"

namespace athena {
namespace {

class WindowManagerImpl : public WindowManager,
                          public WindowOverviewModeDelegate,
                          public aura::WindowObserver,
                          public AcceleratorHandler {
 public:
  WindowManagerImpl();
  virtual ~WindowManagerImpl();

  void Layout();

  // WindowManager:
  virtual void ToggleOverview() OVERRIDE {
    if (overview_) {
      overview_.reset();
      FOR_EACH_OBSERVER(WindowManagerObserver, observers_,
                        OnOverviewModeExit());
    } else {
      overview_ = WindowOverviewMode::Create(container_.get(), this);
      FOR_EACH_OBSERVER(WindowManagerObserver, observers_,
                        OnOverviewModeEnter());
    }
  }

 private:
  enum Command {
    COMMAND_TOGGLE_OVERVIEW,
  };

  void InstallAccelerators() {
    const AcceleratorData accelerator_data[] = {
        {TRIGGER_ON_PRESS, ui::VKEY_F6, ui::EF_NONE, COMMAND_TOGGLE_OVERVIEW,
         AF_NONE},
    };
    AcceleratorManager::Get()->RegisterAccelerators(
        accelerator_data, arraysize(accelerator_data), this);
  }

  // WindowManager:
  virtual void AddObserver(WindowManagerObserver* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(WindowManagerObserver* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  // WindowOverviewModeDelegate:
  virtual void OnSelectWindow(aura::Window* window) OVERRIDE {
    CHECK_EQ(container_.get(), window->parent());
    container_->StackChildAtTop(window);
    overview_.reset();
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_,
                      OnOverviewModeExit());
  }

  // aura::WindowObserver
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE {
    if (window == container_)
      container_.reset();
  }

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE { return true; }
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE {
    switch (command_id) {
      case COMMAND_TOGGLE_OVERVIEW:
        ToggleOverview();
        break;
    }
    return true;
  }

  scoped_ptr<aura::Window> container_;
  scoped_ptr<WindowOverviewMode> overview_;
  ObserverList<WindowManagerObserver> observers_;

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

WindowManagerImpl::WindowManagerImpl() {
  ScreenManager::ContainerParams params("DefaultContainer");
  params.can_activate_children = true;
  container_.reset(ScreenManager::Get()->CreateDefaultContainer(params));
  container_->SetLayoutManager(new AthenaContainerLayoutManager);
  container_->AddObserver(this);
  instance = this;
  InstallAccelerators();
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

// static
WindowManager* WindowManager::GetInstance() {
  DCHECK(instance);
  return instance;
}

}  // namespace athena
