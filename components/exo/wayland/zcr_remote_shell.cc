// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/wayland/zcr_remote_shell.h"

#include <remote-shell-unstable-v1-server-protocol.h>
#include <wayland-server-core.h>
#include <wayland-server-protocol-core.h>

#include "ash/public/cpp/caption_buttons/caption_button_types.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/public/cpp/window_properties.h"
#include "ash/public/interfaces/window_pin_type.mojom.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "ash/wm/window_resizer.h"
#include "ash/wm/window_state.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/exo/client_controlled_shell_surface.h"
#include "components/exo/display.h"
#include "components/exo/input_method_surface.h"
#include "components/exo/notification_surface.h"
#include "components/exo/shell_surface.h"
#include "components/exo/shell_surface_base.h"
#include "components/exo/surface_delegate.h"
#include "components/exo/wayland/server_util.h"
#include "components/exo/wm_helper_chromeos.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/public/activation_change_observer.h"

namespace exo {
namespace wayland {

namespace {

namespace switches {

// This flag can be used to emulate device scale factor for remote shell.
constexpr char kForceRemoteShellScale[] = "force-remote-shell-scale";

}  // namespace switches

// We don't send configure immediately after tablet mode switch
// because layout can change due to orientation lock state or accelerometer.
const int kConfigureDelayAfterLayoutSwitchMs = 300;

uint32_t ResizeDirection(int component) {
  switch (component) {
    case HTCAPTION:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
    case HTTOP:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP;
    case HTTOPRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT;
    case HTRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT;
    case HTBOTTOMRIGHT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT;
    case HTBOTTOM:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM;
    case HTBOTTOMLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT;
    case HTLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT;
    case HTTOPLEFT:
      return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT;
    default:
      LOG(ERROR) << "Unknown component:" << component;
      break;
  }
  NOTREACHED();
  return ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE;
}

// Returns the scale factor to be used by remote shell clients.
double GetDefaultDeviceScaleFactor() {
  // A flag used by VM to emulate a device scale for a particular board.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kForceRemoteShellScale)) {
    std::string value =
        command_line->GetSwitchValueASCII(switches::kForceRemoteShellScale);
    double scale = 1.0;
    if (base::StringToDouble(value, &scale))
      return std::max(1.0, scale);
  }
  return WMHelper::GetInstance()->GetDefaultDeviceScaleFactor();
}

int Component(uint32_t direction) {
  switch (direction) {
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_NONE:
      return HTNOWHERE;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOP:
      return HTTOP;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPRIGHT:
      return HTTOPRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_RIGHT:
      return HTRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMRIGHT:
      return HTBOTTOMRIGHT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOM:
      return HTBOTTOM;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_BOTTOMLEFT:
      return HTBOTTOMLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_LEFT:
      return HTLEFT;
    case ZCR_REMOTE_SURFACE_V1_RESIZE_DIRECTION_TOPLEFT:
      return HTTOPLEFT;
    default:
      VLOG(2) << "Unknown direction:" << direction;
      break;
  }
  return HTNOWHERE;
}

uint32_t CaptionButtonMask(uint32_t mask) {
  uint32_t caption_button_icon_mask = 0;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_BACK)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_BACK;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MENU)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_MENU;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MINIMIZE)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_MINIMIZE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_MAXIMIZE_RESTORE)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_CLOSE)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_CLOSE;
  if (mask & ZCR_REMOTE_SURFACE_V1_FRAME_BUTTON_TYPE_ZOOM)
    caption_button_icon_mask |= 1 << ash::CAPTION_BUTTON_ICON_ZOOM;
  return caption_button_icon_mask;
}

////////////////////////////////////////////////////////////////////////////////
// remote_surface_interface:

