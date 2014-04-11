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

#include "ash/host/root_window_transformer.h"
#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/sys_info.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event.h"
#include "ui/events/event_switches.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_observer.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/events/x/device_data_manager.h"
#include "ui/events/x/device_list_cache_x.h"
#include "ui/events/x/touch_factory_x11.h"

#include "ui/gfx/rect.h"
#include "ui/gfx/screen.h"

namespace ash {

// Accomplishes 2 tasks concerning touch event calibration:
// 1. Being a message-pump observer,
//    routes all the touch events to the X root window,
//    where they can be calibrated later.
// 2. Has the Calibrate method that does the actual bezel calibration,
//    when invoked from X root window's event dispatcher.
class AshWindowTreeHostX11::TouchEventCalibrate
    : public ui::PlatformEventObserver {
 public:
  TouchEventCalibrate() : left_(0), right_(0), top_(0), bottom_(0) {
    if (ui::PlatformEventSource::GetInstance())
      ui::PlatformEventSource::GetInstance()->AddPlatformEventObserver(this);
#if defined(USE_XI2_MT)
    std::vector<std::string> parts;
    if (Tokenize(CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
                     switches::kTouchCalibration),
                 ",",
                 &parts) >= 4) {
      if (!base::StringToInt(parts[0], &left_))
        DLOG(ERROR) << "Incorrect left border calibration value passed.";
      if (!base::StringToInt(parts[1], &right_))
        DLOG(ERROR) << "Incorrect right border calibration value passed.";
      if (!base::StringToInt(parts[2], &top_))
        DLOG(ERROR) << "Incorrect top border calibration value passed.";
      if (!base::StringToInt(parts[3], &bottom_))
        DLOG(ERROR) << "Incorrect bottom border calibration value passed.";
    }
#endif  // defined(USE_XI2_MT)
  }

  virtual ~TouchEventCalibrate() {
    if (ui::PlatformEventSource::GetInstance())
      ui::PlatformEventSource::GetInstance()->RemovePlatformEventObserver(this);
  }

  // Modify the location of the |event|,
  // expanding it from |bounds| to (|bounds| + bezels).
  // Required when touchscreen is bigger than screen (i.e. has bezels),
  // because we receive events in touchscreen coordinates,
  // which need to be expanded when converting to screen coordinates,
  // so that location on bezels will be outside of screen area.
  void Calibrate(ui::TouchEvent* event, const gfx::Rect& bounds) {
#if defined(USE_XI2_MT)
    int x = event->x();
    int y = event->y();

    if (!left_ && !right_ && !top_ && !bottom_)
      return;

    const int resolution_x = bounds.width();
    const int resolution_y = bounds.height();
    // The "grace area" (10% in this case) is to make it easier for the user to
    // navigate to the corner.
    const double kGraceAreaFraction = 0.1;
    if (left_ || right_) {
      // Offset the x position to the real
      x -= left_;
      // Check if we are in the grace area of the left side.
      // Note: We might not want to do this when the gesture is locked?
      if (x < 0 && x > -left_ * kGraceAreaFraction)
        x = 0;
      // Check if we are in the grace area of the right side.
      // Note: We might not want to do this when the gesture is locked?
      if (x > resolution_x - left_ &&
          x < resolution_x - left_ + right_ * kGraceAreaFraction)
        x = resolution_x - left_;
      // Scale the screen area back to the full resolution of the screen.
      x = (x * resolution_x) / (resolution_x - (right_ + left_));
    }
    if (top_ || bottom_) {
      // When there is a top bezel we add our border,
      y -= top_;

      // Check if we are in the grace area of the top side.
      // Note: We might not want to do this when the gesture is locked?
      if (y < 0 && y > -top_ * kGraceAreaFraction)
        y = 0;

      // Check if we are in the grace area of the bottom side.
      // Note: We might not want to do this when the gesture is locked?
      if (y > resolution_y - top_ &&
          y < resolution_y - top_ + bottom_ * kGraceAreaFraction)
        y = resolution_y - top_;
      // Scale the screen area back to the full resolution of the screen.
      y = (y * resolution_y) / (resolution_y - (bottom_ + top_));
    }

    // Set the modified coordinate back to the event.
    if (event->root_location() == event->location()) {
      // Usually those will be equal,
      // if not, I am not sure what the correct value should be.
      event->set_root_location(gfx::Point(x, y));
    }
    event->set_location(gfx::Point(x, y));
#endif  // defined(USE_XI2_MT)
  }

