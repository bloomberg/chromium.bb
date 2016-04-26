// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include <deque>
#include <memory>
#include <string>

#include "ash/wm/common/window_state_observer.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/surface_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
class WindowResizer;
}

namespace base {
namespace trace_event {
class TracedValue;
}
}

namespace exo {
class Surface;

// This class provides functions for treating a surfaces like toplevel,
// fullscreen or popup widgets, move, resize or maximize them, associate
// metadata like title and class, etc.
class ShellSurface : public SurfaceDelegate,
                     public SurfaceObserver,
                     public views::WidgetDelegate,
                     public views::View,
                     public ash::wm::WindowStateObserver,
                     public aura::WindowObserver,
                     public aura::client::ActivationChangeObserver {
 public:
  ShellSurface(Surface* surface,
               ShellSurface* parent,
               const gfx::Rect& initial_bounds,
               bool activatable);
  explicit ShellSurface(Surface* surface);
  ~ShellSurface() override;

  // Set the callback to run when the user wants the shell surface to be closed.
  // The receiver can chose to not close the window on this signal.
  void set_close_callback(const base::Closure& close_callback) {
    close_callback_ = close_callback;
  }

  // Set the callback to run when the surface is destroyed.
  void set_surface_destroyed_callback(
      const base::Closure& surface_destroyed_callback) {
    surface_destroyed_callback_ = surface_destroyed_callback;
  }

  // Set the callback to run when the client is asked to configure the surface.
  // The size is a hint, in the sense that the client is free to ignore it if
  // it doesn't resize, pick a smaller size (to satisfy aspect ratio or resize
  // in steps of NxM pixels).
  using ConfigureCallback =
      base::Callback<uint32_t(const gfx::Size& size,
                              ash::wm::WindowStateType state_type,
                              bool resizing,
                              bool activated)>;
  void set_configure_callback(const ConfigureCallback& configure_callback) {
    configure_callback_ = configure_callback;
  }

  // When the client is asked to configure the surface, it should acknowledge
  // the configure request sometime before the commit. |serial| is the serial
  // from the configure callback.
  void AcknowledgeConfigure(uint32_t serial);

  // Set the "parent" of this surface. This window should be stacked above a
  // parent.
  void SetParent(ShellSurface* parent);

  // Maximizes the shell surface.
  void Maximize();

  // Restore the shell surface.
  void Restore();

  // Set fullscreen state for shell surface.
  void SetFullscreen(bool fullscreen);

  // Set title for surface.
  void SetTitle(const base::string16& title);

  // Sets the application ID for the window. The application ID identifies the
  // general class of applications to which the window belongs.
  static void SetApplicationId(aura::Window* window,
                               std::string* application_id);
  static const std::string GetApplicationId(aura::Window* window);

  // Set application id for surface.
  void SetApplicationId(const std::string& application_id);

  // Start an interactive move of surface.
  void Move();

  // Start an interactive resize of surface. |component| is one of the windows
  // HT constants (see ui/base/hit_test.h) and describes in what direction the
  // surface should be resized.
  void Resize(int component);

  // Signal a request to close the window. It is up to the implementation to
  // actually decide to do so though.
  void Close();

  // Set geometry for surface. The geometry represents the "visible bounds"
  // for the surface from the user's perspective.
  void SetGeometry(const gfx::Rect& geometry);

  // Sets the main surface for the window.
  static void SetMainSurface(aura::Window* window, Surface* surface);
  static Surface* GetMainSurface(const aura::Window* window);

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  bool IsSurfaceSynchronized() const override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from views::WidgetDelegate:
  bool CanMaximize() const override;
  bool CanResize() const override;
  base::string16 GetWindowTitle() const override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // Overridden from views::View:
  gfx::Size GetPreferredSize() const override;

  // Overridden from ash::wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(ash::wm::WindowState* window_state,
                                   ash::wm::WindowStateType old_type) override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from aura::client::ActivationChangeObserver:
  void OnWindowActivated(
      aura::client::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;

 private:
  class ScopedConfigure;

  // Surface state associated with each configure request.
  struct Config {
    uint32_t serial;
    gfx::Vector2d origin_offset;
    int resize_component;
  };

  // Creates the |widget_| for |surface_|. |show_state| is the initial state
  // of the widget (e.g. maximized).
  void CreateShellSurfaceWidget(ui::WindowShowState show_state);

  // Asks the client to configure its surface.
  void Configure();

  // Attempt to start a drag operation. The type of drag operation to start is
  // determined by |component|.
  void AttemptToStartDrag(int component);

  // End current drag operation.
  void EndDrag(bool revert);

  // Returns true if surface is currently being resized.
  bool IsResizing() const;

  // Returns the "visible bounds" for the surface from the user's perspective.
  gfx::Rect GetVisibleBounds() const;

  // Returns the origin for the surface taking visible bounds and current
  // resize direction into account.
  gfx::Point GetSurfaceOrigin() const;

  // Update bounds of widget to match the current surface bounds.
  void UpdateWidgetBounds();

  views::Widget* widget_;
  Surface* surface_;
  aura::Window* parent_;
  const gfx::Rect initial_bounds_;
  const bool activatable_;
  base::string16 title_;
  std::string application_id_;
  gfx::Rect geometry_;
  gfx::Rect pending_geometry_;
  base::Closure close_callback_;
  base::Closure surface_destroyed_callback_;
  ConfigureCallback configure_callback_;
  ScopedConfigure* scoped_configure_;
  bool ignore_window_bounds_changes_;
  gfx::Point origin_;
  gfx::Vector2d pending_origin_offset_;
  gfx::Vector2d pending_origin_config_offset_;
  int resize_component_;  // HT constant (see ui/base/hit_test.h)
  int pending_resize_component_;
  std::deque<Config> pending_configs_;
  std::unique_ptr<ash::WindowResizer> resizer_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