SurfaceFrameType RemoteShellSurfaceFrameType(uint32_t frame_type) {
  switch (frame_type) {
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NONE:
      return SurfaceFrameType::NONE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_NORMAL:
      return SurfaceFrameType::NORMAL;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_SHADOW:
      return SurfaceFrameType::SHADOW;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_AUTOHIDE:
      return SurfaceFrameType::AUTOHIDE;
    case ZCR_REMOTE_SURFACE_V1_FRAME_TYPE_OVERLAY:
      return SurfaceFrameType::OVERLAY;
    default:
      VLOG(2) << "Unknown remote-shell frame type: " << frame_type;
      return SurfaceFrameType::NONE;
  }
}

void remote_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void remote_surface_set_app_id(wl_client* client,
                               wl_resource* resource,
                               const char* app_id) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetApplicationId(app_id);
}

void remote_surface_set_window_geometry(wl_client* client,
                                        wl_resource* resource,
                                        int32_t x,
                                        int32_t y,
                                        int32_t width,
                                        int32_t height) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetGeometry(
      gfx::Rect(x, y, width, height));
}

void remote_surface_set_orientation(wl_client* client,
                                    wl_resource* resource,
                                    int32_t orientation) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientation(
      orientation == ZCR_REMOTE_SURFACE_V1_ORIENTATION_PORTRAIT
          ? Orientation::PORTRAIT
          : Orientation::LANDSCAPE);
}

void remote_surface_set_scale(wl_client* client,
                              wl_resource* resource,
                              wl_fixed_t scale) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetScale(
      wl_fixed_to_double(scale));
}

void remote_surface_set_rectangular_shadow_DEPRECATED(wl_client* client,
                                                      wl_resource* resource,
                                                      int32_t x,
                                                      int32_t y,
                                                      int32_t width,
                                                      int32_t height) {
  NOTIMPLEMENTED();
}

void remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED(
    wl_client* client,
    wl_resource* resource,
    wl_fixed_t opacity) {
  NOTIMPLEMENTED();
}

void remote_surface_set_title(wl_client* client,
                              wl_resource* resource,
                              const char* title) {
  GetUserDataAs<ShellSurfaceBase>(resource)->SetTitle(
      base::string16(base::UTF8ToUTF16(title)));
}

void remote_surface_set_top_inset(wl_client* client,
                                  wl_resource* resource,
                                  int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetTopInset(height);
}

void remote_surface_activate(wl_client* client,
                             wl_resource* resource,
                             uint32_t serial) {
  ShellSurfaceBase* shell_surface = GetUserDataAs<ShellSurfaceBase>(resource);
  shell_surface->Activate();
}

void remote_surface_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximized();
}

void remote_surface_minimize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimized();
}

void remote_surface_restore(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetRestored();
}

void remote_surface_fullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(true);
}

void remote_surface_unfullscreen(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFullscreen(false);
}

void remote_surface_pin(wl_client* client,
                        wl_resource* resource,
                        int32_t trusted) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      trusted ? ash::mojom::WindowPinType::TRUSTED_PINNED
              : ash::mojom::WindowPinType::PINNED);
}

void remote_surface_unpin(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPinned(
      ash::mojom::WindowPinType::NONE);
}

void remote_surface_set_system_modal(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(true);
}

void remote_surface_unset_system_modal(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemModal(false);
}

void remote_surface_set_rectangular_surface_shadow(wl_client* client,
                                                   wl_resource* resource,
                                                   int32_t x,
                                                   int32_t y,
                                                   int32_t width,
                                                   int32_t height) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->SetShadowBounds(gfx::Rect(x, y, width, height));
}

void remote_surface_set_systemui_visibility(wl_client* client,
                                            wl_resource* resource,
                                            uint32_t visibility) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSystemUiVisibility(
      visibility != ZCR_REMOTE_SURFACE_V1_SYSTEMUI_VISIBILITY_STATE_VISIBLE);
}

void remote_surface_set_always_on_top(wl_client* client,
                                      wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(true);
}

void remote_surface_unset_always_on_top(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetAlwaysOnTop(false);
}

void remote_surface_ack_configure(wl_client* client,
                                  wl_resource* resource,
                                  uint32_t serial) {
  // DEPRECATED
}

