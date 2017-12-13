// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
#define COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_

#include <memory>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "ash/wm/client_controlled_state.h"
#include "base/callback.h"
#include "base/macros.h"
#include "components/exo/shell_surface_base.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/display/display_observer.h"

namespace ash {
namespace mojom {
enum class WindowPinType;
}
}  // namespace ash

namespace exo {
class Surface;

enum class Orientation { PORTRAIT, LANDSCAPE };

// This class implements a ShellSurface whose window state and bounds are
// controlled by a remote shell client rather than the window manager. The
// position specified as part of the geometry is relative to the origin of
// the screen coordinate system.
class ClientControlledShellSurface
    : public ShellSurfaceBase,
      public display::DisplayObserver,
      public ash::WindowTreeHostManager::Observer,
      public ui::CompositorLockClient {
 public:
  using GeometryChangedCallback =
      base::RepeatingCallback<void(const gfx::Rect& geometry)>;

  ClientControlledShellSurface(Surface* surface,
                               bool can_minimize,
                               int container);
  ~ClientControlledShellSurface() override;

  void set_geometry_changed_callback(const GeometryChangedCallback& callback) {
    geometry_changed_callback_ = callback;
  }

  // Called when the client was maximized.
  void SetMaximized();

  // Called when the client was minimized.
  void SetMinimized();

  // Called when the client was restored.
  void SetRestored();

  // Called when the client chagned the fullscreen state.
  void SetFullscreen(bool fullscreen);

  // Set the callback to run when the surface state changed.
  using StateChangedCallback =
      base::RepeatingCallback<void(ash::mojom::WindowStateType old_state_type,
                                   ash::mojom::WindowStateType new_state_type)>;
  void set_state_changed_callback(
      const StateChangedCallback& state_changed_callback) {
    state_changed_callback_ = state_changed_callback;
  }

  // Pin/unpin the surface. Pinned surface cannot be switched to
  // other windows unless its explicitly unpinned.
  void SetPinned(ash::mojom::WindowPinType type);

  // Sets the surface to be on top of all other windows.
  void SetAlwaysOnTop(bool always_on_top);

  // Controls the visibility of the system UI when this surface is active.
  void SetSystemUiVisibility(bool autohide);

  // Set orientation for surface.
  void SetOrientation(Orientation orientation);

  // Set shadow bounds in surface coordinates. Empty bounds disable the shadow.
  void SetShadowBounds(const gfx::Rect& bounds);

  void SetScale(double scale);

  // Set top inset for surface.
  void SetTopInset(int height);

  // Set resize outset for surface.
  void SetResizeOutset(int outset);

  // Sends the window state change event to client.
  void OnWindowStateChangeEvent(ash::mojom::WindowStateType old_state,
                                ash::mojom::WindowStateType next_state);

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;

  void SaveWindowPlacement(const gfx::Rect& bounds,
                           ui::WindowShowState show_state) override;
  bool GetSavedWindowPlacement(const views::Widget* widget,
                               gfx::Rect* bounds,
                               ui::WindowShowState* show_state) const override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;

  // Overridden from display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  // Overridden from ash::WindowTreeHostManager::Observer:
  void OnDisplayConfigurationChanged() override;

  // Overridden from ui::CompositorLockClient:
  void CompositorLockTimedOut() override;

  // A factory callback to create ClientControlledState::Delegate.
  using DelegateFactoryCallback = base::RepeatingCallback<
      std::unique_ptr<ash::wm::ClientControlledState::Delegate>(void)>;

  // Set the factory callback for unit test.
  static void SetClientControlledStateDelegateFactoryForTest(
      const DelegateFactoryCallback& callback);

 private:
  // Overridden from ShellSurface:
  void SetWidgetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetShadowBounds() const override;
  void InitializeWindowState(ash::wm::WindowState* window_state) override;
  void UpdateBackdrop() override;
  float GetScale() const override;
  bool CanAnimateWindowStateTransitions() const override;
  aura::Window* GetDragWindow() override;
  std::unique_ptr<ash::WindowResizer> CreateWindowResizer(
      aura::Window* window,
      int component) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  gfx::Point GetWidgetOrigin() const override;
  gfx::Point GetSurfaceOrigin() const override;

  // Lock the compositor if it's not already locked, or extends the
  // lock timeout if it's already locked.
  // TODO(reveman): Remove this when using configure callbacks for orientation.
  // crbug.com/765954
  void EnsureCompositorIsLockedForOrientationChange();

  ash::wm::WindowState* GetWindowState();

  GeometryChangedCallback geometry_changed_callback_;
  int64_t primary_display_id_;

  int top_inset_height_ = 0;
  int pending_top_inset_height_ = 0;

  double scale_ = 1.0;
  double pending_scale_ = 1.0;

  StateChangedCallback state_changed_callback_;

  // TODO(reveman): Use configure callbacks for orientation. crbug.com/765954
  Orientation pending_orientation_ = Orientation::LANDSCAPE;
  Orientation orientation_ = Orientation::LANDSCAPE;
  Orientation expected_orientation_ = Orientation::LANDSCAPE;

  ash::wm::ClientControlledState* client_controlled_state_ = nullptr;

  std::unique_ptr<ui::CompositorLock> orientation_compositor_lock_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
