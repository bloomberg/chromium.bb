// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_SHELL_SURFACE_H_
#define COMPONENTS_EXO_SHELL_SURFACE_H_

#include <cstdint>
#include <memory>
#include <string>

#include "ash/display/window_tree_host_manager.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/string16.h"
#include "components/exo/surface_observer.h"
#include "components/exo/surface_tree_host.h"

#include "ui/aura/window_observer.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/display/display_observer.h"
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

namespace ui {
class CompositorLock;
}  // namespace ui

namespace exo {
class Surface;

// This class provides functions for treating a surfaces like toplevel,
// fullscreen or popup widgets, move, resize or maximize them, associate
// metadata like title and class, etc.
class ShellSurface : public SurfaceTreeHost,
                     public SurfaceObserver,
                     public aura::WindowObserver,
                     public views::WidgetDelegate,
                     public views::View,
                     public ash::wm::WindowStateObserver,
                     public wm::ActivationChangeObserver {
 public:
  enum class BoundsMode { SHELL, CLIENT, FIXED };

  // The |origin| is in screen coordinates. When bounds are controlled by the
  // shell or fixed, it determines the initial position of the shell surface.
  // In that case, the position specified as part of the geometry is relative
  // to the shell surface.
  // TODO(reveman|oshima): Remove bounds_mode.
  ShellSurface(Surface* surface,
               BoundsMode bounds_mode,
               const gfx::Point& origin,
               bool activatable,
               bool can_minimize,
               int container);
  ShellSurface(Surface* surface);
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

  // Set the callback to run when the surface state changed.
  using StateChangedCallback =
      base::Callback<void(ash::mojom::WindowStateType old_state_type,
                          ash::mojom::WindowStateType new_state_type)>;
  void set_state_changed_callback(
      const StateChangedCallback& state_changed_callback) {
    state_changed_callback_ = state_changed_callback;
  }

  // Set the callback to run when the client is asked to configure the surface.
  // The size is a hint, in the sense that the client is free to ignore it if
  // it doesn't resize, pick a smaller size (to satisfy aspect ratio or resize
  // in steps of NxM pixels).
  using ConfigureCallback =
      base::Callback<uint32_t(const gfx::Size& size,
                              ash::mojom::WindowStateType state_type,
                              bool resizing,
                              bool activated,
                              const gfx::Vector2d& origin_offset)>;
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

  // Activates the shell surface.
  void Activate();

  // Maximizes the shell surface.
  void Maximize();

  // Minimize the shell surface.
  void Minimize();

  // Restore the shell surface.
  void Restore();

  // Set fullscreen state for shell surface.
  void SetFullscreen(bool fullscreen);

  // Start an interactive move of surface.
  virtual void Move();

  // Start an interactive resize of surface. |component| is one of the windows
  // HT constants (see ui/base/hit_test.h) and describes in what direction the
  // surface should be resized.
  virtual void Resize(int component);

  // Set title for the surface.
  void SetTitle(const base::string16& title);

  // Set icon for the surface.
  void SetIcon(const gfx::ImageSkia& icon);

  // Sets the system modality.
  void SetSystemModal(bool system_modal);

  // Sets the application ID for the window. The application ID identifies the
  // general class of applications to which the window belongs.
  static void SetApplicationId(aura::Window* window, const std::string& id);
  static const std::string* GetApplicationId(aura::Window* window);

  // Set the application ID for the surface.
  void SetApplicationId(const std::string& application_id);

  // Signal a request to close the window. It is up to the implementation to
  // actually decide to do so though.
  void Close();

  // Set geometry for surface. The geometry represents the "visible bounds"
  // for the surface from the user's perspective.
  void SetGeometry(const gfx::Rect& geometry);

  // Set origin in screen coordinate space.
  void SetOrigin(const gfx::Point& origin);

  // Set activatable state for surface.
  void SetActivatable(bool activatable);

  // Set container for surface.
  void SetContainer(int container);

  // Set the maximum size for the surface.
  void SetMaximumSize(const gfx::Size& size);

  // Set the miniumum size for the surface.
  void SetMinimumSize(const gfx::Size& size);

  void SetCanMinimize(bool can_minimize);

  // Sets the main surface for the window.
  static void SetMainSurface(aura::Window* window, Surface* surface);

  // Returns the main Surface instance or nullptr if it is not set.
  // |window| must not be nullptr.
  static Surface* GetMainSurface(const aura::Window* window);

  // Returns a trace value representing the state of the surface.
  std::unique_ptr<base::trace_event::TracedValue> AsTracedValue() const;

  // Overridden from SurfaceDelegate:
  void OnSurfaceCommit() override;
  void OnSetFrame(SurfaceFrameType type) override;

  // Overridden from SurfaceObserver:
  void OnSurfaceDestroying(Surface* surface) override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;
  base::string16 GetWindowTitle() const override;
  gfx::ImageSkia GetWindowIcon() override;
  void WindowClosing() override;
  views::Widget* GetWidget() override;
  const views::Widget* GetWidget() const override;
  views::View* GetContentsView() override;
  views::NonClientFrameView* CreateNonClientFrameView(
      views::Widget* widget) override;
  bool WidgetHasHitTestMask() const override;
  void GetWidgetHitTestMask(gfx::Path* mask) const override;

  // Overridden from views::View:
  gfx::Size CalculatePreferredSize() const override;
  gfx::Size GetMinimumSize() const override;
  gfx::Size GetMaximumSize() const override;

  // Overridden from ash::wm::WindowStateObserver:
  void OnPreWindowStateTypeChange(
      ash::wm::WindowState* window_state,
      ash::mojom::WindowStateType old_type) override;
  void OnPostWindowStateTypeChange(
      ash::wm::WindowState* window_state,
      ash::mojom::WindowStateType old_type) override;

  // Overridden from aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroying(aura::Window* window) override;

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // Overridden from ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Overridden from ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;

  Surface* surface_for_testing() { return root_surface(); }

 protected:
  // Helper class used to coalesce a number of changes into one "configure"
  // callback. Callbacks are suppressed while an instance of this class is
  // instantiated and instead called when the instance is destroyed.
  // If |force_configure_| is true ShellSurface::Configure() will be called
  // even if no changes to shell surface took place during the lifetime of the
  // ScopedConfigure instance.
  class ScopedConfigure {
   public:
    ScopedConfigure(ShellSurface* shell_surface, bool force_configure);
    ~ScopedConfigure();

    void set_needs_configure() { needs_configure_ = true; }

   private:
    ShellSurface* const shell_surface_;
    const bool force_configure_;
    bool needs_configure_ = false;

    DISALLOW_COPY_AND_ASSIGN(ScopedConfigure);
  };

  // Creates the |widget_| for |surface_|. |show_state| is the initial state
  // of the widget (e.g. maximized).
  void CreateShellSurfaceWidget(ui::WindowShowState show_state);

  // Asks the client to configure its surface.
  void Configure();

  // Returns the window that has capture during dragging.
  aura::Window* GetDragWindow();

  // Returns true if surface is currently being resized.
  bool IsResizing() const;

  // Updates the bounds of widget to match the current surface bounds.
  void UpdateWidgetBounds();

  // Updates the bounds of surface to match the current widget bounds.
  void UpdateSurfaceBounds();

  // Creates, deletes and update the shadow bounds based on
  // |shadow_bounds_|.
  void UpdateShadow();

  // Applies |system_modal_| to |widget_|.
  void UpdateSystemModal();

  // In the coordinate system of the parent root window.
  gfx::Point GetMouseLocation() const;

  virtual void InitializeWindowState(ash::wm::WindowState* window_state);

  // Attempt to start a drag operation. The type of drag operation to start is
  // determined by |component|.
  virtual void AttemptToStartDrag(int component);

  virtual float GetScale() const;

  // Updates the backdrop state of the shell surface based on the
  // bounds mode and window state.
  virtual void UpdateBackdrop();

  views::Widget* widget_ = nullptr;
  aura::Window* parent_ = nullptr;
  // TODO(oshima): Move to xdg private or internal.n
  BoundsMode bounds_mode_ = BoundsMode::SHELL;
  gfx::Point origin_;

  // Container Window Id (see ash/public/cpp/shell_window_ids.h)
  int container_;
  bool ignore_window_bounds_changes_ = false;
  gfx::Vector2d origin_offset_;
  gfx::Vector2d pending_origin_offset_;
  gfx::Vector2d pending_origin_offset_accumulator_;
  int resize_component_ = HTCAPTION;  // HT constant (see ui/base/hit_test.h)
  int pending_resize_component_ = HTCAPTION;
  base::Optional<gfx::Rect> shadow_bounds_;
  bool shadow_bounds_changed_ = false;
  std::unique_ptr<ash::WindowResizer> resizer_;

 private:
  struct Config;

  class ScopedAnimationsDisabled;

  // End current drag operation.
  void EndDrag(bool revert);

  // Returns the "visible bounds" for the surface from the user's perspective.
  gfx::Rect GetVisibleBounds() const;

  // Returns the origin for the surface taking visible bounds and current
  // resize direction into account.
  gfx::Point GetSurfaceOrigin() const;

  bool activatable_ = true;
  bool can_minimize_ = true;
  bool frame_enabled_ = false;
  bool pending_show_widget_ = false;
  base::string16 title_;
  std::string application_id_;
  gfx::Rect geometry_;
  gfx::Rect pending_geometry_;
  base::Closure close_callback_;
  base::Closure surface_destroyed_callback_;
  StateChangedCallback state_changed_callback_;
  ConfigureCallback configure_callback_;
  ScopedConfigure* scoped_configure_ = nullptr;
  std::unique_ptr<ui::CompositorLock> configure_compositor_lock_;
  base::circular_deque<std::unique_ptr<Config>> pending_configs_;
  std::unique_ptr<ScopedAnimationsDisabled> scoped_animations_disabled_;
  bool system_modal_ = false;
  bool non_system_modal_window_was_active_ = false;
  gfx::ImageSkia icon_;
  gfx::Size minimum_size_;
  gfx::Size pending_minimum_size_;
  gfx::Size maximum_size_;
  gfx::Size pending_maximum_size_;

  DISALLOW_COPY_AND_ASSIGN(ShellSurface);
};

}  // namespace exo

#endif  // COMPONENTS_EXO_SHELL_SURFACE_H_