void remote_surface_move(wl_client* client, wl_resource* resource) {
  // DEPRECATED
}

void remote_surface_set_window_type(wl_client* client,
                                    wl_resource* resource,
                                    uint32_t type) {
  if (type == ZCR_REMOTE_SURFACE_V1_WINDOW_TYPE_SYSTEM_UI) {
    auto* widget = GetUserDataAs<ShellSurfaceBase>(resource)->GetWidget();
    if (widget) {
      widget->GetNativeWindow()->SetProperty(ash::kHideInOverviewKey, true);

      wm::SetWindowVisibilityAnimationType(
          widget->GetNativeWindow(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
    }
  }
}

void remote_surface_resize(wl_client* client, wl_resource* resource) {
  // DEPRECATED
}

void remote_surface_set_resize_outset(wl_client* client,
                                      wl_resource* resource,
                                      int32_t outset) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetResizeOutset(
      outset);
}

void remote_surface_start_move(wl_client* client,
                               wl_resource* resource,
                               int32_t x,
                               int32_t y) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->StartDrag(
      HTCAPTION, gfx::Point(x, y));
}

void remote_surface_set_can_maximize(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(true);
}

void remote_surface_unset_can_maximize(wl_client* client,
                                       wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetCanMaximize(false);
}

void remote_surface_set_min_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMinimumSize(
      gfx::Size(width, height));
}

void remote_surface_set_max_size(wl_client* client,
                                 wl_resource* resource,
                                 int32_t width,
                                 int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetMaximumSize(
      gfx::Size(width, height));
}

void remote_surface_set_snapped_to_left(wl_client* client,
                                        wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnappedToLeft();
}

void remote_surface_set_snapped_to_right(wl_client* client,
                                         wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetSnappedToRight();
}

void remote_surface_start_resize(wl_client* client,
                                 wl_resource* resource,
                                 uint32_t direction,
                                 int32_t x,
                                 int32_t y) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->StartDrag(
      Component(direction), gfx::Point(x, y));
}

void remote_surface_set_frame(wl_client* client,
                              wl_resource* resource,
                              uint32_t type) {
  ClientControlledShellSurface* shell_surface =
      GetUserDataAs<ClientControlledShellSurface>(resource);
  shell_surface->root_surface()->SetFrame(RemoteShellSurfaceFrameType(type));
}

void remote_surface_set_frame_buttons(wl_client* client,
                                      wl_resource* resource,
                                      uint32_t visible_button_mask,
                                      uint32_t enabled_button_mask) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetFrameButtons(
      CaptionButtonMask(visible_button_mask),
      CaptionButtonMask(enabled_button_mask));
}

void remote_surface_set_extra_title(wl_client* client,
                                    wl_resource* resource,
                                    const char* extra_title) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetExtraTitle(
      base::string16(base::UTF8ToUTF16(extra_title)));
}

ash::OrientationLockType OrientationLock(uint32_t orientation_lock) {
  switch (orientation_lock) {
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_NONE:
      return ash::OrientationLockType::kAny;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_CURRENT:
      return ash::OrientationLockType::kCurrent;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT:
      return ash::OrientationLockType::kPortrait;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE:
      return ash::OrientationLockType::kLandscape;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_PRIMARY:
      return ash::OrientationLockType::kPortraitPrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_PORTRAIT_SECONDARY:
      return ash::OrientationLockType::kPortraitSecondary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_PRIMARY:
      return ash::OrientationLockType::kLandscapePrimary;
    case ZCR_REMOTE_SURFACE_V1_ORIENTATION_LOCK_LANDSCAPE_SECONDARY:
      return ash::OrientationLockType::kLandscapeSecondary;
  }
  VLOG(2) << "Unexpected value of orientation_lock: " << orientation_lock;
  return ash::OrientationLockType::kAny;
}

void remote_surface_set_orientation_lock(wl_client* client,
                                         wl_resource* resource,
                                         uint32_t orientation_lock) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetOrientationLock(
      OrientationLock(orientation_lock));
}

