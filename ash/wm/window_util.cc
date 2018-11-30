// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_util.h"

#include <memory>

#include "ash/public/cpp/ash_constants.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/root_window_controller.h"
#include "ash/session/session_controller.h"
#include "ash/shelf/shelf.h"
#include "ash/shell.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/widget_finder.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ash/ws/window_service_owner.h"
#include "services/ws/window_service.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/capture_client.h"
#include "ui/aura/client/focus_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_targeter.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/dip_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/easy_resize_window_targeter.h"
#include "ui/wm/core/window_properties.h"
#include "ui/wm/core/window_util.h"
#include "ui/wm/public/activation_client.h"

namespace ash {
namespace wm {

namespace {

// Moves |window| to the given |root| window's corresponding container, if it is
// not already in the same root window. Returns true if |window| was moved.
bool MoveWindowToRoot(aura::Window* window, aura::Window* root) {
  if (!root || root == window->GetRootWindow())
    return false;
  aura::Window* container = RootWindowController::ForWindow(root)->GetContainer(
      window->parent()->id());
  if (!container)
    return false;
  container->AddChild(window);
  return true;
}

// Asks the remote client that owns |window| to close it. Returns true if there
// was a remote client for |window|, false otherwise.
bool AskRemoteClientToCloseWindow(aura::Window* window) {
  ws::WindowService* window_service =
      Shell::Get()->window_service_owner()->window_service();
  return window_service && window_service->RequestClose(window);
}

// This window targeter reserves space for the portion of the resize handles
// that extend within a top level window.
class InteriorResizeHandleTargeter : public aura::WindowTargeter {
 public:
  InteriorResizeHandleTargeter() {
    SetInsets(gfx::Insets(kResizeInsideBoundsSize));
  }

  ~InteriorResizeHandleTargeter() override = default;

  bool GetHitTestRects(aura::Window* target,
                       gfx::Rect* hit_test_rect_mouse,
                       gfx::Rect* hit_test_rect_touch) const override {
    if (target == window() && window()->parent() &&
        window()->parent()->targeter()) {
      // Defer to the parent's targeter on whether |window_| should be able to
      // receive the event. This should be EasyResizeWindowTargeter, which is
      // installed on the container window, and is necessary for
      // kResizeOutsideBoundsSize to work.
      return window()->parent()->targeter()->GetHitTestRects(
          target, hit_test_rect_mouse, hit_test_rect_touch);
    }

    return WindowTargeter::GetHitTestRects(target, hit_test_rect_mouse,
                                           hit_test_rect_touch);
  }

