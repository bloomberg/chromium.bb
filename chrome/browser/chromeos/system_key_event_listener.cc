// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system_key_event_listener.h"

#include "chrome/browser/chromeos/audio_handler.h"
#include "chrome/browser/chromeos/volume_bubble.h"
#include "cros/chromeos_wm_ipc_enums.h"

namespace chromeos {

namespace {

const double kStepPercentage = 4.0;

}  // namespace

// static
SystemKeyEventListener* SystemKeyEventListener::instance() {
  return Singleton<SystemKeyEventListener>::get();
}

SystemKeyEventListener::SystemKeyEventListener()
    : audio_handler_(AudioHandler::instance()) {
  WmMessageListener::instance()->AddObserver(this);
}

SystemKeyEventListener::~SystemKeyEventListener() {
  WmMessageListener::instance()->RemoveObserver(this);
}

// TODO(davej): Move the ShowVolumeBubble() calls in to AudioHandler so that
// this function returns faster without blocking on GetVolumePercent(), and
// still guarantees that the volume displayed will be that after the adjustment.

// TODO(davej): The IsMute() check can also be made non-blocking by changing
// to an AdjustVolumeByPercentOrUnmute() function which can do the steps off
// of this thread when ShowVolumeBubble() is moved in to AudioHandler.

void SystemKeyEventListener::ProcessWmMessage(const WmIpc::Message& message,
                                              GdkWindow* window) {
  if (message.type() != WM_IPC_MESSAGE_CHROME_NOTIFY_SYSKEY_PRESSED)
    return;

  switch (message.param(0)) {
    case WM_IPC_SYSTEM_KEY_VOLUME_MUTE:
      // Always muting (and not toggling) as per final decision on
      // http://crosbug.com/3751
      audio_handler_->SetMute(true);
      VolumeBubble::instance()->ShowVolumeBubble(0);
      break;
    case WM_IPC_SYSTEM_KEY_VOLUME_DOWN:
      if (audio_handler_->IsMute()) {
        VolumeBubble::instance()->ShowVolumeBubble(0);
      } else {
        audio_handler_->AdjustVolumeByPercent(-kStepPercentage);
        VolumeBubble::instance()->ShowVolumeBubble(
            audio_handler_->GetVolumePercent());
      }
      break;
    case WM_IPC_SYSTEM_KEY_VOLUME_UP:
      if (audio_handler_->IsMute())
        audio_handler_->SetMute(false);
      else
        audio_handler_->AdjustVolumeByPercent(kStepPercentage);
      VolumeBubble::instance()->ShowVolumeBubble(
          audio_handler_->GetVolumePercent());
      break;
    default:
      DLOG(ERROR) << "SystemKeyEventListener: Unexpected message "
                  << message.param(0)
                  << " received";
  }
}

}  // namespace chromeos