void remote_surface_pip(wl_client* client, wl_resource* resource) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetPip();
}

void remote_surface_set_bounds(wl_client* client,
                               wl_resource* resource,
                               uint32_t display_id_hi,
                               uint32_t display_id_lo,
                               int32_t x,
                               int32_t y,
                               int32_t width,
                               int32_t height) {
  GetUserDataAs<ClientControlledShellSurface>(resource)->SetBounds(
      static_cast<int64_t>(display_id_hi) << 32 | display_id_lo,
      gfx::Rect(x, y, width, height));
}

const struct zcr_remote_surface_v1_interface remote_surface_implementation = {
    remote_surface_destroy,
    remote_surface_set_app_id,
    remote_surface_set_window_geometry,
    remote_surface_set_scale,
    remote_surface_set_rectangular_shadow_DEPRECATED,
    remote_surface_set_rectangular_shadow_background_opacity_DEPRECATED,
    remote_surface_set_title,
    remote_surface_set_top_inset,
    remote_surface_activate,
    remote_surface_maximize,
    remote_surface_minimize,
    remote_surface_restore,
    remote_surface_fullscreen,
    remote_surface_unfullscreen,
    remote_surface_pin,
    remote_surface_unpin,
    remote_surface_set_system_modal,
    remote_surface_unset_system_modal,
    remote_surface_set_rectangular_surface_shadow,
    remote_surface_set_systemui_visibility,
    remote_surface_set_always_on_top,
    remote_surface_unset_always_on_top,
    remote_surface_ack_configure,
    remote_surface_move,
    remote_surface_set_orientation,
    remote_surface_set_window_type,
    remote_surface_resize,
    remote_surface_set_resize_outset,
    remote_surface_start_move,
    remote_surface_set_can_maximize,
    remote_surface_unset_can_maximize,
    remote_surface_set_min_size,
    remote_surface_set_max_size,
    remote_surface_set_snapped_to_left,
    remote_surface_set_snapped_to_right,
    remote_surface_start_resize,
    remote_surface_set_frame,
    remote_surface_set_frame_buttons,
    remote_surface_set_extra_title,
    remote_surface_set_orientation_lock,
    remote_surface_pip,
    remote_surface_set_bounds};

////////////////////////////////////////////////////////////////////////////////
// notification_surface_interface:

void notification_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

void notification_surface_set_app_id(wl_client* client,
                                     wl_resource* resource,
                                     const char* app_id) {
  GetUserDataAs<NotificationSurface>(resource)->SetApplicationId(app_id);
}

const struct zcr_notification_surface_v1_interface
    notification_surface_implementation = {notification_surface_destroy,
                                           notification_surface_set_app_id};

////////////////////////////////////////////////////////////////////////////////
// input_method_surface_interface:

void input_method_surface_destroy(wl_client* client, wl_resource* resource) {
  wl_resource_destroy(resource);
}

const struct zcr_input_method_surface_v1_interface
    input_method_surface_implementation = {input_method_surface_destroy};

////////////////////////////////////////////////////////////////////////////////
// remote_shell_interface:

