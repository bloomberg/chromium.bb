// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/host/ash_window_tree_host_x11.h"

#include <X11/extensions/Xfixes.h>
#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <string>
#include <vector>

#include "ash/host/ash_window_tree_host_init_params.h"
#include "ash/host/root_window_transformer.h"
#include "base/basictypes.h"
#include "base/sys_info.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/device_data_manager.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/x/device_list_cache_x.h"
#include "ui/events/x/touch_factory_x11.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {

AshWindowTreeHostX11::AshWindowTreeHostX11(const gfx::Rect& initial_bounds)
    : WindowTreeHostX11(initial_bounds),
      transformer_helper_(this),
      display_ids_(std::make_pair(gfx::Display::kInvalidDisplayID,
                                  gfx::Display::kInvalidDisplayID)) {
  aura::Env::GetInstance()->AddObserver(this);
}

AshWindowTreeHostX11::~AshWindowTreeHostX11() {
  aura::Env::GetInstance()->RemoveObserver(this);
  UnConfineCursor();
}

void AshWindowTreeHostX11::ToggleFullScreen() { NOTIMPLEMENTED(); }

bool AshWindowTreeHostX11::ConfineCursorToRootWindow() {
#if XFIXES_MAJOR >= 5
  DCHECK(!pointer_barriers_.get());
  if (pointer_barriers_)
    return false;
  pointer_barriers_.reset(new XID[4]);
  gfx::Rect barrier(bounds());
  barrier.Inset(transformer_helper_.GetHostInsets());
  // Horizontal, top barriers.
  pointer_barriers_[0] = XFixesCreatePointerBarrier(xdisplay(),
                                                    x_root_window(),
                                                    barrier.x(),
                                                    barrier.y(),
                                                    barrier.right(),
                                                    barrier.y(),
                                                    BarrierPositiveY,
                                                    0,
                                                    XIAllDevices);
  // Horizontal, bottom barriers.
  pointer_barriers_[1] = XFixesCreatePointerBarrier(xdisplay(),
                                                    x_root_window(),
                                                    barrier.x(),
                                                    barrier.bottom(),
                                                    barrier.right(),
                                                    barrier.bottom(),
                                                    BarrierNegativeY,
                                                    0,
                                                    XIAllDevices);
  // Vertical, left  barriers.
  pointer_barriers_[2] = XFixesCreatePointerBarrier(xdisplay(),
                                                    x_root_window(),
                                                    barrier.x(),
                                                    barrier.y(),
                                                    barrier.x(),
                                                    barrier.bottom(),
                                                    BarrierPositiveX,
                                                    0,
                                                    XIAllDevices);
  // Vertical, right barriers.
  pointer_barriers_[3] = XFixesCreatePointerBarrier(xdisplay(),
                                                    x_root_window(),
                                                    barrier.right(),
                                                    barrier.y(),
                                                    barrier.right(),
                                                    barrier.bottom(),
                                                    BarrierNegativeX,
                                                    0,
                                                    XIAllDevices);
#endif
  return true;
}

void AshWindowTreeHostX11::UnConfineCursor() {
#if XFIXES_MAJOR >= 5
  if (pointer_barriers_) {
    XFixesDestroyPointerBarrier(xdisplay(), pointer_barriers_[0]);
    XFixesDestroyPointerBarrier(xdisplay(), pointer_barriers_[1]);
    XFixesDestroyPointerBarrier(xdisplay(), pointer_barriers_[2]);
    XFixesDestroyPointerBarrier(xdisplay(), pointer_barriers_[3]);
    pointer_barriers_.reset();
  }
#endif
}

void AshWindowTreeHostX11::SetRootWindowTransformer(
    scoped_ptr<RootWindowTransformer> transformer) {
  transformer_helper_.SetRootWindowTransformer(transformer.Pass());
  if (pointer_barriers_) {
    UnConfineCursor();
    ConfineCursorToRootWindow();
  }
}

gfx::Insets AshWindowTreeHostX11::GetHostInsets() const {
  return transformer_helper_.GetHostInsets();
}

aura::WindowTreeHost* AshWindowTreeHostX11::AsWindowTreeHost() { return this; }

void AshWindowTreeHostX11::UpdateDisplayID(int64 id1, int64 id2) {
  display_ids_.first = id1;
  display_ids_.second = id2;
}

void AshWindowTreeHostX11::PrepareForShutdown() {
  if (ui::PlatformEventSource::GetInstance())
    ui::PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
}

void AshWindowTreeHostX11::SetBounds(const gfx::Rect& bounds) {
  WindowTreeHostX11::SetBounds(bounds);
  if (pointer_barriers_) {
    UnConfineCursor();
    ConfineCursorToRootWindow();
  }
}

gfx::Transform AshWindowTreeHostX11::GetRootTransform() const {
  return transformer_helper_.GetTransform();
}

void AshWindowTreeHostX11::SetRootTransform(const gfx::Transform& transform) {
  transformer_helper_.SetTransform(transform);
}

