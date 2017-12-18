// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/client_controlled_shell_surface.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/cpp/window_state_type.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/wm/client_controlled_state.h"
#include "ash/wm/drag_window_resizer.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
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
#include "ui/wm/core/window_util.h"

namespace exo {
namespace {
ClientControlledShellSurface::DelegateFactoryCallback g_factory_callback;

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

// A ClientControlledStateDelegate that sends the state/bounds
// change request to exo client.
class ClientControlledStateDelegate
    : public ash::wm::ClientControlledState::Delegate {
 public:
  explicit ClientControlledStateDelegate(
      ClientControlledShellSurface* shell_surface)
      : shell_surface_(shell_surface) {}
  ~ClientControlledStateDelegate() override {}

  // Overridden from ash::wm::ClientControlledState::Delegate:
  void HandleWindowStateRequest(
      ash::wm::WindowState* window_state,
      ash::mojom::WindowStateType next_state) override {
    shell_surface_->OnWindowStateChangeEvent(window_state->GetStateType(),
                                             next_state);
  }
  void HandleBoundsRequest(ash::wm::WindowState* window_state,
                           const gfx::Rect& bounds) override {
    // TODO(oshima): Implement this.
  }

 private:
  ClientControlledShellSurface* shell_surface_;
  DISALLOW_COPY_AND_ASSIGN(ClientControlledStateDelegate);
};

// A WindowStateDelegate that implements ToggleFullscreen behavior for
// client controlled window.
class ClientControlledWindowStateDelegate
    : public ash::wm::WindowStateDelegate {
 public:
  explicit ClientControlledWindowStateDelegate(
      ash::wm::ClientControlledState::Delegate* delegate)
      : delegate_(delegate) {}
  ~ClientControlledWindowStateDelegate() override {}

  // Overridden from ash::wm::WindowStateDelegate:
  bool ToggleFullscreen(ash::wm::WindowState* window_state) override {
    ash::mojom::WindowStateType next_state;
    // ash::mojom::WindowStateType current_state = window_state->GetStateType();
    aura::Window* window = window_state->window();
    switch (window_state->GetStateType()) {
      case ash::mojom::WindowStateType::DEFAULT:
      // current_state = ash::mojom::WindowStateType::NORMAL;
      case ash::mojom::WindowStateType::NORMAL:
        window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                            ui::SHOW_STATE_NORMAL);
        next_state = ash::mojom::WindowStateType::FULLSCREEN;
        break;
      case ash::mojom::WindowStateType::MAXIMIZED:
        window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                            ui::SHOW_STATE_MAXIMIZED);
        next_state = ash::mojom::WindowStateType::FULLSCREEN;
        break;
      case ash::mojom::WindowStateType::FULLSCREEN:
        switch (window->GetProperty(aura::client::kPreFullscreenShowStateKey)) {
          case ui::SHOW_STATE_DEFAULT:
          case ui::SHOW_STATE_NORMAL:
            next_state = ash::mojom::WindowStateType::NORMAL;
            break;
          case ui::SHOW_STATE_MAXIMIZED:
            next_state = ash::mojom::WindowStateType::MAXIMIZED;
            break;
          case ui::SHOW_STATE_MINIMIZED:
            next_state = ash::mojom::WindowStateType::MINIMIZED;
            break;
          case ui::SHOW_STATE_FULLSCREEN:
          case ui::SHOW_STATE_INACTIVE:
          case ui::SHOW_STATE_END:
            NOTREACHED() << " unknown state :"
                         << window->GetProperty(
                                aura::client::kPreFullscreenShowStateKey);
            return false;
        }
        break;
      case ash::mojom::WindowStateType::MINIMIZED: {
        ui::WindowShowState pre_full_state =
            window->GetProperty(aura::client::kPreMinimizedShowStateKey);
        if (pre_full_state != ui::SHOW_STATE_FULLSCREEN) {
          window->SetProperty(aura::client::kPreFullscreenShowStateKey,
                              pre_full_state);
        }
        next_state = ash::mojom::WindowStateType::FULLSCREEN;
        break;
      }
      default:
        // TODO(oshima|xdai): Handle SNAP state.
        return false;
    }
    delegate_->HandleWindowStateRequest(window_state, next_state);
    return true;
  }

 private:
  ash::wm::ClientControlledState::Delegate* delegate_;

  DISALLOW_COPY_AND_ASSIGN(ClientControlledWindowStateDelegate);
};

bool IsPinned(const ash::wm::WindowState* window_state) {
  return window_state->IsPinned() || window_state->IsTrustedPinned();
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, public:

ClientControlledShellSurface::ClientControlledShellSurface(Surface* surface,
                                                           bool can_minimize,
                                                           int container)
    : ShellSurfaceBase(surface, gfx::Point(), true, can_minimize, container),
      primary_display_id_(
          display::Screen::GetScreen()->GetPrimaryDisplay().id()) {
  WMHelper::GetInstance()->AddDisplayConfigurationObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
}

ClientControlledShellSurface::~ClientControlledShellSurface() {
  WMHelper::GetInstance()->RemoveDisplayConfigurationObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
}

void ClientControlledShellSurface::SetMaximized() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetMaximized");
  pending_show_state_ = ui::SHOW_STATE_MAXIMIZED;
}

