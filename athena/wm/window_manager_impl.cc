// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/public/window_manager.h"

#include <algorithm>

#include "athena/common/container_priorities.h"
#include "athena/input/public/accelerator_manager.h"
#include "athena/screen/public/screen_manager.h"
#include "athena/wm/bezel_controller.h"
#include "athena/wm/mru_window_tracker.h"
#include "athena/wm/public/window_manager_observer.h"
#include "athena/wm/split_view_controller.h"
#include "athena/wm/title_drag_controller.h"
#include "athena/wm/window_overview_mode.h"
#include "base/logging.h"
#include "base/observer_list.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_state.h"
#include "ui/wm/public/window_types.h"

namespace athena {
namespace {

class WindowManagerImpl : public WindowManager,
                          public WindowOverviewModeDelegate,
                          public aura::WindowObserver,
                          public AcceleratorHandler,
                          public TitleDragControllerDelegate {
 public:
  WindowManagerImpl();
  virtual ~WindowManagerImpl();

  void Layout();

  // WindowManager:
  virtual void ToggleOverview() OVERRIDE;

  virtual bool IsOverviewModeActive() OVERRIDE;

 private:
  enum Command {
    COMMAND_TOGGLE_OVERVIEW,
  };

  // Sets whether overview mode is active.
  void SetInOverview(bool active);

  void InstallAccelerators();

  // WindowManager:
  virtual void AddObserver(WindowManagerObserver* observer) OVERRIDE;
  virtual void RemoveObserver(WindowManagerObserver* observer) OVERRIDE;

  // WindowOverviewModeDelegate:
  virtual void OnSelectWindow(aura::Window* window) OVERRIDE;

  // aura::WindowObserver
  virtual void OnWindowAdded(aura::Window* new_window) OVERRIDE;
  virtual void OnWindowDestroying(aura::Window* window) OVERRIDE;

  // AcceleratorHandler:
  virtual bool IsCommandEnabled(int command_id) const OVERRIDE;
  virtual bool OnAcceleratorFired(int command_id,
                                  const ui::Accelerator& accelerator) OVERRIDE;

  // TitleDragControllerDelegate:
  virtual aura::Window* GetWindowBehind(aura::Window* window) OVERRIDE;
  virtual void OnTitleDragStarted(aura::Window* window) OVERRIDE;
  virtual void OnTitleDragCompleted(aura::Window* window) OVERRIDE;
  virtual void OnTitleDragCanceled(aura::Window* window) OVERRIDE;

  // Should be declared first so that it is destoyed last.
  ObserverList<WindowManagerObserver> observers_;

  scoped_ptr<aura::Window> container_;
  scoped_ptr<MruWindowTracker> mru_window_tracker_;
  scoped_ptr<WindowOverviewMode> overview_;
  scoped_ptr<BezelController> bezel_controller_;
  scoped_ptr<SplitViewController> split_view_controller_;
  scoped_ptr<wm::WMState> wm_state_;
  scoped_ptr<TitleDragController> title_drag_controller_;

  DISALLOW_COPY_AND_ASSIGN(WindowManagerImpl);
};

class AthenaContainerLayoutManager : public aura::LayoutManager {
 public:
  AthenaContainerLayoutManager();
  virtual ~AthenaContainerLayoutManager();

