// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
#define COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_

#include <memory>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "base/macros.h"
#include "components/exo/shell_surface.h"
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
    : public ShellSurface,
      public display::DisplayObserver,
      public ash::WindowTreeHostManager::Observer,
      public ui::CompositorLockClient {
 public:
  ClientControlledShellSurface(Surface* surface,
                               bool can_minimize,
                               int container);
  ~ClientControlledShellSurface() override;

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

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
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

 private:
  // Overridden from ShellSurface:
  void SetWidgetBounds(const gfx::Rect& bounds) override;
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

  int64_t primary_display_id_;

  int top_inset_height_ = 0;
  int pending_top_inset_height_ = 0;

  double scale_ = 1.0;
  double pending_scale_ = 1.0;

  // TODO(reveman): Use configure callbacks for orientation. crbug.com/765954
  Orientation pending_orientation_ = Orientation::LANDSCAPE;
  Orientation orientation_ = Orientation::LANDSCAPE;
  Orientation expected_orientation_ = Orientation::LANDSCAPE;

  std::unique_ptr<ui::CompositorLock> orientation_compositor_lock_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_CLIENT_CONTROLLED_SHELL_SURFACE_H_