  bool ShouldUseExtendedBounds(const aura::Window* target) const override {
    // Fullscreen/maximized windows can't be drag-resized.
    if (GetWindowState(window())->IsMaximizedOrFullscreenOrPinned() ||
        !wm::GetWindowState(target)->CanResize()) {
      return false;
    }

    // The shrunken hit region only applies to children of |window()|.
    return target->parent() == window();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(InteriorResizeHandleTargeter);
};

}  // namespace

// TODO(beng): replace many of these functions with the corewm versions.
void ActivateWindow(aura::Window* window) {
  ::wm::ActivateWindow(window);
}

void DeactivateWindow(aura::Window* window) {
  ::wm::DeactivateWindow(window);
}

bool IsActiveWindow(aura::Window* window) {
  return ::wm::IsActiveWindow(window);
}

aura::Window* GetActiveWindow() {
  return ::wm::GetActivationClient(Shell::GetPrimaryRootWindow())
      ->GetActiveWindow();
}

aura::Window* GetActivatableWindow(aura::Window* window) {
  return ::wm::GetActivatableWindow(window);
}

bool CanActivateWindow(aura::Window* window) {
  return ::wm::CanActivateWindow(window);
}

aura::Window* GetFocusedWindow() {
  return aura::client::GetFocusClient(Shell::GetPrimaryRootWindow())
      ->GetFocusedWindow();
}

aura::Window* GetCaptureWindow() {
  return aura::client::GetCaptureWindow(Shell::GetPrimaryRootWindow());
}

void GetBlockingContainersForRoot(aura::Window* root_window,
                                  aura::Window** min_container,
                                  aura::Window** system_modal_container) {
  if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    *min_container =
        root_window->GetChildById(kShellWindowId_LockScreenContainersContainer);
    *system_modal_container =
        root_window->GetChildById(kShellWindowId_LockSystemModalContainer);
  } else {
    *min_container = nullptr;
    *system_modal_container =
        root_window->GetChildById(kShellWindowId_SystemModalContainer);
  }
}

bool IsWindowUserPositionable(aura::Window* window) {
  return GetWindowState(window)->IsUserPositionable();
}

void PinWindow(aura::Window* window, bool trusted) {
  wm::WMEvent event(trusted ? wm::WM_EVENT_TRUSTED_PIN : wm::WM_EVENT_PIN);
  wm::GetWindowState(window)->OnWMEvent(&event);
}

void SetAutoHideShelf(aura::Window* window, bool autohide) {
  wm::GetWindowState(window)->set_autohide_shelf_when_maximized_or_fullscreen(
      autohide);
  for (aura::Window* root_window : Shell::GetAllRootWindows())
    Shelf::ForWindow(root_window)->UpdateVisibilityState();
}

bool MoveWindowToDisplay(aura::Window* window, int64_t display_id) {
  DCHECK(window);
  WindowState* window_state = GetWindowState(window);
  if (window_state->allow_set_bounds_direct()) {
    aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
    if (root) {
      gfx::Rect bounds = window->bounds();
      MoveWindowToRoot(window, root);
      // Client controlled won't update the bounds upon the root window
      // Change. Explicitly update the bounds so that the client can
      // make decision.
      window->SetBounds(bounds);
      return true;
    }
    return false;
  }
  aura::Window* root = Shell::GetRootWindowForDisplayId(display_id);
  // Update restore bounds to target root window.
  if (window_state->HasRestoreBounds()) {
    gfx::Rect restore_bounds = window_state->GetRestoreBoundsInParent();
    ::wm::ConvertRectToScreen(root, &restore_bounds);
    window_state->SetRestoreBoundsInScreen(restore_bounds);
  }
  return root && MoveWindowToRoot(window, root);
}

bool MoveWindowToEventRoot(aura::Window* window, const ui::Event& event) {
  DCHECK(window);
  views::View* target = static_cast<views::View*>(event.target());
  if (!target)
    return false;
  aura::Window* root = target->GetWidget()->GetNativeView()->GetRootWindow();
  return root && MoveWindowToRoot(window, root);
}

void SetSnapsChildrenToPhysicalPixelBoundary(aura::Window* container) {
  DCHECK(!container->GetProperty(::wm::kSnapChildrenToPixelBoundary))
      << container->GetName();
  container->SetProperty(::wm::kSnapChildrenToPixelBoundary, true);
}

int GetNonClientComponent(aura::Window* window, const gfx::Point& location) {
  return window->delegate()
             ? window->delegate()->GetNonClientComponent(location)
             : HTNOWHERE;
}

void SetChildrenUseExtendedHitRegionForWindow(aura::Window* window) {
  gfx::Insets mouse_extend(-kResizeOutsideBoundsSize, -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize,
                           -kResizeOutsideBoundsSize);
  gfx::Insets touch_extend =
      mouse_extend.Scale(kResizeOutsideBoundsScaleForTouch);
  // TODO: EasyResizeWindowTargeter makes it so children get events outside
  // their bounds. This only works in mash when mash is providing the non-client
  // frame. Mus needs to support an api for the WindowManager that enables
  // events to be dispatched to windows outside the windows bounds that this
  // function calls into. http://crbug.com/679056.
  window->SetEventTargeter(std::make_unique<::wm::EasyResizeWindowTargeter>(
      mouse_extend, touch_extend));
}

void CloseWidgetForWindow(aura::Window* window) {
  if (AskRemoteClientToCloseWindow(window))
    return;

  views::Widget* widget = GetInternalWidgetForWindow(window);
  DCHECK(widget);
  widget->Close();
}

void InstallResizeHandleWindowTargeterForWindow(aura::Window* window) {
  window->SetEventTargeter(std::make_unique<InteriorResizeHandleTargeter>());
  // For Mash, ServerWindows will override the event targeter with a
  // ServerWindowTargeter, so make sure it knows about the resize insets.
  window->SetProperty(aura::client::kResizeHandleInset,
                      kResizeInsideBoundsSize);
}

bool IsDraggingTabs(const aura::Window* window) {
  return window->GetProperty(ash::kIsDraggingTabsKey);
}

bool ShouldExcludeForBothCycleListAndOverview(const aura::Window* window) {
  // Exclude windows:
  // - non user positionable windows, such as extension popups.
  // - windows being dragged
  // - pip windows
  const wm::WindowState* state = wm::GetWindowState(window);
  if (!state->IsUserPositionable() || state->is_dragged() || state->IsPip())
    return true;

  return window->GetProperty(kHideInOverviewKey);
}

bool ShouldExcludeForCycleList(const aura::Window* window) {
  // Exclude the AppList window, which will hide as soon as cycling starts
  // anyway. It doesn't make sense to count it as a "switchable" window, yet
  // a lot of code relies on the MRU list returning the app window. If we
  // don't manually remove it, the window cycling UI won't crash or misbehave,
  // but there will be a flicker as the target window changes. Also exclude
  // unselectable windows such as extension popups.
  // TODO(sammiequon): Investigate if this is needed.
  for (auto* parent = window->parent(); parent; parent = parent->parent()) {
    if (parent->id() == kShellWindowId_AppListContainer)
      return true;
  }

  return ShouldExcludeForBothCycleListAndOverview(window);
}

bool ShouldExcludeForOverview(const aura::Window* window) {
  // Remove the default snapped window from the window list. The default
  // snapped window occupies one side of the screen, while the other windows
  // occupy the other side of the screen in overview mode. The default snap
  // position is the position where the window was first snapped. See
  // |default_snap_position_| in SplitViewController for more detail.
  if (Shell::Get()->IsSplitViewModeActive() &&
      window ==
          Shell::Get()->split_view_controller()->GetDefaultSnappedWindow()) {
    return true;
  }

  return ShouldExcludeForBothCycleListAndOverview(window);
}

void RemoveTransientDescendants(std::vector<aura::Window*>* out_window_list) {
  for (auto it = out_window_list->begin(); it != out_window_list->end();) {
    aura::Window* transient_root = ::wm::GetTransientRoot(*it);
    if (*it != transient_root &&
        base::ContainsValue(*out_window_list, transient_root)) {
      it = out_window_list->erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace wm
}  // namespace ash
