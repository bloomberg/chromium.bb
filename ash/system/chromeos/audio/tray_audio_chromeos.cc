// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/audio/tray_audio_chromeos.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/system/audio/volume_view.h"
#include "ash/system/chromeos/audio/audio_detailed_view.h"
#include "ash/system/chromeos/audio/tray_audio_delegate_chromeos.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/views/view.h"

namespace ash {

using system::TrayAudioDelegate;
using system::TrayAudioDelegateChromeOs;

TrayAudioChromeOs::TrayAudioChromeOs(SystemTray* system_tray)
    : TrayAudio(
          system_tray,
          std::unique_ptr<TrayAudioDelegate>(new TrayAudioDelegateChromeOs())),
      audio_detail_view_(NULL) {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(
      this);
}

TrayAudioChromeOs::~TrayAudioChromeOs() {
  chromeos::DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(
      this);
}

void TrayAudioChromeOs::Update() {
  TrayAudio::Update();

  if (audio_detail_view_)
    audio_detail_view_->Update();
}

views::View* TrayAudioChromeOs::CreateDetailedView(user::LoginStatus status) {
  if (pop_up_volume_view_) {
    volume_view_ = new tray::VolumeView(this, audio_delegate_.get(), false);
    return volume_view_;
  } else {
    Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_STATUS_AREA_DETAILED_AUDIO_VIEW);
    audio_detail_view_ = new tray::AudioDetailedView(this);
    return audio_detail_view_;
  }
}

void TrayAudioChromeOs::DestroyDetailedView() {
  if (audio_detail_view_) {
    audio_detail_view_ = NULL;
  } else if (volume_view_) {
    volume_view_ = NULL;
    pop_up_volume_view_ = false;
  }
}

void TrayAudioChromeOs::OnDisplayAdded(const display::Display& new_display) {
  TrayAudio::OnDisplayAdded(new_display);

  // This event will be triggered when the lid of the device is opened to exit
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void TrayAudioChromeOs::OnDisplayRemoved(const display::Display& old_display) {
  TrayAudio::OnDisplayRemoved(old_display);

  // This event will be triggered when the lid of the device is closed to enter
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void TrayAudioChromeOs::OnDisplayMetricsChanged(const display::Display& display,
                                                uint32_t changed_metrics) {
  // The event could be triggered multiple times during the HDMI display
  // transition, we don't need to restart HDMI re-discovering grace period
  // it is already started earlier.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(false);
}

void TrayAudioChromeOs::SuspendDone(const base::TimeDelta& sleep_duration) {
  // This event is triggered when the device resumes after earlier suspension,
  // we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

}  // namespace ash
