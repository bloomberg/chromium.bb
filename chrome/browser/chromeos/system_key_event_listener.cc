// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_key_event_listener.h"

#include <gdk/gdkx.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>

#include "chrome/browser/accessibility_events.h"
#include "chrome/browser/chromeos/audio_handler.h"
#include "chrome/browser/chromeos/brightness_bubble.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "content/browser/user_metrics.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"

#if defined(TOUCH_UI)
#include "base/message_pump_x.h"
#endif

namespace chromeos {

namespace {

// Percent by which the volume should be changed when a volume key is pressed.
const double kStepPercentage = 4.0;

// Percent to which the volume should be set when the "volume up" key is pressed
// while we're muted and have the volume set to 0.  See
// http://crosbug.com/13618.
const double kVolumePercentOnVolumeUpWhileMuted = 25.0;

}  // namespace

// static
SystemKeyEventListener* SystemKeyEventListener::GetInstance() {
  return Singleton<SystemKeyEventListener>::get();
}

SystemKeyEventListener::SystemKeyEventListener()
    : stopped_(false),
      waiting_for_shift_for_caps_lock_(false),
      caps_lock_is_on_(input_method::CapsLockIsEnabled()),
      xkb_event_base_(0),
      audio_handler_(AudioHandler::GetInstance()) {
  WmMessageListener::GetInstance()->AddObserver(this);

  key_volume_mute_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_AudioMute);
  key_volume_down_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_AudioLowerVolume);
  key_volume_up_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_AudioRaiseVolume);
  key_f8_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F8);
  key_f9_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F9);
  key_f10_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F10);
  key_left_shift_ = XKeysymToKeycode(GDK_DISPLAY(), XK_Shift_L);
  key_right_shift_ = XKeysymToKeycode(GDK_DISPLAY(), XK_Shift_R);

  if (key_volume_mute_)
    GrabKey(key_volume_mute_, 0);
  if (key_volume_down_)
    GrabKey(key_volume_down_, 0);
  if (key_volume_up_)
    GrabKey(key_volume_up_, 0);
  GrabKey(key_f8_, 0);
  GrabKey(key_f9_, 0);
  GrabKey(key_f10_, 0);

  int xkb_major_version = XkbMajorVersion;
  int xkb_minor_version = XkbMinorVersion;
  if (!XkbQueryExtension(GDK_DISPLAY(),
                         NULL,  // opcode_return
                         &xkb_event_base_,
                         NULL,  // error_return
                         &xkb_major_version,
                         &xkb_minor_version)) {
    LOG(WARNING) << "Could not query Xkb extension";
  }

  if (!XkbSelectEvents(GDK_DISPLAY(), XkbUseCoreKbd,
                       XkbStateNotifyMask,
                       XkbStateNotifyMask)) {
    LOG(WARNING) << "Could not install Xkb Indicator observer";
  }

#if defined(TOUCH_UI)
  MessageLoopForUI::current()->AddObserver(this);
#else
  gdk_window_add_filter(NULL, GdkEventFilter, this);
#endif
}

SystemKeyEventListener::~SystemKeyEventListener() {
  Stop();
}

void SystemKeyEventListener::Stop() {
  if (stopped_)
    return;
  WmMessageListener::GetInstance()->RemoveObserver(this);
#if defined(TOUCH_UI)
  MessageLoopForUI::current()->RemoveObserver(this);
#else
  gdk_window_remove_filter(NULL, GdkEventFilter, this);
#endif
  stopped_ = true;
}

void SystemKeyEventListener::AddCapsLockObserver(CapsLockObserver* observer) {
  caps_lock_observers_.AddObserver(observer);
}

void SystemKeyEventListener::RemoveCapsLockObserver(
    CapsLockObserver* observer) {
  caps_lock_observers_.RemoveObserver(observer);
}

void SystemKeyEventListener::ProcessWmMessage(const WmIpc::Message& message,
                                              GdkWindow* window) {
  if (message.type() != WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED)
    return;

  switch (message.param(0)) {
    case WM_IPC_SYSTEM_KEY_VOLUME_MUTE:
      OnVolumeMute();
      break;
    case WM_IPC_SYSTEM_KEY_VOLUME_DOWN:
      OnVolumeDown();
      break;
    case WM_IPC_SYSTEM_KEY_VOLUME_UP:
      OnVolumeUp();
      break;
    default:
      DLOG(ERROR) << "SystemKeyEventListener: Unexpected message "
                  << message.param(0)
                  << " received";
  }
}

#if defined(TOUCH_UI)
base::MessagePumpObserver::EventStatus
    SystemKeyEventListener::WillProcessXEvent(XEvent* xevent) {
  return ProcessedXEvent(xevent) ? EVENT_HANDLED : EVENT_CONTINUE;
}
#else  // defined(TOUCH_UI)
// static
GdkFilterReturn SystemKeyEventListener::GdkEventFilter(GdkXEvent* gxevent,
                                                       GdkEvent* gevent,
                                                       gpointer data) {
  SystemKeyEventListener* listener = static_cast<SystemKeyEventListener*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);

  return listener->ProcessedXEvent(xevent) ? GDK_FILTER_REMOVE
                                           : GDK_FILTER_CONTINUE;
}
#endif  // defined(TOUCH_UI)