// Implements remote shell interface and monitors workspace state needed
// for the remote shell interface.
class WaylandRemoteShell : public ash::TabletModeObserver,
                           public wm::ActivationChangeObserver,
                           public display::DisplayObserver {
 public:
  WaylandRemoteShell(Display* display, wl_resource* remote_shell_resource)
      : display_(display),
        remote_shell_resource_(remote_shell_resource),
        weak_ptr_factory_(this) {
    WMHelperChromeOS* helper = WMHelperChromeOS::GetInstance();
    helper->AddTabletModeObserver(this);
    helper->AddActivationObserver(this);
    display::Screen::GetScreen()->AddObserver(this);

    layout_mode_ = helper->IsTabletModeWindowManagerEnabled()
                       ? ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET
                       : ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

    if (wl_resource_get_version(remote_shell_resource_) >= 8) {
      double scale_factor = GetDefaultDeviceScaleFactor();
      // Send using 16.16 fixed point.
      const int kDecimalBits = 24;
      int32_t fixed_scale =
          static_cast<int32_t>(scale_factor * (1 << kDecimalBits));
      zcr_remote_shell_v1_send_default_device_scale_factor(
          remote_shell_resource_, fixed_scale);
    }

    SendDisplayMetrics();
    SendActivated(helper->GetActiveWindow(), nullptr);
  }
  ~WaylandRemoteShell() override {
    WMHelperChromeOS* helper = WMHelperChromeOS::GetInstance();
    helper->RemoveTabletModeObserver(this);
    helper->RemoveActivationObserver(this);
    display::Screen::GetScreen()->RemoveObserver(this);
  }

  std::unique_ptr<ClientControlledShellSurface> CreateShellSurface(
      Surface* surface,
      int container,
      double default_device_scale_factor) {
    return display_->CreateClientControlledShellSurface(
        surface, container, default_device_scale_factor);
  }

  std::unique_ptr<NotificationSurface> CreateNotificationSurface(
      Surface* surface,
      const std::string& notification_key) {
    return display_->CreateNotificationSurface(surface, notification_key);
  }

  std::unique_ptr<InputMethodSurface> CreateInputMethodSurface(
      Surface* surface,
      double default_device_scale_factor) {
    return display_->CreateInputMethodSurface(surface,
                                              default_device_scale_factor);
  }

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override {
    ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayRemoved(const display::Display& old_display) override {
    ScheduleSendDisplayMetrics(0);
  }

  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override {
    // No need to update when a primary display has changed without bounds
    // change. See WaylandDisplayObserver::OnDisplayMetricsChanged
    // for more details.
    if (changed_metrics &
        (DISPLAY_METRIC_BOUNDS | DISPLAY_METRIC_DEVICE_SCALE_FACTOR |
         DISPLAY_METRIC_ROTATION | DISPLAY_METRIC_WORK_AREA)) {
      ScheduleSendDisplayMetrics(0);
    }
  }

  // Overridden from ash::TabletModeObserver:
  void OnTabletModeStarted() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_TABLET;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnding() override {
    layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;
    ScheduleSendDisplayMetrics(kConfigureDelayAfterLayoutSwitchMs);
  }
  void OnTabletModeEnded() override {}

  // Overridden from wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override {
    SendActivated(gained_active, lost_active);
  }

 private:
  void ScheduleSendDisplayMetrics(int delay_ms) {
    needs_send_display_metrics_ = true;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&WaylandRemoteShell::SendDisplayMetrics,
                       weak_ptr_factory_.GetWeakPtr()),
        base::TimeDelta::FromMilliseconds(delay_ms));
  }

  // Returns the transform that a display's output is currently adjusted for.
  wl_output_transform DisplayTransform(display::Display::Rotation rotation) {
    switch (rotation) {
      case display::Display::ROTATE_0:
        return WL_OUTPUT_TRANSFORM_NORMAL;
      case display::Display::ROTATE_90:
        return WL_OUTPUT_TRANSFORM_90;
      case display::Display::ROTATE_180:
        return WL_OUTPUT_TRANSFORM_180;
      case display::Display::ROTATE_270:
        return WL_OUTPUT_TRANSFORM_270;
    }
    NOTREACHED();
    return WL_OUTPUT_TRANSFORM_NORMAL;
  }

  void SendDisplayMetrics() {
    if (!needs_send_display_metrics_)
      return;
    needs_send_display_metrics_ = false;

    const display::Screen* screen = display::Screen::GetScreen();

    for (const auto& display : screen->GetAllDisplays()) {
      const gfx::Rect& bounds = display.bounds();
      const gfx::Insets& insets = display.GetWorkAreaInsets();

      double device_scale_factor = display.device_scale_factor();

      uint32_t display_id_hi = static_cast<uint32_t>(display.id() >> 32);
      uint32_t display_id_lo = static_cast<uint32_t>(display.id());

      zcr_remote_shell_v1_send_workspace(
          remote_shell_resource_, display_id_hi, display_id_lo, bounds.x(),
          bounds.y(), bounds.width(), bounds.height(), insets.left(),
          insets.top(), insets.right(), insets.bottom(),
          DisplayTransform(display.rotation()),
          wl_fixed_from_double(device_scale_factor), display.IsInternal());

      if (wl_resource_get_version(remote_shell_resource_) >= 19) {
        gfx::Size size_in_pixel = display.GetSizeInPixel();

        wl_array data;
        wl_array_init(&data);

        const auto& bytes =
            WMHelper::GetInstance()->GetDisplayIdentificationData(display.id());
        for (uint8_t byte : bytes) {
          uint8_t* ptr =
              static_cast<uint8_t*>(wl_array_add(&data, sizeof(uint8_t)));
          DCHECK(ptr);
          *ptr = byte;
        }

        zcr_remote_shell_v1_send_display_info(
            remote_shell_resource_, display_id_hi, display_id_lo,
            size_in_pixel.width(), size_in_pixel.height(), &data);

        wl_array_release(&data);
      }
    }

    zcr_remote_shell_v1_send_configure(remote_shell_resource_, layout_mode_);
    wl_client_flush(wl_resource_get_client(remote_shell_resource_));
  }

  void SendActivated(aura::Window* gained_active, aura::Window* lost_active) {
    Surface* gained_active_surface =
        gained_active ? ShellSurface::GetMainSurface(gained_active) : nullptr;
    Surface* lost_active_surface =
        lost_active ? ShellSurface::GetMainSurface(lost_active) : nullptr;
    wl_resource* gained_active_surface_resource =
        gained_active_surface ? GetSurfaceResource(gained_active_surface)
                              : nullptr;
    wl_resource* lost_active_surface_resource =
        lost_active_surface ? GetSurfaceResource(lost_active_surface) : nullptr;

    wl_client* client = wl_resource_get_client(remote_shell_resource_);

    // If surface that gained active is not owned by remote shell client then
    // set it to null.
    if (gained_active_surface_resource &&
        wl_resource_get_client(gained_active_surface_resource) != client) {
      gained_active_surface_resource = nullptr;
    }

    // If surface that lost active is not owned by remote shell client then
    // set it to null.
    if (lost_active_surface_resource &&
        wl_resource_get_client(lost_active_surface_resource) != client) {
      lost_active_surface_resource = nullptr;
    }

    zcr_remote_shell_v1_send_activated(remote_shell_resource_,
                                       gained_active_surface_resource,
                                       lost_active_surface_resource);
    wl_client_flush(client);
  }

  // The exo display instance. Not owned.
  Display* const display_;

  // The remote shell resource associated with observer.
  wl_resource* const remote_shell_resource_;

  bool needs_send_display_metrics_ = true;

  int layout_mode_ = ZCR_REMOTE_SHELL_V1_LAYOUT_MODE_WINDOWED;

  base::WeakPtrFactory<WaylandRemoteShell> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandRemoteShell);
};

