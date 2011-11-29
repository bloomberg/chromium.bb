// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_key_event_listener.h"

#define XK_MISCELLANY 1
#include <X11/keysymdef.h>
#include <X11/XF86keysym.h>
#include <X11/XKBlib.h>

#include "chrome/browser/accessibility/accessibility_events.h"
#include "chrome/browser/chromeos/audio_handler.h"
#include "chrome/browser/chromeos/brightness_bubble.h"
#include "chrome/browser/chromeos/dbus/dbus_thread_manager.h"
#include "chrome/browser/chromeos/dbus/power_manager_client.h"
#include "chrome/browser/chromeos/input_method/hotkey_manager.h"
#include "chrome/browser/chromeos/input_method/input_method_manager.h"
#include "chrome/browser/chromeos/input_method/xkeyboard.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "content/browser/user_metrics.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/x/x11_util.h"

#if defined(USE_WAYLAND)
#include "base/message_pump_wayland.h"
#elif !defined(TOOLKIT_USES_GTK)
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

// In ProcessedXEvent(), we should check only Alt, Shift, Control, and Caps Lock
// modifiers, and should ignore Num Lock, Super, Hyper etc. See
// http://crosbug.com/21842.
const unsigned int kSupportedModifiers =
    Mod1Mask | ShiftMask | ControlMask | LockMask;

static SystemKeyEventListener* g_system_key_event_listener = NULL;

}  // namespace

// static
void SystemKeyEventListener::Initialize() {
  CHECK(!g_system_key_event_listener);
  g_system_key_event_listener = new SystemKeyEventListener();
}

// static
void SystemKeyEventListener::Shutdown() {
  // We may call Shutdown without calling Initialize, e.g. if we exit early.
  if (g_system_key_event_listener) {
    delete g_system_key_event_listener;
    g_system_key_event_listener = NULL;
  }
}

// static
SystemKeyEventListener* SystemKeyEventListener::GetInstance() {
  VLOG_IF(1, !g_system_key_event_listener)
      << "SystemKeyEventListener::GetInstance() with NULL global instance.";
  return g_system_key_event_listener;
}

SystemKeyEventListener::SystemKeyEventListener()
    : stopped_(false),
      num_lock_mask_(input_method::XKeyboard::GetNumLockMask()),
      xkb_event_base_(0) {
  input_method::XKeyboard::GetLockedModifiers(
      num_lock_mask_, &caps_lock_is_on_, &num_lock_is_on_);

  Display* display = ui::GetXDisplay();
  key_brightness_down_ = XKeysymToKeycode(display,
                                          XF86XK_MonBrightnessDown);
  key_brightness_up_ = XKeysymToKeycode(display, XF86XK_MonBrightnessUp);
  key_volume_mute_ = XKeysymToKeycode(display, XF86XK_AudioMute);
  key_volume_down_ = XKeysymToKeycode(display, XF86XK_AudioLowerVolume);
  key_volume_up_ = XKeysymToKeycode(display, XF86XK_AudioRaiseVolume);
  key_f6_ = XKeysymToKeycode(display, XK_F6);
  key_f7_ = XKeysymToKeycode(display, XK_F7);
  key_f8_ = XKeysymToKeycode(display, XK_F8);
  key_f9_ = XKeysymToKeycode(display, XK_F9);
  key_f10_ = XKeysymToKeycode(display, XK_F10);
  key_left_shift_ = XKeysymToKeycode(display, XK_Shift_L);
  key_right_shift_ = XKeysymToKeycode(display, XK_Shift_R);

  if (key_brightness_down_)
    GrabKey(key_brightness_down_, 0);
  if (key_brightness_up_)
    GrabKey(key_brightness_up_, 0);
  if (key_volume_mute_)
    GrabKey(key_volume_mute_, 0);
  if (key_volume_down_)
    GrabKey(key_volume_down_, 0);
  if (key_volume_up_)
    GrabKey(key_volume_up_, 0);
  GrabKey(key_f6_, 0);
  GrabKey(key_f7_, 0);
  GrabKey(key_f8_, 0);
  GrabKey(key_f9_, 0);
  GrabKey(key_f10_, 0);

  int xkb_major_version = XkbMajorVersion;
  int xkb_minor_version = XkbMinorVersion;
  if (!XkbQueryExtension(display,
                         NULL,  // opcode_return
                         &xkb_event_base_,
                         NULL,  // error_return
                         &xkb_major_version,
                         &xkb_minor_version)) {
    LOG(WARNING) << "Could not query Xkb extension";
  }

  if (!XkbSelectEvents(display, XkbUseCoreKbd,
                       XkbStateNotifyMask,
                       XkbStateNotifyMask)) {
    LOG(WARNING) << "Could not install Xkb Indicator observer";
  }

#if defined(TOOLKIT_USES_GTK)
  gdk_window_add_filter(NULL, GdkEventFilter, this);
#else
  MessageLoopForUI::current()->AddObserver(this);
#endif
}

