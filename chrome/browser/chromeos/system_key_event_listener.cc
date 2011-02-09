// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_key_event_listener.h"

#include <gdk/gdkx.h>
#include <X11/XF86keysym.h>

#include "chrome/browser/chromeos/audio_handler.h"
#include "chrome/browser/chromeos/brightness_bubble.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"

namespace chromeos {

namespace {

const double kStepPercentage = 4.0;

}  // namespace

// static
SystemKeyEventListener* SystemKeyEventListener::GetInstance() {
  return Singleton<SystemKeyEventListener>::get();
}

SystemKeyEventListener::SystemKeyEventListener()
    : stopped_(false),
      audio_handler_(AudioHandler::GetInstance()) {
  WmMessageListener::GetInstance()->AddObserver(this);

  key_volume_mute_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_AudioMute);
  key_volume_down_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_AudioLowerVolume);
  key_volume_up_ = XKeysymToKeycode(GDK_DISPLAY(), XF86XK_AudioRaiseVolume);
  key_f8_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F8);
  key_f9_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F9);
  key_f10_ = XKeysymToKeycode(GDK_DISPLAY(), XK_F10);

  if (key_volume_mute_)
    GrabKey(key_volume_mute_, 0);
  if (key_volume_down_)
    GrabKey(key_volume_down_, 0);
  if (key_volume_up_)
    GrabKey(key_volume_up_, 0);
  GrabKey(key_f8_, 0);
  GrabKey(key_f9_, 0);
  GrabKey(key_f10_, 0);
  gdk_window_add_filter(NULL, GdkEventFilter, this);
}

SystemKeyEventListener::~SystemKeyEventListener() {
  Stop();
}

void SystemKeyEventListener::Stop() {
  if (stopped_)
    return;
  WmMessageListener::GetInstance()->RemoveObserver(this);
  gdk_window_remove_filter(NULL, GdkEventFilter, this);
  audio_handler_->Disconnect();
  stopped_ = true;
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

// static
GdkFilterReturn SystemKeyEventListener::GdkEventFilter(GdkXEvent* gxevent,
                                                       GdkEvent* gevent,
                                                       gpointer data) {
  SystemKeyEventListener* listener = static_cast<SystemKeyEventListener*>(data);
  XEvent* xevent = static_cast<XEvent*>(gxevent);

  if (xevent->type == KeyPress) {
    int32 keycode = xevent->xkey.keycode;
    if (keycode) {
      // Only doing non-Alt/Shift/Ctrl modified keys
      if (!(xevent->xkey.state & (Mod1Mask | ShiftMask | ControlMask))) {
        if ((keycode == listener->key_f8_) ||
            (keycode == listener->key_volume_mute_)) {
          if (keycode == listener->key_f8_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeMute_F8"));
          listener->OnVolumeMute();
          return GDK_FILTER_REMOVE;
        } else if ((keycode == listener->key_f9_) ||
                    keycode == listener->key_volume_down_) {
          if (keycode == listener->key_f9_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeDown_F9"));
          listener->OnVolumeDown();
          return GDK_FILTER_REMOVE;
        } else if ((keycode == listener->key_f10_) ||
                   (keycode == listener->key_volume_up_)) {
          if (keycode == listener->key_f10_)
            UserMetrics::RecordAction(UserMetricsAction("Accel_VolumeUp_F10"));
          listener->OnVolumeUp();
          return GDK_FILTER_REMOVE;
        }
      }
    }
  }
  return GDK_FILTER_CONTINUE;
}

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

// TODO(davej): Move the ShowBubble() calls in to AudioHandler so that this
// function returns faster without blocking on GetVolumePercent(), and still
// guarantees that the volume displayed will be that after the adjustment.

// TODO(davej): The IsMute() check can also be made non-blocking by changing to
// an AdjustVolumeByPercentOrUnmute() function which can do the steps off of
// this thread when ShowBubble() is moved in to AudioHandler.

void SystemKeyEventListener::OnVolumeMute() {
  // Always muting (and not toggling) as per final decision on
  // http://crosbug.com/3751
  audio_handler_->SetMute(true);
  VolumeBubble::GetInstance()->ShowBubble(0);
  BrightnessBubble::GetInstance()->HideBubble();
}

void SystemKeyEventListener::OnVolumeDown() {
  if (audio_handler_->IsMute()) {
    VolumeBubble::GetInstance()->ShowBubble(0);
  } else {
    audio_handler_->AdjustVolumeByPercent(-kStepPercentage);
    VolumeBubble::GetInstance()->ShowBubble(
        audio_handler_->GetVolumePercent());
  }
  BrightnessBubble::GetInstance()->HideBubble();
}

void SystemKeyEventListener::OnVolumeUp() {
  if (audio_handler_->IsMute())
    audio_handler_->SetMute(false);
  else
    audio_handler_->AdjustVolumeByPercent(kStepPercentage);
  VolumeBubble::GetInstance()->ShowBubble(
      audio_handler_->GetVolumePercent());
  BrightnessBubble::GetInstance()->HideBubble();
}

}  // namespace chromeos