gfx::Transform AshWindowTreeHostX11::GetInverseRootTransform() const {
  return transformer_helper_.GetInverseTransform();
}

void AshWindowTreeHostX11::UpdateRootWindowSize(const gfx::Size& host_size) {
  transformer_helper_.UpdateWindowSize(host_size);
}

void AshWindowTreeHostX11::OnCursorVisibilityChangedNative(bool show) {
#if defined(OS_CHROMEOS)
  SetCrOSTapPaused(!show);
#endif
}

void AshWindowTreeHostX11::OnWindowInitialized(aura::Window* window) {}

void AshWindowTreeHostX11::OnHostInitialized(aura::WindowTreeHost* host) {
  if (host != AsWindowTreeHost())
    return;

#if defined(OS_CHROMEOS)
  // We have to enable Tap-to-click by default because the cursor is set to
  // visible in Shell::InitRootWindowController.
  SetCrOSTapPaused(false);
#endif
}

void AshWindowTreeHostX11::OnConfigureNotify() {
  // Always update barrier and mouse location because |bounds_| might
  // have already been updated in |SetBounds|.
  if (pointer_barriers_) {
    UnConfineCursor();
    ConfineCursorToRootWindow();
  }
}

bool AshWindowTreeHostX11::CanDispatchEvent(const ui::PlatformEvent& event) {
  if(!WindowTreeHostX11::CanDispatchEvent(event))
    return false;
  XEvent* xev = event;
  ui::EventType type = ui::EventTypeFromNative(xev);
  // For touch event, check if the root window is residing on the according
  // touch display.
  switch (type) {
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_RELEASED: {
#if defined(OS_CHROMEOS)
      XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(xev->xcookie.data);
      int64 touch_display_id =
          ui::DeviceDataManager::GetInstance()->GetDisplayForTouchDevice(
              xiev->deviceid);
      // If we don't have record of display id for this touch device, check
      // that if the event is within the bound of the root window. Note
      // that in multi-monitor case, the event position is in framebuffer
      // space so the bounds check will not work so well.
      if (touch_display_id == gfx::Display::kInvalidDisplayID) {
        if (base::SysInfo::IsRunningOnChromeOS() &&
            !bounds().Contains(ui::EventLocationFromNative(xev)))
          return false;
      } else if (touch_display_id != display_ids_.first &&
                 touch_display_id != display_ids_.second) {
        return false;
      }
#endif  // defined(OS_CHROMEOS)
      return true;
    }
    default:
      return true;
  }
}
void AshWindowTreeHostX11::TranslateAndDispatchLocatedEvent(
    ui::LocatedEvent* event) {
  if (!event->IsTouchEvent()) {
    aura::Window* root_window = window();
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window);
    gfx::Rect local(bounds().size());
    local.Inset(transformer_helper_.GetHostInsets());

    if (screen_position_client && !local.Contains(event->location())) {
      gfx::Point location(event->location());
      // In order to get the correct point in screen coordinates
      // during passive grab, we first need to find on which host window
      // the mouse is on, and find out the screen coordinates on that
      // host window, then convert it back to this host window's coordinate.
      screen_position_client->ConvertHostPointToScreen(root_window,
                                                       &location);
      screen_position_client->ConvertPointFromScreen(root_window, &location);
      ConvertPointToHost(&location);
      event->set_location(location);
      event->set_root_location(location);
    }
  }
  SendEventToProcessor(event);
}

#if defined(OS_CHROMEOS)
void AshWindowTreeHostX11::SetCrOSTapPaused(bool state) {
  if (!ui::IsXInput2Available())
    return;
  // Temporarily pause tap-to-click when the cursor is hidden.
  Atom prop = atom_cache()->GetAtom("Tap Paused");
  unsigned char value = state;
  XIDeviceList dev_list =
      ui::DeviceListCacheX::GetInstance()->GetXI2DeviceList(xdisplay());

  // Only slave pointer devices could possibly have tap-paused property.
  for (int i = 0; i < dev_list.count; i++) {
    if (dev_list[i].use == XISlavePointer) {
      Atom old_type;
      int old_format;
      unsigned long old_nvalues, bytes;
      unsigned char* data;
      int result = XIGetProperty(xdisplay(),
                                 dev_list[i].deviceid,
                                 prop,
                                 0,
                                 0,
                                 False,
                                 AnyPropertyType,
                                 &old_type,
                                 &old_format,
                                 &old_nvalues,
                                 &bytes,
                                 &data);
      if (result != Success)
        continue;
      XFree(data);
      XIChangeProperty(xdisplay(),
                       dev_list[i].deviceid,
                       prop,
                       XA_INTEGER,
                       8,
                       PropModeReplace,
                       &value,
                       1);
    }
  }
}
#endif

AshWindowTreeHost* AshWindowTreeHost::Create(
    const AshWindowTreeHostInitParams& init_params) {
  return new AshWindowTreeHostX11(init_params.initial_bounds);
}

}  // namespace ash
