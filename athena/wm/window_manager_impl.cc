// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/wm/window_manager_impl.h"

#include <algorithm>

#include "athena/screen/public/screen_manager.h"
#include "athena/util/container_priorities.h"
#include "athena/wm/bezel_controller.h"
#include "athena/wm/public/window_manager_observer.h"
#include "athena/wm/split_view_controller.h"
#include "athena/wm/title_drag_controller.h"
#include "athena/wm/window_list_provider_impl.h"
#include "athena/wm/window_overview_mode.h"
#include "base/bind.h"
#include "base/logging.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/compositor/closure_animation_observer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/display.h"
#include "ui/gfx/screen.h"
#include "ui/wm/core/shadow_controller.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/core/wm_state.h"
#include "ui/wm/public/activation_client.h"
#include "ui/wm/public/window_types.h"

namespace athena {
namespace {
class WindowManagerImpl* instance = NULL;

void SetWindowState(aura::Window* window,
                    const gfx::Rect& bounds,
                    const gfx::Transform& transform) {
  window->SetBounds(bounds);
  window->SetTransform(transform);
}

}  // namespace

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

AthenaContainerLayoutManager::AthenaContainerLayoutManager() {
}

AthenaContainerLayoutManager::~AthenaContainerLayoutManager() {
}

void AthenaContainerLayoutManager::OnWindowResized() {
  // Resize all the existing windows.
  const aura::Window::Windows& list =
      instance->window_list_provider_->GetWindowList();
  const gfx::Size work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area().size();
  bool is_splitview = instance->split_view_controller_->IsSplitViewModeActive();
  gfx::Size split_size;
  if (is_splitview) {
    CHECK(instance->split_view_controller_->left_window());
    split_size =
        instance->split_view_controller_->left_window()->bounds().size();
  }

  for (aura::Window::Windows::const_iterator iter = list.begin();
       iter != list.end();
       ++iter) {
    aura::Window* window = *iter;
    if (is_splitview) {
      if (window == instance->split_view_controller_->left_window())
        window->SetBounds(gfx::Rect(split_size));
      else if (window == instance->split_view_controller_->right_window())
        window->SetBounds(
            gfx::Rect(gfx::Point(split_size.width(), 0), split_size));
      else
        window->SetBounds(gfx::Rect(work_area));
    } else {
      window->SetBounds(gfx::Rect(work_area));
    }
  }
}

void AthenaContainerLayoutManager::OnWindowAddedToLayout(aura::Window* child) {
  if (!instance->window_list_provider_->IsWindowInList(child))
    return;

  if (instance->split_view_controller_->IsSplitViewModeActive() &&
      !instance->IsOverviewModeActive()) {
    instance->split_view_controller_->ReplaceWindow(
        instance->split_view_controller_->left_window(), child);
  } else {
    gfx::Size size =
        gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area().size();
    child->SetBounds(gfx::Rect(size));
  }

  if (instance->IsOverviewModeActive()) {
    // TODO(pkotwicz|oshima). Creating a new window should only exit overview
    // mode if the new window is activated. crbug.com/415266
    instance->OnSelectWindow(child);
  }
}

void AthenaContainerLayoutManager::OnWillRemoveWindowFromLayout(
    aura::Window* child) {
}

void AthenaContainerLayoutManager::OnWindowRemovedFromLayout(
    aura::Window* child) {
}

void AthenaContainerLayoutManager::OnChildWindowVisibilityChanged(
    aura::Window* child,
    bool visible) {
}

void AthenaContainerLayoutManager::SetChildBounds(
    aura::Window* child,
    const gfx::Rect& requested_bounds) {
  if (!requested_bounds.IsEmpty())
    SetChildBoundsDirect(child, requested_bounds);
}

WindowManagerImpl::WindowManagerImpl() {
  ScreenManager::ContainerParams params("DefaultContainer", CP_DEFAULT);
  params.can_activate_children = true;
  container_.reset(ScreenManager::Get()->CreateDefaultContainer(params));
  container_->SetLayoutManager(new AthenaContainerLayoutManager);
  container_->AddObserver(this);
  window_list_provider_.reset(new WindowListProviderImpl(container_.get()));
  bezel_controller_.reset(new BezelController(container_.get()));
  split_view_controller_.reset(
      new SplitViewController(container_.get(), window_list_provider_.get()));
  AddObserver(split_view_controller_.get());
  bezel_controller_->set_left_right_delegate(split_view_controller_.get());
  container_->AddPreTargetHandler(bezel_controller_.get());
  title_drag_controller_.reset(new TitleDragController(container_.get(), this));
  wm_state_.reset(new wm::WMState());
  aura::client::ActivationClient* activation_client =
      aura::client::GetActivationClient(container_->GetRootWindow());
  shadow_controller_.reset(new wm::ShadowController(activation_client));
  instance = this;
  InstallAccelerators();
}

WindowManagerImpl::~WindowManagerImpl() {
  overview_.reset();
  RemoveObserver(split_view_controller_.get());
  split_view_controller_.reset();
  window_list_provider_.reset();
  if (container_) {
    container_->RemoveObserver(this);
    container_->RemovePreTargetHandler(bezel_controller_.get());
  }
  // |title_drag_controller_| needs to be reset before |container_|.
  title_drag_controller_.reset();
  container_.reset();
  instance = NULL;
}

void WindowManagerImpl::ToggleSplitView() {
  if (IsOverviewModeActive())
    return;

  if (split_view_controller_->IsSplitViewModeActive()) {
    split_view_controller_->DeactivateSplitMode();
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_, OnSplitViewModeExit());
    // Relayout so that windows are maximzied.
    container_->layout_manager()->OnWindowResized();
  } else if (split_view_controller_->CanActivateSplitViewMode()) {
    FOR_EACH_OBSERVER(WindowManagerObserver,
                      observers_,
                      OnSplitViewModeEnter());
    split_view_controller_->ActivateSplitMode(NULL, NULL, NULL);
  }
}