 private:
  // aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnWindowRemovedFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaContainerLayoutManager);
};

class WindowManagerImpl* instance = NULL;

WindowManagerImpl::WindowManagerImpl() {
  ScreenManager::ContainerParams params("DefaultContainer", CP_DEFAULT);
  params.can_activate_children = true;
  container_.reset(ScreenManager::Get()->CreateDefaultContainer(params));
  container_->SetLayoutManager(new AthenaContainerLayoutManager);
  container_->AddObserver(this);
  mru_window_tracker_.reset(new MruWindowTracker(container_.get()));
  bezel_controller_.reset(new BezelController(container_.get()));
  split_view_controller_.reset(new SplitViewController(
      container_.get(), mru_window_tracker_.get(), this));
  bezel_controller_->set_left_right_delegate(split_view_controller_.get());
  container_->AddPreTargetHandler(bezel_controller_.get());
  title_drag_controller_.reset(new TitleDragController(container_.get(), this));
  wm_state_.reset(new wm::WMState());
  instance = this;
  InstallAccelerators();
}

WindowManagerImpl::~WindowManagerImpl() {
  overview_.reset();
  if (container_) {
    container_->RemoveObserver(this);
    container_->RemovePreTargetHandler(bezel_controller_.get());
  }
  // |title_drag_controller_| needs to be reset before |container_|.
  title_drag_controller_.reset();
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
    if ((*iter)->type() == ui::wm::WINDOW_TYPE_NORMAL)
      (*iter)->SetBounds(bounds);
  }
}

void WindowManagerImpl::ToggleOverview() {
  SetInOverview(overview_.get() == NULL);
}

bool WindowManagerImpl::IsOverviewModeActive() {
  return overview_;
}

void WindowManagerImpl::SetInOverview(bool active) {
  bool in_overview = !!overview_;
  if (active == in_overview)
    return;

  if (active) {
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_,
                      OnOverviewModeEnter());
    // Re-stack all windows in the order defined by mru_window_tracker_.
    aura::Window::Windows window_list = mru_window_tracker_->GetWindowList();
    aura::Window::Windows::iterator it;
    for (it = window_list.begin(); it != window_list.end(); ++it)
      container_->StackChildAtTop(*it);
    overview_ = WindowOverviewMode::Create(container_.get(),
                                           mru_window_tracker_.get(),
                                           this);
  } else {
    overview_.reset();
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_,
                      OnOverviewModeExit());
  }
}

void WindowManagerImpl::InstallAccelerators() {
  const AcceleratorData accelerator_data[] = {
      {TRIGGER_ON_PRESS, ui::VKEY_F6, ui::EF_NONE, COMMAND_TOGGLE_OVERVIEW,
       AF_NONE},
  };
  AcceleratorManager::Get()->RegisterAccelerators(
      accelerator_data, arraysize(accelerator_data), this);
}

void WindowManagerImpl::AddObserver(WindowManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void WindowManagerImpl::RemoveObserver(WindowManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void WindowManagerImpl::OnSelectWindow(aura::Window* window) {
  mru_window_tracker_->MoveToFront(window);
  wm::ActivateWindow(window);
  SetInOverview(false);
}

void WindowManagerImpl::OnWindowAdded(aura::Window* new_window) {
  if (new_window->type() == ui::wm::WINDOW_TYPE_NORMAL)
    SetInOverview(false);
}

void WindowManagerImpl::OnWindowDestroying(aura::Window* window) {
  if (window == container_)
    container_.reset();
}

bool WindowManagerImpl::IsCommandEnabled(int command_id) const {
  return true;
}

bool WindowManagerImpl::OnAcceleratorFired(int command_id,
                                           const ui::Accelerator& accelerator) {
  switch (command_id) {
    case COMMAND_TOGGLE_OVERVIEW:
      ToggleOverview();
      break;
  }
  return true;
}

aura::Window* WindowManagerImpl::GetWindowBehind(aura::Window* window) {
  const aura::Window::Windows& windows = container_->children();
  aura::Window::Windows::const_iterator iter =
      std::find(windows.begin(), windows.end(), window);
  CHECK(iter != windows.end());
  return (iter == windows.begin()) ? NULL : *(iter - 1);
}

void WindowManagerImpl::OnTitleDragStarted(aura::Window* window) {
}

void WindowManagerImpl::OnTitleDragCompleted(aura::Window* window) {
  aura::Window* next_window = GetWindowBehind(window);
  if (next_window)
    OnSelectWindow(next_window);
}

void WindowManagerImpl::OnTitleDragCanceled(aura::Window* window) {
}

AthenaContainerLayoutManager::AthenaContainerLayoutManager() {
}

AthenaContainerLayoutManager::~AthenaContainerLayoutManager() {
}

void AthenaContainerLayoutManager::OnWindowResized() {
  instance->Layout();
}

void AthenaContainerLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  instance->Layout();
}

void AthenaContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void AthenaContainerLayoutManager::OnWindowRemovedFromLayout(
    aura::Window* child) {
  instance->Layout();
}

void AthenaContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
  instance->Layout();
}

void AthenaContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  if (!requested_bounds.IsEmpty())
    SetChildBoundsDirect(child, requested_bounds);
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
