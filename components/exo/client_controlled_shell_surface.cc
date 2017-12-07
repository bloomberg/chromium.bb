// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_shell_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/trace_event_argument.h"
#include "components/exo/surface.h"
#include "components/exo/wm_helper.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/class_property.h"
#include "ui/compositor/compositor_lock.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
////#include "ui/wm/core/shadow.h"
//#include "ui/wm/core/shadow_controller.h"
//#include "ui/wm/core/shadow_types.h"
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {

// Maximum amount of time to wait for contents that match the display's
// orientation in tablet mode.
// TODO(oshima): Looks like android is generating unnecessary frames.
// Fix it on Android side and reduce the timeout.
constexpr int kOrientationLockTimeoutMs = 2500;

// Minimal WindowResizer that unlike DefaultWindowResizer does not handle
// dragging and resizing windows.
class CustomWindowResizer : public ash::WindowResizer {
 public:
  explicit CustomWindowResizer(ash::wm::WindowState* window_state)
      : WindowResizer(window_state) {}

  // Overridden from ash::WindowResizer:
  void Drag(const gfx::Point& location, int event_flags) override {}
  void CompleteDrag() override {}
  void RevertDrag() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomWindowResizer);
};

Orientation SizeToOrientation(const gfx::Size& size) {
  DCHECK_NE(size.width(), size.height());
  return size.width() > size.height() ? Orientation::LANDSCAPE
                                      : Orientation::PORTRAIT;
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ShellSurface, public:

ClientControlledShellSurface::ClientControlledShellSurface(Surface* surface,
                                                           bool can_minimize,
                                                           int container)
    : ShellSurface(surface,
                   gfx::Point(),
                   true,
                   can_minimize,
                   container),
      primary_display_id_(
          display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
  WMHelper::GetInstance()->AddDisplayConfigurationObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

ClientControlledShellSurface::~ClientControlledShellSurface() {
  WMHelper::GetInstance()->RemoveDisplayConfigurationObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

void ClientControlledShellSurface::SetPinned(ash::mojom::WindowPinType type) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetPinned", "type",
               static_cast<int>(type));

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  // Note: This will ask client to configure its surface even if pinned
  // state doesn't change.
  ScopedConfigure scoped_configure(this, true);
  widget_->GetNativeWindow()->SetProperty(ash::kWindowPinTypeKey, type);
}

void ClientControlledShellSurface::SetSystemUiVisibility(bool autohide) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetSystemUiVisibility",
               "autohide", autohide);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  ash::wm::SetAutoHideShelf(widget_->GetNativeWindow(), autohide);
}

void ClientControlledShellSurface::SetAlwaysOnTop(bool always_on_top) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetAlwaysOnTop",
               "always_on_top", always_on_top);

  if (!widget_)
    CreateShellSurfaceWidget(ui::SHOW_STATE_NORMAL);

  widget_->GetNativeWindow()->SetProperty(aura::client::kAlwaysOnTopKey,
                                          always_on_top);
}

void ClientControlledShellSurface::SetOrientation(Orientation orientation) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetOrientation",
               "orientation",
               orientation == Orientation::PORTRAIT ? "portrait" : "landscape");
  pending_orientation_ = orientation;
}

void ClientControlledShellSurface::SetShadowBounds(const gfx::Rect& bounds) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetShadowBounds", "bounds",
               bounds.ToString());
  auto shadow_bounds =
      bounds.IsEmpty() ? base::nullopt : base::make_optional(bounds);
  if (shadow_bounds_ != shadow_bounds) {
    shadow_bounds_ = shadow_bounds;
    shadow_bounds_changed_ = true;
  }
}

void ClientControlledShellSurface::SetScale(double scale) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetScale", "scale", scale);

  if (scale <= 0.0) {
    DLOG(WARNING) << "Surface scale must be greater than 0";
    return;
  }

  pending_scale_ = scale;
}