void WindowManagerImpl::ToggleOverview() {
  if (IsOverviewModeActive()) {
    SetInOverview(false);

    // Activate the window which was active prior to entering overview.
    const aura::Window::Windows windows =
        window_list_provider_->GetWindowList();
    if (!windows.empty()) {
      aura::Window* window = windows.back();
      // Show the window in case the exit overview animation has finished and
      // |window| was hidden.
      window->Show();

      wm::ActivateWindow(window);
    }
  } else {
    SetInOverview(true);
  }
}

bool WindowManagerImpl::IsOverviewModeActive() {
  return overview_;
}

void WindowManagerImpl::SetInOverview(bool active) {
  bool in_overview = !!overview_;
  if (active == in_overview)
    return;

  bezel_controller_->set_left_right_delegate(
      active ? NULL : split_view_controller_.get());
  if (active) {
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_, OnOverviewModeEnter());

    // Note: The window_list_provider_ resembles the exact window list of the
    // container, so no re-stacking is required before showing the OverviewMode.
    overview_ = WindowOverviewMode::Create(
        container_.get(), window_list_provider_.get(),
        split_view_controller_.get(), this);
  } else {
    overview_.reset();
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_, OnOverviewModeExit());
  }
}

void WindowManagerImpl::InstallAccelerators() {
  const AcceleratorData accelerator_data[] = {
      {TRIGGER_ON_PRESS, ui::VKEY_F6, ui::EF_NONE, CMD_TOGGLE_OVERVIEW,
       AF_NONE},
      {TRIGGER_ON_PRESS, ui::VKEY_F6, ui::EF_CONTROL_DOWN,
       CMD_TOGGLE_SPLIT_VIEW, AF_NONE},
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

void WindowManagerImpl::ToggleSplitViewForTest() {
  ToggleSplitView();
}

WindowListProvider* WindowManagerImpl::GetWindowListProvider() {
  return window_list_provider_.get();
}

void WindowManagerImpl::OnSelectWindow(aura::Window* window) {
  SetInOverview(false);

  // Show the window in case the exit overview animation has finished and
  // |window| was hidden.
  window->Show();

  wm::ActivateWindow(window);

  if (split_view_controller_->IsSplitViewModeActive()) {
    split_view_controller_->DeactivateSplitMode();
    FOR_EACH_OBSERVER(WindowManagerObserver, observers_, OnSplitViewModeExit());
  }
  // If |window| does not have the size of the work-area, then make sure it is
  // resized.
  const gfx::Size work_area =
      gfx::Screen::GetNativeScreen()->GetPrimaryDisplay().work_area().size();
  if (window->GetTargetBounds().size() != work_area) {
    const gfx::Rect& window_bounds = window->bounds();
    const gfx::Rect desired_bounds(work_area);
    gfx::Transform transform;
    transform.Translate(desired_bounds.x() - window_bounds.x(),
                        desired_bounds.y() - window_bounds.y());
    transform.Scale(desired_bounds.width() / window_bounds.width(),
                    desired_bounds.height() / window_bounds.height());
    ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
    settings.SetPreemptionStrategy(
        ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET);
    settings.AddObserver(
        new ui::ClosureAnimationObserver(base::Bind(&SetWindowState,
                                                    base::Unretained(window),
                                                    desired_bounds,
                                                    gfx::Transform())));
    window->SetTransform(transform);
  }
}

void WindowManagerImpl::OnSelectSplitViewWindow(aura::Window* left,
                                                aura::Window* right,
                                                aura::Window* to_activate) {
  SetInOverview(false);
  FOR_EACH_OBSERVER(WindowManagerObserver, observers_, OnSplitViewModeEnter());
  split_view_controller_->ActivateSplitMode(left, right, to_activate);
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
    case CMD_TOGGLE_OVERVIEW:
      ToggleOverview();
      break;
    case CMD_TOGGLE_SPLIT_VIEW:
      ToggleSplitView();
      break;
  }
  return true;
}

aura::Window* WindowManagerImpl::GetWindowBehind(aura::Window* window) {
  const aura::Window::Windows& windows = window_list_provider_->GetWindowList();
  aura::Window::Windows::const_reverse_iterator iter =
      std::find(windows.rbegin(), windows.rend(), window);
  CHECK(iter != windows.rend());
  ++iter;
  aura::Window* behind = NULL;
  if (iter != windows.rend())
    behind = *iter++;

  if (split_view_controller_->IsSplitViewModeActive()) {
    aura::Window* left = split_view_controller_->left_window();
    aura::Window* right = split_view_controller_->right_window();
    CHECK(window == left || window == right);
    if (behind == left || behind == right)
      behind = (iter == windows.rend()) ? NULL : *iter;
  }

  return behind;
}

void WindowManagerImpl::OnTitleDragStarted(aura::Window* window) {
  aura::Window* next_window = GetWindowBehind(window);
  if (!next_window)
    return;
  // Make sure |window| is active.
  wm::ActivateWindow(window);

  // Make sure |next_window| is visibile.
  next_window->Show();

  // Position |next_window| correctly (left aligned if it's larger than
  // |window|, and center aligned otherwise).
  int dx = window->bounds().x() - next_window->bounds().x();
  if (next_window->bounds().width() < window->bounds().width())
    dx -= (next_window->bounds().width() - window->bounds().width()) / 2;

  if (dx) {
    gfx::Transform transform;
    transform.Translate(dx, 0);
    next_window->SetTransform(transform);
  }
}

void WindowManagerImpl::OnTitleDragCompleted(aura::Window* window) {
  aura::Window* next_window = GetWindowBehind(window);
  if (!next_window)
    return;
  if (split_view_controller_->IsSplitViewModeActive()) {
    split_view_controller_->ReplaceWindow(window, next_window);
  } else {
    ui::ScopedLayerAnimationSettings
        settings(next_window->layer()->GetAnimator());
    settings.AddObserver(new ui::ClosureAnimationObserver(
        base::Bind(&SetWindowState,
                   base::Unretained(next_window),
                   window->bounds(),
                   gfx::Transform())));

    gfx::Transform transform;
    transform.Scale(window->bounds().width() / next_window->bounds().width(),
                    window->bounds().height() / next_window->bounds().height());
    transform.Translate(window->bounds().x() - next_window->bounds().x(), 0);
    next_window->SetTransform(transform);

    wm::ActivateWindow(next_window);
  }
  window->Hide();
}

void WindowManagerImpl::OnTitleDragCanceled(aura::Window* window) {
  aura::Window* next_window = GetWindowBehind(window);
  if (!next_window)
    return;
  next_window->SetTransform(gfx::Transform());
  next_window->Hide();
}

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
WindowManager* WindowManager::Get() {
  DCHECK(instance);
  return instance;
}

}  // namespace athena