SystemKeyEventListener::~SystemKeyEventListener() {
  Stop();
}

void SystemKeyEventListener::Stop() {
  if (stopped_)
    return;
#if defined(TOOLKIT_USES_GTK)
  gdk_window_remove_filter(NULL, GdkEventFilter, this);
#else
  MessageLoopForUI::current()->RemoveObserver(this);
#endif
  stopped_ = true;
}

AudioHandler* SystemKeyEventListener::GetAudioHandler() const {
  AudioHandler* audio_handler = AudioHandler::GetInstance();
  if (!audio_handler || !audio_handler->IsInitialized())
    return NULL;
  return audio_handler;
}

void SystemKeyEventListener::AddCapsLockObserver(CapsLockObserver* observer) {
  caps_lock_observers_.AddObserver(observer);
}

void SystemKeyEventListener::RemoveCapsLockObserver(
    CapsLockObserver* observer) {
  caps_lock_observers_.RemoveObserver(observer);
}

#if defined(TOOLKIT_USES_GTK)
// static
GdkFilterReturn SystemKeyEventListener::GdkEventFilter(GdkXEvent* gxevent,
                                                       GdkEvent* gevent,
                                                       gpointer data) {
  SystemKeyEventListener* listener = static_cast<SystemKeyEventListener*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);

  return listener->ProcessedXEvent(xevent) ? GDK_FILTER_REMOVE
                                           : GDK_FILTER_CONTINUE;
}
#else  // defined(TOOLKIT_USES_GTK)
base::EventStatus SystemKeyEventListener::WillProcessEvent(
    const base::NativeEvent& event) {
  return ProcessedXEvent(event) ? base::EVENT_HANDLED : base::EVENT_CONTINUE;
}

void SystemKeyEventListener::DidProcessEvent(const base::NativeEvent& event) {
}
#endif  // defined(TOOLKIT_USES_GTK)

void SystemKeyEventListener::GrabKey(int32 key, uint32 mask) {
  uint32 caps_lock_mask = LockMask;
  Display* display = ui::GetXDisplay();
  Window root = DefaultRootWindow(display);
  XGrabKey(display, key, mask, root, True, GrabModeAsync, GrabModeAsync);
  XGrabKey(display, key, mask | caps_lock_mask, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, key, mask | num_lock_mask_, root, True,
           GrabModeAsync, GrabModeAsync);
  XGrabKey(display, key, mask | caps_lock_mask | num_lock_mask_, root,
           True, GrabModeAsync, GrabModeAsync);
}

void SystemKeyEventListener::OnBrightnessDown() {
  DBusThreadManager::Get()->GetPowerManagerClient()->
      DecreaseScreenBrightness(true);
}

void SystemKeyEventListener::OnBrightnessUp() {
  DBusThreadManager::Get()->GetPowerManagerClient()->
      IncreaseScreenBrightness();
}

void SystemKeyEventListener::OnVolumeMute() {
  AudioHandler* audio_handler = GetAudioHandler();
  if (!audio_handler)
    return;

  // Always muting (and not toggling) as per final decision on
  // http://crosbug.com/3751
  audio_handler->SetMuted(true);

  SendAccessibilityVolumeNotification(
      audio_handler->GetVolumePercent(),
      audio_handler->IsMuted());

  ShowVolumeBubble();
}

void SystemKeyEventListener::OnVolumeDown() {
  AudioHandler* audio_handler = GetAudioHandler();
  if (!audio_handler)
    return;

  if (audio_handler->IsMuted())
    audio_handler->SetVolumePercent(0.0);
  else
    audio_handler->AdjustVolumeByPercent(-kStepPercentage);

  SendAccessibilityVolumeNotification(
      audio_handler->GetVolumePercent(),
      audio_handler->IsMuted());

  ShowVolumeBubble();
}

