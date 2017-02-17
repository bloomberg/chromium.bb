// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/system/chromeos/audio/tray_audio.h"

#include "ash/common/system/chromeos/audio/audio_detailed_view.h"
#include "ash/common/system/chromeos/audio/tray_audio_delegate_chromeos.h"
#include "ash/common/system/chromeos/audio/volume_view.h"
#include "ash/common/system/tray/system_tray.h"
#include "ash/common/system/tray/tray_constants.h"
#include "ash/common/wm_shell.h"
#include "ash/common/wm_window.h"
#include "ash/root_window_controller.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "ui/display/display.h"
#include "ui/display/manager/managed_display_info.h"
#include "ui/display/screen.h"
#include "ui/views/view.h"

namespace ash {

using chromeos::CrasAudioHandler;
using chromeos::DBusThreadManager;
using system::TrayAudioDelegate;
using system::TrayAudioDelegateChromeOs;

TrayAudio::TrayAudio(SystemTray* system_tray)
    : TrayImageItem(system_tray, kSystemTrayVolumeMuteIcon, UMA_AUDIO),
      audio_delegate_(new TrayAudioDelegateChromeOs()),
      volume_view_(nullptr),
      pop_up_volume_view_(false),
      audio_detail_view_(nullptr) {
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->AddAudioObserver(this);
  display::Screen::GetScreen()->AddObserver(this);
  DBusThreadManager::Get()->GetPowerManagerClient()->AddObserver(this);
}

TrayAudio::~TrayAudio() {
  DBusThreadManager::Get()->GetPowerManagerClient()->RemoveObserver(this);
  display::Screen::GetScreen()->RemoveObserver(this);
  if (CrasAudioHandler::IsInitialized())
    CrasAudioHandler::Get()->RemoveAudioObserver(this);
}

// static
void TrayAudio::ShowPopUpVolumeView() {
  // Show the popup on all monitors with a system tray.
  for (WmWindow* root : WmShell::Get()->GetAllRootWindows()) {
    SystemTray* system_tray = root->GetRootWindowController()->GetSystemTray();
    if (!system_tray)
      continue;
    // Show the popup by simulating a volume change. The provided node id and
    // volume value are ignored.
    system_tray->GetTrayAudio()->OnOutputNodeVolumeChanged(0, 0);
  }
}

bool TrayAudio::GetInitialVisibility() {
  return audio_delegate_->IsOutputAudioMuted();
}

views::View* TrayAudio::CreateDefaultView(LoginStatus status) {
  volume_view_ = new tray::VolumeView(this, audio_delegate_.get(), true);
  return volume_view_;
}

views::View* TrayAudio::CreateDetailedView(LoginStatus status) {
  if (pop_up_volume_view_) {
    volume_view_ = new tray::VolumeView(this, audio_delegate_.get(), false);
    return volume_view_;
  } else {
    WmShell::Get()->RecordUserMetricsAction(
        UMA_STATUS_AREA_DETAILED_AUDIO_VIEW);
    audio_detail_view_ = new tray::AudioDetailedView(this);
    return audio_detail_view_;
  }
}

void TrayAudio::DestroyDefaultView() {
  volume_view_ = NULL;
}

void TrayAudio::DestroyDetailedView() {
  if (audio_detail_view_) {
    audio_detail_view_ = nullptr;
  } else if (volume_view_) {
    volume_view_ = nullptr;
    pop_up_volume_view_ = false;
  }
}

bool TrayAudio::ShouldShowShelf() const {
  return !pop_up_volume_view_;
}

void TrayAudio::OnOutputNodeVolumeChanged(uint64_t /* node_id */,
                                          int /* volume */) {
  float percent =
      static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f;
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }
  pop_up_volume_view_ = true;
  PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

void TrayAudio::OnOutputMuteChanged(bool /* mute_on */, bool system_adjust) {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->Update();
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
  } else if (!system_adjust) {
    pop_up_volume_view_ = true;
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
  }
}

void TrayAudio::OnAudioNodesChanged() {
  Update();
}

void TrayAudio::OnActiveOutputNodeChanged() {
  Update();
}

void TrayAudio::OnActiveInputNodeChanged() {
  Update();
}

void TrayAudio::ChangeInternalSpeakerChannelMode() {
  // Swap left/right channel only if it is in Yoga mode.
  system::TrayAudioDelegate::AudioChannelMode channel_mode =
      system::TrayAudioDelegate::NORMAL;
  if (display::Display::HasInternalDisplay()) {
    const display::ManagedDisplayInfo& display_info =
        WmShell::Get()->GetDisplayInfo(display::Display::InternalDisplayId());
    if (display_info.GetActiveRotation() == display::Display::ROTATE_180)
      channel_mode = system::TrayAudioDelegate::LEFT_RIGHT_SWAPPED;
  }

  audio_delegate_->SetInternalSpeakerChannelMode(channel_mode);
}

void TrayAudio::OnDisplayAdded(const display::Display& new_display) {
  if (!new_display.IsInternal())
    return;
  ChangeInternalSpeakerChannelMode();

  // This event will be triggered when the lid of the device is opened to exit
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void TrayAudio::OnDisplayRemoved(const display::Display& old_display) {
  if (!old_display.IsInternal())
    return;
  ChangeInternalSpeakerChannelMode();

  // This event will be triggered when the lid of the device is closed to enter
  // the docked mode, we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void TrayAudio::OnDisplayMetricsChanged(const display::Display& display,
                                        uint32_t changed_metrics) {
  if (!display.IsInternal())
    return;

  if (changed_metrics & display::DisplayObserver::DISPLAY_METRIC_ROTATION)
    ChangeInternalSpeakerChannelMode();

  // The event could be triggered multiple times during the HDMI display
  // transition, we don't need to restart HDMI re-discovering grace period
  // it is already started earlier.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(false);
}

void TrayAudio::SuspendDone(const base::TimeDelta& sleep_duration) {
  // This event is triggered when the device resumes after earlier suspension,
  // we should always start or re-start HDMI re-discovering
  // grace period right after this event.
  audio_delegate_->SetActiveHDMIOutoutRediscoveringIfNecessary(true);
}

void TrayAudio::Update() {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());
  if (volume_view_) {
    volume_view_->SetVolumeLevel(
        static_cast<float>(audio_delegate_->GetOutputVolumeLevel()) / 100.0f);
    volume_view_->Update();
  }

  if (audio_detail_view_)
    audio_detail_view_->Update();
}

}  // namespace ash