void ClientControlledShellSurface::SetTopInset(int height) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetTopInset", "height",
               height);

  pending_top_inset_height_ = height;
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ClientControlledShellSurface::OnSurfaceCommit() {
  ShellSurface::OnSurfaceCommit();
  if (widget_) {
    // Apply new top inset height.
    if (pending_top_inset_height_ != top_inset_height_) {
      widget_->GetNativeWindow()->SetProperty(aura::client::kTopViewInset,
                                              pending_top_inset_height_);
      top_inset_height_ = pending_top_inset_height_;
    }

    // Update surface scale.
    if (pending_scale_ != scale_) {
      gfx::Transform transform;
      DCHECK_NE(pending_scale_, 0.0);
      transform.Scale(1.0 / pending_scale_, 1.0 / pending_scale_);
      host_window()->SetTransform(transform);
      scale_ = pending_scale_;
    }

    orientation_ = pending_orientation_;
    if (expected_orientation_ == orientation_)
      orientation_compositor_lock_.reset();
  } else {
    orientation_compositor_lock_.reset();
  }
}

////////////////////////////////////////////////////////////////////////////////
// aura::WindowObserver overrides:
void ClientControlledShellSurface::OnWindowBoundsChanged(
    aura::Window* window,
    const gfx::Rect& old_bounds,
    const gfx::Rect& new_bounds,
    ui::PropertyChangeReason reason) {}

////////////////////////////////////////////////////////////////////////////////
// views::WidgetDelegate overrides:

bool ClientControlledShellSurface::CanResize() const {
  return false;
}

void ClientControlledShellSurface::SaveWindowPlacement(
    const gfx::Rect& bounds,
    ui::WindowShowState show_state) {}

bool ClientControlledShellSurface::GetSavedWindowPlacement(
    const views::Widget* widget,
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// display::DisplayObserver overrides:

void ClientControlledShellSurface::OnDisplayMetricsChanged(
    const display::Display& new_display,
    uint32_t changed_metrics) {
  if (!widget_ || !widget_->IsActive() ||
      !WMHelper::GetInstance()->IsTabletModeWindowManagerEnabled()) {
    return;
  }

  const display::Screen* screen = display::Screen::GetScreen();
  display::Display current_display =
      screen->GetDisplayNearestWindow(widget_->GetNativeWindow());
  if (current_display.id() != new_display.id() ||
      !(changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)) {
    return;
  }

  Orientation target_orientation = SizeToOrientation(new_display.size());
  if (orientation_ == target_orientation)
    return;
  expected_orientation_ = target_orientation;
  EnsureCompositorIsLockedForOrientationChange();
}

////////////////////////////////////////////////////////////////////////////////
// ash::WindowTreeHostManager::Observer overrides:

void ClientControlledShellSurface::OnDisplayConfigurationChanged() {
  const display::Screen* screen = display::Screen::GetScreen();
  int64_t primary_display_id = screen->GetPrimaryDisplay().id();
  if (primary_display_id == primary_display_id_)
    return;

  display::Display old_primary_display;
  if (screen->GetDisplayWithDisplayId(primary_display_id_,
                                      &old_primary_display)) {
    // Give the client a chance to adjust window positions before switching to
    // the new coordinate system. Retain the old origin by reverting the origin
    // delta until the next configure is acknowledged.
    gfx::Vector2d delta = gfx::Point() - old_primary_display.bounds().origin();
    origin_offset_ -= delta;
    pending_origin_offset_accumulator_ += delta;

    if (widget_) {
      UpdateWidgetBounds();
      UpdateShadow();
    }

    Configure();
  }

  primary_display_id_ = primary_display_id;
}

////////////////////////////////////////////////////////////////////////////////
// ui::CompositorLockClient overrides:

void ClientControlledShellSurface::CompositorLockTimedOut() {
  orientation_compositor_lock_.reset();
}

////////////////////////////////////////////////////////////////////////////////
// ShellSurface overrides:

void ClientControlledShellSurface::SetWidgetBounds(const gfx::Rect& bounds) {
  if (!resizer_) {
    ShellSurface::SetWidgetBounds(bounds);
    return;
  }

  // TODO(domlaskowski): Synchronize window state transitions with the client,
  // and abort client-side dragging on transition to fullscreen.
  // See crbug.com/699746.
  DLOG_IF(ERROR, widget_->GetWindowBoundsInScreen().size() != bounds.size())
      << "Window size changed during client-driven drag";

  // Convert from screen to display coordinates.
  gfx::Point origin = bounds.origin();
  wm::ConvertPointFromScreen(widget_->GetNativeWindow()->parent(), &origin);

  // Move the window relative to the current display.
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(origin, bounds.size()));
  UpdateSurfaceBounds();

  // Render phantom windows when beyond the current display.
  resizer_->Drag(GetMouseLocation(), 0);
}