void SystemKeyEventListener::OnVolumeUp() {
  AudioHandler* audio_handler = GetAudioHandler();
  if (!audio_handler)
    return;

  if (audio_handler->IsMuted()) {
    audio_handler->SetMuted(false);
    if (audio_handler->GetVolumePercent() <= 0.1)  // float comparison
      audio_handler->SetVolumePercent(kVolumePercentOnVolumeUpWhileMuted);
  } else {
    audio_handler->AdjustVolumeByPercent(kStepPercentage);
  }

  SendAccessibilityVolumeNotification(
      audio_handler->GetVolumePercent(),
      audio_handler->IsMuted());

  ShowVolumeBubble();
}

void SystemKeyEventListener::OnCapsLock(bool enabled) {
  FOR_EACH_OBSERVER(
      CapsLockObserver, caps_lock_observers_, OnCapsLockChange(enabled));
}

void SystemKeyEventListener::ShowVolumeBubble() {
  AudioHandler* audio_handler = GetAudioHandler();
  if (audio_handler) {
    VolumeBubble::GetInstance()->ShowBubble(
        audio_handler->GetVolumePercent(),
        !audio_handler->IsMuted());
  }
  BrightnessBubble::GetInstance()->HideBubble();
}

bool SystemKeyEventListener::ProcessedXEvent(XEvent* xevent) {
  input_method::InputMethodManager* input_method_manager =
      input_method::InputMethodManager::GetInstance();

  if (xevent->type == KeyPress || xevent->type == KeyRelease) {
    // Change the current keyboard layout (or input method) if xevent is one of
    // the input method hotkeys.
    input_method::HotkeyManager* hotkey_manager =
        input_method_manager->GetHotkeyManager();
    if (hotkey_manager->FilterKeyEvent(*xevent)) {
      return true;
    }
  }

  if (xevent->type == xkb_event_base_) {
    XkbEvent* xkey_event = reinterpret_cast<XkbEvent*>(xevent);
    if (xkey_event->any.xkb_type == XkbStateNotify) {
      input_method::ModifierLockStatus new_caps_lock_state =
          input_method::kDontChange;
      input_method::ModifierLockStatus new_num_lock_state =
          input_method::kDontChange;

      bool enabled = (xkey_event->state.locked_mods) & LockMask;
      if (caps_lock_is_on_ != enabled) {
        caps_lock_is_on_ = enabled;
        new_caps_lock_state =
            enabled ? input_method::kEnableLock : input_method::kDisableLock;
        OnCapsLock(caps_lock_is_on_);
      }

      enabled = (xkey_event->state.locked_mods) & num_lock_mask_;
      if (num_lock_is_on_ != enabled) {
        num_lock_is_on_ = enabled;
        new_num_lock_state =
            enabled ? input_method::kEnableLock : input_method::kDisableLock;
      }

      // Propagate the keyboard LED change to _ALL_ keyboards
      input_method_manager->GetXKeyboard()->SetLockedModifiers(
          new_caps_lock_state, new_num_lock_state);

      return true;
    }
  } else if (xevent->type == KeyPress) {
    const int32 keycode = xevent->xkey.keycode;
    if (keycode) {
      const unsigned int state = (xevent->xkey.state & kSupportedModifiers);

      // Toggle Caps Lock if both Shift keys are pressed simultaneously.
      if (keycode == key_left_shift_ || keycode == key_right_shift_) {
        const bool other_shift_is_held = (state & ShiftMask);
        const bool other_mods_are_held = (state & ~(ShiftMask | LockMask));
        if (other_shift_is_held && !other_mods_are_held)
          input_method_manager->GetXKeyboard()->SetCapsLockEnabled(
              !caps_lock_is_on_);
      }

      // Only doing non-Alt/Shift/Ctrl modified keys
      if (!(state & (Mod1Mask | ShiftMask | ControlMask))) {
        if (keycode == key_f6_ || keycode == key_brightness_down_) {
          if (keycode == key_f6_)
            UserMetrics::RecordAction(
                UserMetricsAction("Accel_BrightnessDown_F6"));
          OnBrightnessDown();
          return true;
        } else if (keycode == key_f7_ || keycode == key_brightness_up_) {
          if (keycode == key_f7_)
            UserMetrics::RecordAction(
                UserMetricsAction("Accel_BrightnessUp_F7"));
          OnBrightnessUp();
          return true;
        } else if (keycode == key_f8_ || keycode == key_volume_mute_) {
          if (keycode == key_f8_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));
          OnVolumeMute();
          return true;
        } else if (keycode == key_f9_ || keycode == key_volume_down_) {
          if (keycode == key_f9_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));
          OnVolumeDown();
          return true;
        } else if (keycode == key_f10_ || keycode == key_volume_up_) {
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