void remote_shell_destroy(wl_client* client, wl_resource* resource) {
  // Nothing to do here.
}

int RemoteSurfaceContainer(uint32_t container) {
  switch (container) {
    case ZCR_REMOTE_SHELL_V1_CONTAINER_DEFAULT:
      return ash::kShellWindowId_DefaultContainer;
    case ZCR_REMOTE_SHELL_V1_CONTAINER_OVERLAY:
      return ash::kShellWindowId_SystemModalContainer;
    default:
      DLOG(WARNING) << "Unsupported container: " << container;
      return ash::kShellWindowId_DefaultContainer;
  }
}

void HandleRemoteSurfaceCloseCallback(wl_resource* resource) {
  zcr_remote_surface_v1_send_close(resource);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceStateChangedCallback(
    wl_resource* resource,
    ash::mojom::WindowStateType old_state_type,
    ash::mojom::WindowStateType new_state_type) {
  DCHECK_NE(old_state_type, new_state_type);

  uint32_t state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_NORMAL;
  switch (new_state_type) {
    case ash::mojom::WindowStateType::MINIMIZED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MINIMIZED;
      break;
    case ash::mojom::WindowStateType::MAXIMIZED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_MAXIMIZED;
      break;
    case ash::mojom::WindowStateType::FULLSCREEN:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_FULLSCREEN;
      break;
    case ash::mojom::WindowStateType::PINNED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PINNED;
      break;
    case ash::mojom::WindowStateType::TRUSTED_PINNED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_TRUSTED_PINNED;
      break;
    case ash::mojom::WindowStateType::LEFT_SNAPPED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_LEFT_SNAPPED;
      break;
    case ash::mojom::WindowStateType::RIGHT_SNAPPED:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_RIGHT_SNAPPED;
      break;
    case ash::mojom::WindowStateType::PIP:
      state_type = ZCR_REMOTE_SHELL_V1_STATE_TYPE_PIP;
      break;
    default:
      break;
  }

  zcr_remote_surface_v1_send_state_type_changed(resource, state_type);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceBoundsChangedCallback(
    wl_resource* resource,
    ash::mojom::WindowStateType current_state,
    ash::mojom::WindowStateType requested_state,
    int64_t display_id,
    const gfx::Rect& bounds,
    bool resize,
    int bounds_change) {
  zcr_remote_surface_v1_bounds_change_reason reason =
      resize ? ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_RESIZE
             : ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_MOVE;
  if (bounds_change & ash::WindowResizer::kBoundsChange_Resizes) {
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_RESIZE;
  } else if (bounds_change & ash::WindowResizer::kBoundsChange_Repositions) {
    reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_DRAG_MOVE;
  }
  // Override the reason only if the window enters snapped mode. If the window
  // resizes by dragging in snapped mode, we need to keep the original reason.
  if (requested_state != current_state) {
    if (requested_state == ash::mojom::WindowStateType::LEFT_SNAPPED) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_LEFT;
    } else if (requested_state == ash::mojom::WindowStateType::RIGHT_SNAPPED) {
      reason = ZCR_REMOTE_SURFACE_V1_BOUNDS_CHANGE_REASON_SNAP_TO_RIGHT;
    }
  }
  zcr_remote_surface_v1_send_bounds_changed(
      resource, static_cast<uint32_t>(display_id >> 32),
      static_cast<uint32_t>(display_id), bounds.x(), bounds.y(), bounds.width(),
      bounds.height(), reason);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceDragStartedCallback(wl_resource* resource,
                                            int component) {
  zcr_remote_surface_v1_send_drag_started(resource, ResizeDirection(component));
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceDragFinishedCallback(wl_resource* resource,
                                             int x,
                                             int y,
                                             bool canceled) {
  zcr_remote_surface_v1_send_drag_finished(resource, x, y, canceled ? 1 : 0);
  wl_client_flush(wl_resource_get_client(resource));
}

void HandleRemoteSurfaceGeometryChangedCallback(wl_resource* resource,
                                                const gfx::Rect& geometry) {
  zcr_remote_surface_v1_send_window_geometry_changed(
      resource, geometry.x(), geometry.y(), geometry.width(),
      geometry.height());
  wl_client_flush(wl_resource_get_client(resource));
}

void remote_shell_get_remote_surface(wl_client* client,
                                     wl_resource* resource,
                                     uint32_t id,
                                     wl_resource* surface,
                                     uint32_t container) {
  WaylandRemoteShell* shell = GetUserDataAs<WaylandRemoteShell>(resource);
  double default_scale_factor = wl_resource_get_version(resource) >= 8
                                    ? GetDefaultDeviceScaleFactor()
                                    : 1.0;

  std::unique_ptr<ClientControlledShellSurface> shell_surface =
      shell->CreateShellSurface(GetUserDataAs<Surface>(surface),
                                RemoteSurfaceContainer(container),
                                default_scale_factor);
  if (!shell_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  wl_resource* remote_surface_resource =
      wl_resource_create(client, &zcr_remote_surface_v1_interface,
                         wl_resource_get_version(resource), id);

  shell_surface->set_close_callback(
      base::Bind(&HandleRemoteSurfaceCloseCallback,
                 base::Unretained(remote_surface_resource)));
  shell_surface->set_state_changed_callback(
      base::Bind(&HandleRemoteSurfaceStateChangedCallback,
                 base::Unretained(remote_surface_resource)));
  shell_surface->set_geometry_changed_callback(
      base::BindRepeating(&HandleRemoteSurfaceGeometryChangedCallback,
                          base::Unretained(remote_surface_resource)));

  if (wl_resource_get_version(remote_surface_resource) >= 10) {
    shell_surface->set_client_controlled_move_resize(false);
    shell_surface->set_bounds_changed_callback(
        base::BindRepeating(&HandleRemoteSurfaceBoundsChangedCallback,
                            base::Unretained(remote_surface_resource)));
    shell_surface->set_drag_started_callback(
        base::BindRepeating(&HandleRemoteSurfaceDragStartedCallback,
                            base::Unretained(remote_surface_resource)));
    shell_surface->set_drag_finished_callback(
        base::BindRepeating(&HandleRemoteSurfaceDragFinishedCallback,
                            base::Unretained(remote_surface_resource)));
  }

  SetImplementation(remote_surface_resource, &remote_surface_implementation,
                    std::move(shell_surface));
}

void remote_shell_get_notification_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface,
                                           const char* notification_key) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<NotificationSurface> notification_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateNotificationSurface(
          GetUserDataAs<Surface>(surface), std::string(notification_key));
  if (!notification_surface) {
    wl_resource_post_error(resource,
                           ZCR_REMOTE_SHELL_V1_ERROR_INVALID_NOTIFICATION_KEY,
                           "invalid notification key");
    return;
  }

  wl_resource* notification_surface_resource =
      wl_resource_create(client, &zcr_notification_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(notification_surface_resource,
                    &notification_surface_implementation,
                    std::move(notification_surface));
}

void remote_shell_get_input_method_surface(wl_client* client,
                                           wl_resource* resource,
                                           uint32_t id,
                                           wl_resource* surface) {
  if (GetUserDataAs<Surface>(surface)->HasSurfaceDelegate()) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "surface has already been assigned a role");
    return;
  }

  std::unique_ptr<ClientControlledShellSurface> input_method_surface =
      GetUserDataAs<WaylandRemoteShell>(resource)->CreateInputMethodSurface(
          GetUserDataAs<Surface>(surface), GetDefaultDeviceScaleFactor());
  if (!input_method_surface) {
    wl_resource_post_error(resource, ZCR_REMOTE_SHELL_V1_ERROR_ROLE,
                           "Cannot create an IME surface");
    return;
  }

  wl_resource* input_method_surface_resource =
      wl_resource_create(client, &zcr_input_method_surface_v1_interface,
                         wl_resource_get_version(resource), id);
  SetImplementation(input_method_surface_resource,
                    &input_method_surface_implementation,
                    std::move(input_method_surface));
}

const struct zcr_remote_shell_v1_interface remote_shell_implementation = {
    remote_shell_destroy, remote_shell_get_remote_surface,
    remote_shell_get_notification_surface,
    remote_shell_get_input_method_surface};

}  // namespace

void bind_remote_shell(wl_client* client,
                       void* data,
                       uint32_t version,
                       uint32_t id) {
  wl_resource* resource =
      wl_resource_create(client, &zcr_remote_shell_v1_interface,
                         std::min(version, kZcrRemoteShellVersion), id);

  SetImplementation(resource, &remote_shell_implementation,
                    std::make_unique<WaylandRemoteShell>(
                        static_cast<Display*>(data), resource));
}

}  // namespace wayland
}  // namespace exo