void ClientControlledShellSurface::SetMinimized() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetMinimized");
  pending_show_state_ = ui::SHOW_STATE_MINIMIZED;
}

void ClientControlledShellSurface::SetRestored() {
  TRACE_EVENT0("exo", "ClientControlledShellSurface::SetRestored");
  pending_show_state_ = ui::SHOW_STATE_NORMAL;
}

void ClientControlledShellSurface::SetFullscreen(bool fullscreen) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetFullscreen",
               "fullscreen", fullscreen);
  pending_show_state_ =
      fullscreen ? ui::SHOW_STATE_FULLSCREEN : ui::SHOW_STATE_NORMAL;
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

void ClientControlledShellSurface::SetResizeOutset(int outset) {
  TRACE_EVENT1("exo", "ClientControlledShellSurface::SetResizeOutset", "outset",
               outset);

  if (root_surface())
    root_surface()->SetInputOutset(outset);
}

void ClientControlledShellSurface::OnWindowStateChangeEvent(
    ash::mojom::WindowStateType current_state,
    ash::mojom::WindowStateType next_state) {
  if (!state_changed_callback_.is_null())
    state_changed_callback_.Run(current_state, next_state);
}

////////////////////////////////////////////////////////////////////////////////
// SurfaceDelegate overrides:

void ClientControlledShellSurface::OnSurfaceCommit() {
  if (!widget_)
    CreateShellSurfaceWidget(pending_show_state_);

  if (widget_->GetNativeWindow()->GetProperty(aura::client::kShowStateKey) !=
      pending_show_state_) {
    ash::wm::WindowState* window_state = GetWindowState();
    if (!IsPinned(window_state)) {
      ash::mojom::WindowStateType next_window_state =
          ash::mojom::WindowStateType::NORMAL;
      ash::wm::ClientControlledState::BoundsChangeAnimationType animation_type =
          ash::wm::ClientControlledState::kAnimationNone;
      switch (pending_show_state_) {
        case ui::SHOW_STATE_NORMAL:
          if (widget_->IsMaximized() || widget_->IsFullscreen())
            animation_type =
                ash::wm::ClientControlledState::kAnimationCrossFade;
          break;
        case ui::SHOW_STATE_MINIMIZED:
          next_window_state = ash::mojom::WindowStateType::MINIMIZED;
          break;
        case ui::SHOW_STATE_MAXIMIZED:
          animation_type = ash::wm::ClientControlledState::kAnimationCrossFade;
          next_window_state = ash::mojom::WindowStateType::MAXIMIZED;
          break;
        case ui::SHOW_STATE_FULLSCREEN:
          animation_type = ash::wm::ClientControlledState::kAnimationCrossFade;
          next_window_state = ash::mojom::WindowStateType::FULLSCREEN;
          break;
        default:
          break;
      }
      client_controlled_state_->EnterNextState(window_state, next_window_state,
                                               animation_type);
    } else {
      VLOG(1) << "State change was requested while it is pinned";
    }
  }

  ShellSurfaceBase::OnSurfaceCommit();

  UpdateBackdrop();

  if (!geometry_changed_callback_.is_null())
    geometry_changed_callback_.Run(GetVisibleBounds());

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
}