void SystemKeyEventListener::GrabKey(int32 key, uint32 mask) {
  uint32 num_lock_mask = Mod2Mask;
  uint32 caps_lock_mask = LockMask;
  Window root = DefaultRootWindow(GDK_DISPLAY());
  XGrabKey(GDK_DISPLAY(), key, mask, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | caps_lock_mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | num_lock_mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(GDK_DISPLAY(), key, mask | caps_lock_mask | num_lock_mask, root,
           True, GrabModeAsync, GrabModeAsync);
}

void SystemKeyEventListener::OnVolumeMute() {
  if (!audio_handler_->IsInitialized())
    return;

  // Always muting (and not toggling) as per final decision on
  // http://crosbug.com/3751
  audio_handler_->SetMuted(true);

  SendAccessibilityVolumeNotification(
      audio_handler_->GetVolumePercent(),
      audio_handler_->IsMuted());

  ShowVolumeBubble();
}

void SystemKeyEventListener::OnVolumeDown() {
  if (!audio_handler_->IsInitialized())
    return;

  if (audio_handler_->IsMuted())
    audio_handler_->SetVolumePercent(0.0);
  else
    audio_handler_->AdjustVolumeByPercent(-kStepPercentage);

  SendAccessibilityVolumeNotification(
      audio_handler_->GetVolumePercent(),
      audio_handler_->IsMuted());

  ShowVolumeBubble();
}

void SystemKeyEventListener::OnVolumeUp() {
  if (!audio_handler_->IsInitialized())
    return;

  if (audio_handler_->IsMuted()) {
    audio_handler_->SetMuted(false);
    if (audio_handler_->GetVolumePercent() <= 0.1)  // float comparison
      audio_handler_->SetVolumePercent(kVolumePercentOnVolumeUpWhileMuted);
  } else {
    audio_handler_->AdjustVolumeByPercent(kStepPercentage);
  }

  SendAccessibilityVolumeNotification(
      audio_handler_->GetVolumePercent(),
      audio_handler_->IsMuted());

  ShowVolumeBubble();
}

void SystemKeyEventListener::OnCapsLock(bool enabled) {
  FOR_EACH_OBSERVER(
      CapsLockObserver, caps_lock_observers_, OnCapsLockChange(enabled));
}

void SystemKeyEventListener::ShowVolumeBubble() {
  VolumeBubble::GetInstance()->ShowBubble(
      audio_handler_->GetVolumePercent(),
      !audio_handler_->IsMuted());
  BrightnessBubble::GetInstance()->HideBubble();
}

bool SystemKeyEventListener::ProcessedXEvent(XEvent* xevent) {
  if (xevent->type == xkb_event_base_) {
    XkbEvent* xkey_event = reinterpret_cast<XkbEvent*>(xevent);
    if (xkey_event->any.xkb_type == XkbStateNotify) {
      caps_lock_is_on_ = (xkey_event->state.locked_mods) & LockMask;
      OnCapsLock(caps_lock_is_on_);
      return true;
    }
  } else if (xevent->type == KeyPress) {
    const int32 keycode = xevent->xkey.keycode;
    if (keycode) {
      // Toggle Caps Lock if both Shift keys are pressed simultaneously.
      if (keycode == key_left_shift_ || keycode == key_right_shift_) {
        const bool other_shift_is_held = (xevent->xkey.state & ShiftMask);
        const bool other_mods_are_held =
            (xevent->xkey.state & ~(ShiftMask | LockMask));

        if (waiting_for_shift_for_caps_lock_ &&
            other_shift_is_held && !other_mods_are_held) {
          input_method::SetCapsLockEnabled(!caps_lock_is_on_);
        }

        // Only toggle on the next Shift press if we're seeing the first Shift
        // key get pressed by itself here.
        waiting_for_shift_for_caps_lock_ =
            (!other_shift_is_held && !other_mods_are_held);
      } else {
        // If we see a non-Shift key get pressed, start over.
        waiting_for_shift_for_caps_lock_ = false;
      }

      // Only doing non-Alt/Shift/Ctrl modified keys
      if (!(xevent->xkey.state & (Mod1Mask | ShiftMask | ControlMask))) {
        if ((keycode == key_f8_) ||
            (keycode == key_volume_mute_)) {
          if (keycode == key_f8_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));
          OnVolumeMute();
          return true;
        } else if ((keycode == key_f9_) ||
                    keycode == key_volume_down_) {
          if (keycode == key_f9_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));
          OnVolumeDown();
          return true;
        } else if ((keycode == key_f10_) ||
                   (keycode == key_volume_up_)) {
          if (keycode == key_f10_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));
          OnVolumeUp();
          return true;
        }
      }
    }
  }
  return false;
}

}  // namespace chromeos
