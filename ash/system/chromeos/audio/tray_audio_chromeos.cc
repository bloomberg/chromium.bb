// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/chromeos/audio/tray_audio_chromeos.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/system/audio/volume_view.h"
#include "ash/system/chromeos/audio/audio_detailed_view.h"
#include "ash/system/chromeos/audio/tray_audio_delegate_chromeos.h"
#include "ui/views/view.h"

namespace ash {

using system::TrayAudioDelegate;
using system::TrayAudioDelegateChromeOs;

TrayAudioChromeOs::TrayAudioChromeOs(SystemTray* system_tray)
    : TrayAudio(system_tray,
                scoped_ptr<TrayAudioDelegate>(new TrayAudioDelegateChromeOs())),
      audio_detail_view_(NULL) {
}

TrayAudioChromeOs::~TrayAudioChromeOs() {
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
    audio_detail_view_ =
        new tray::AudioDetailedView(this, status);
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

}  // namespace ash