 private:
  // ui::PlatformEventObserver:
  virtual void WillProcessEvent(const ui::PlatformEvent& event) OVERRIDE {
#if defined(USE_XI2_MT)
    if (event->type == GenericEvent &&
        (event->xgeneric.evtype == XI_TouchBegin ||
         event->xgeneric.evtype == XI_TouchUpdate ||
         event->xgeneric.evtype == XI_TouchEnd)) {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(event->xcookie.data);
      xievent->event = xievent->root;
      xievent->event_x = xievent->root_x;
      xievent->event_y = xievent->root_y;
    }
#endif  // defined(USE_XI2_MT)
  }

  virtual void DidProcessEvent(const ui::PlatformEvent& event) OVERRIDE {}

  // The difference in screen's native resolution pixels between
  // the border of the touchscreen and the border of the screen,
  // aka bezel sizes.
  int left_;
  int right_;
  int top_;
  int bottom_;

  DISALLOW_COPY_AND_ASSIGN(TouchEventCalibrate);
};

////////////////////////////////////////////////////////////////////////////////
// AshWindowTreeHostX11

AshWindowTreeHostX11::AshWindowTreeHostX11(const gfx::Rect& initial_bounds)
    : WindowTreeHostX11(initial_bounds),
      is_internal_display_(false),
      touch_calibrate_(new TouchEventCalibrate),
      transformer_helper_(this) {
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
  UpdateIsInternalDisplay();
  if (pointer_barriers_) {
    UnConfineCursor();
    ConfineCursorToRootWindow();
  }
}

aura::WindowTreeHost* AshWindowTreeHostX11::AsWindowTreeHost() { return this; }

void AshWindowTreeHostX11::SetBounds(const gfx::Rect& bounds) {
  WindowTreeHostX11::SetBounds(bounds);
  UpdateIsInternalDisplay();
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
  SetCrOSTapPaused(!show);
}

void AshWindowTreeHostX11::OnWindowInitialized(aura::Window* window) {}

void AshWindowTreeHostX11::OnHostInitialized(aura::WindowTreeHost* host) {
  // UpdateIsInternalDisplay relies on RootWindowSettings' display_id being set
  // available by the time WED::Init is called. (set in
  // DisplayManager::CreateRootWindowForDisplay)
  // Ready when NotifyHostInitialized is called from WED::Init.
  if (host != AsWindowTreeHost())
    return;
  UpdateIsInternalDisplay();

  // We have to enable Tap-to-click by default because the cursor is set to
  // visible in Shell::InitRootWindowController.
  SetCrOSTapPaused(false);
}

void AshWindowTreeHostX11::OnConfigureNotify() {
  UpdateIsInternalDisplay();

  // Always update barrier and mouse location because |bounds_| might
  // have already been updated in |SetBounds|.
  if (pointer_barriers_) {
    UnConfineCursor();
    ConfineCursorToRootWindow();
  }
}

void AshWindowTreeHostX11::TranslateAndDispatchLocatedEvent(
    ui::LocatedEvent* event) {
  switch (event->type()) {
    case ui::ET_TOUCH_MOVED:
    case ui::ET_TOUCH_PRESSED:
    case ui::ET_TOUCH_CANCELLED:
    case ui::ET_TOUCH_RELEASED: {
      ui::TouchEvent* touchev = static_cast<ui::TouchEvent*>(event);
      if (base::SysInfo::IsRunningOnChromeOS()) {
        // X maps the touch-surface to the size of the X root-window.
        // In multi-monitor setup, Coordinate Transformation Matrix
        // repositions the touch-surface onto part of X root-window
        // containing aura root-window corresponding to the touchscreen.
        // However, if aura root-window has non-zero origin,
        // we need to relocate the event into aura root-window coordinates.
        touchev->Relocate(bounds().origin());
#if defined(USE_XI2_MT)
        if (is_internal_display_)
          touch_calibrate_->Calibrate(touchev, bounds());
#endif  // defined(USE_XI2_MT)
      }
      break;
    }
    default: {
      aura::Window* root_window = window();
      aura::client::ScreenPositionClient* screen_position_client =
          aura::client::GetScreenPositionClient(root_window);
      gfx::Rect local(bounds().size());

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
      break;
    }
  }
  SendEventToProcessor(event);
}

void AshWindowTreeHostX11::UpdateIsInternalDisplay() {
  aura::Window* root_window = window();
  gfx::Screen* screen = gfx::Screen::GetScreenFor(root_window);
  gfx::Display display = screen->GetDisplayNearestWindow(root_window);
  DCHECK(display.is_valid());
  is_internal_display_ = display.IsInternal();
}

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

AshWindowTreeHost* AshWindowTreeHost::Create(const gfx::Rect& initial_bounds) {
  return new AshWindowTreeHostX11(initial_bounds);
}

}  // namespace ash