void ClientControlledShellSurface::InitializeWindowState(
    ash::wm::WindowState* window_state) {
  // Allow the client to request bounds that do not fill the entire work area
  // when maximized, or the entire display when fullscreen.
  window_state->set_allow_set_bounds_direct(true);
  widget_->set_movement_disabled(true);
  window_state->set_ignore_keyboard_bounds_change(true);
}

void ClientControlledShellSurface::UpdateBackdrop() {
  aura::Window* window = widget_->GetNativeWindow();
  bool enable_backdrop = widget_->IsFullscreen() || widget_->IsMaximized();
  if (window->GetProperty(aura::client::kHasBackdrop) != enable_backdrop)
    window->SetProperty(aura::client::kHasBackdrop, enable_backdrop);
}

float ClientControlledShellSurface::GetScale() const {
  return scale_;
}

bool ClientControlledShellSurface::CanAnimateWindowStateTransitions() const {
  // TODO(domlaskowski): The configure callback does not yet support window
  // state changes. See crbug.com/699746.
  return false;
}

aura::Window* ClientControlledShellSurface::GetDragWindow() {
  return root_surface() ? root_surface()->window() : nullptr;
}

std::unique_ptr<ash::WindowResizer>
ClientControlledShellSurface::CreateWindowResizer(aura::Window* window,
                                                  int component) {
  ash::wm::WindowState* window_state =
      ash::wm::GetWindowState(widget_->GetNativeWindow());
  DCHECK(!window_state->drag_details());
  DCHECK(component == HTCAPTION);
  window_state->CreateDragDetails(GetMouseLocation(), component,
                                  wm::WINDOW_MOVE_SOURCE_MOUSE);

  // Chained with a CustomWindowResizer, DragWindowResizer does not handle
  // dragging. It only renders phantom windows and moves the window to the
  // target root window when dragging ends.
  return std::unique_ptr<ash::WindowResizer>(ash::DragWindowResizer::Create(
      new CustomWindowResizer(window_state), window_state));
}

bool ClientControlledShellSurface::OnMouseDragged(const ui::MouseEvent&) {
  // TODO(domlaskowski): When VKEY_ESCAPE is pressed during dragging, the client
  // destroys the window, but should instead revert the drag to be consistent
  // with ShellSurface::OnKeyEvent. See crbug.com/699746.
  return false;
}

gfx::Point ClientControlledShellSurface::GetWidgetOrigin() const {
  return origin_ - GetSurfaceOrigin().OffsetFromOrigin();
}

gfx::Point ClientControlledShellSurface::GetSurfaceOrigin() const {
  DCHECK(resize_component_ == HTCAPTION);
  gfx::Rect visible_bounds = GetVisibleBounds();
  return origin_ + origin_offset_ - visible_bounds.OffsetFromOrigin();
}

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, private:

void ClientControlledShellSurface::
    EnsureCompositorIsLockedForOrientationChange() {
  if (!orientation_compositor_lock_) {
    ui::Compositor* compositor =
        widget_->GetNativeWindow()->layer()->GetCompositor();
    orientation_compositor_lock_ = compositor->GetCompositorLock(
        this, base::TimeDelta::FromMilliseconds(kOrientationLockTimeoutMs));
  }
}

}  // namespace exo