bool ClientControlledShellSurface::IsTouchEnabled(Surface* surface) const {
  // The target for input events is selected by recursively hit-testing surfaces
  // in the surface tree. During client-driven dragging/resizing, capture is set
  // on the root surface. When capture is reset to a different target, mouse
  // events are redirected from the old to the new target, but touch/gesture
  // events are cancelled. To avoid prematurely ending the drag/resize, ensure
  // that the target and capture windows are the same by disabling touch input
  // for all but the root surface.
  return surface == root_surface();
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

views::NonClientFrameView*
ClientControlledShellSurface::CreateNonClientFrameView(views::Widget* widget) {
  ash::wm::WindowState* window_state = GetWindowState();
  std::unique_ptr<ash::wm::ClientControlledState::Delegate> delegate =
      g_factory_callback.is_null()
          ? std::make_unique<ClientControlledStateDelegate>(this)
          : g_factory_callback.Run();

  auto window_delegate =
      std::make_unique<ClientControlledWindowStateDelegate>(delegate.get());
  auto state =
      std::make_unique<ash::wm::ClientControlledState>(std::move(delegate));
  client_controlled_state_ = state.get();
  window_state->SetStateObject(std::move(state));
  window_state->SetDelegate(std::move(window_delegate));
  return ShellSurfaceBase::CreateNonClientFrameView(widget);
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
  if (!resizer_ || resizer_->details().window_component != HTCAPTION) {
    client_controlled_state_->set_bounds_locally(true);
    widget_->SetBounds(bounds);
    client_controlled_state_->set_bounds_locally(false);
    UpdateSurfaceBounds();
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
  client_controlled_state_->set_bounds_locally(true);
  widget_->GetNativeWindow()->SetBounds(gfx::Rect(origin, bounds.size()));
  client_controlled_state_->set_bounds_locally(false);
  UpdateSurfaceBounds();

  // Render phantom windows when beyond the current display.
  resizer_->Drag(GetMouseLocation(), 0);
}

gfx::Rect ClientControlledShellSurface::GetShadowBounds() const {
  gfx::Rect shadow_bounds = ShellSurfaceBase::GetShadowBounds();

  if (geometry_changed_callback_.is_null()) {
    aura::Window* window = widget_->GetNativeWindow();

    // Convert from screen to display coordinates.
    shadow_bounds -= origin_offset_;
    wm::ConvertRectFromScreen(window->parent(), &shadow_bounds);

    // Convert from display to window coordinates.
    shadow_bounds -= window->bounds().OffsetFromOrigin();
  }

  return shadow_bounds;
}

void ClientControlledShellSurface::InitializeWindowState(
    ash::wm::WindowState* window_state) {
  // Allow the client to request bounds that do not fill the entire work area
  // when maximized, or the entire display when fullscreen.
  window_state->set_allow_set_bounds_direct(true);
  widget_->set_movement_disabled(true);
  window_state->set_ignore_keyboard_bounds_change(true);
}

float ClientControlledShellSurface::GetScale() const {
  return scale_;
}

aura::Window* ClientControlledShellSurface::GetDragWindow() {
  // Set capture on the root surface rather than the focus surface, because
  // the client may destroy the latter during dragging/resizing.
  return root_surface() ? root_surface()->window() : nullptr;
}

std::unique_ptr<ash::WindowResizer>
ClientControlledShellSurface::CreateWindowResizer(aura::Window* window,
                                                  int component) {
  ash::wm::WindowState* window_state = GetWindowState();
  DCHECK(!window_state->drag_details());
  window_state->CreateDragDetails(GetMouseLocation(), component,
                                  wm::WINDOW_MOVE_SOURCE_MOUSE);

  std::unique_ptr<ash::WindowResizer> resizer =
      std::make_unique<CustomWindowResizer>(window_state);

  if (component == HTCAPTION) {
    // Chained with a CustomWindowResizer, DragWindowResizer does not handle
    // dragging. It only renders phantom windows and moves the window to the
    // target root window when dragging ends.
    resizer.reset(
        ash::DragWindowResizer::Create(resizer.release(), window_state));
  }

  return resizer;
}

bool ClientControlledShellSurface::OnMouseDragged(const ui::MouseEvent&) {
  // TODO(domlaskowski): When VKEY_ESCAPE is pressed during dragging, the client
  // destroys the window, but should instead revert the drag to be consistent
  // with ShellSurface::OnKeyEvent. See crbug.com/699746.
  return false;
}

gfx::Point ClientControlledShellSurface::GetWidgetOrigin() const {
  return GetVisibleBounds().origin() - origin_offset_;
}

gfx::Point ClientControlledShellSurface::GetSurfaceOrigin() const {
  DCHECK(resize_component_ == HTCAPTION);
  if (!geometry_changed_callback_.is_null())
    return gfx::Point();
  return gfx::Point() - GetWidgetOrigin().OffsetFromOrigin();
}

////////////////////////////////////////////////////////////////////////////////
// ClientControlledShellSurface, private:

void ClientControlledShellSurface::UpdateBackdrop() {
  aura::Window* window = widget_->GetNativeWindow();
  const display::Display display =
      display::Screen::GetScreen()->GetDisplayNearestWindow(window);
  bool enable_backdrop =
      (widget_->IsFullscreen() || widget_->IsMaximized()) &&
      !widget_->GetWindowBoundsInScreen().Contains(display.work_area());

  if (window->GetProperty(aura::client::kHasBackdrop) != enable_backdrop)
    window->SetProperty(aura::client::kHasBackdrop, enable_backdrop);
}

void ClientControlledShellSurface::
    EnsureCompositorIsLockedForOrientationChange() {
  if (!orientation_compositor_lock_) {
    ui::Compositor* compositor =
        widget_->GetNativeWindow()->layer()->GetCompositor();
    orientation_compositor_lock_ = compositor->GetCompositorLock(
        this, base::TimeDelta::FromMilliseconds(kOrientationLockTimeoutMs));
  }
}

ash::wm::WindowState* ClientControlledShellSurface::GetWindowState() {
  return ash::wm::GetWindowState(widget_->GetNativeWindow());
}

// static
void ClientControlledShellSurface::
    SetClientControlledStateDelegateFactoryForTest(
        const DelegateFactoryCallback& callback) {
  g_factory_callback = callback;
}

}  // namespace exo
