// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_H_
#define ASH_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_H_

#include "ash/system/chromeos/audio/audio_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "chromeos/audio/cras_audio_handler.h"

namespace ash {
namespace internal {

namespace tray {
class VolumeView;
class AudioDetailedView;
}

class TrayAudio : public TrayImageItem,
                  public chromeos::CrasAudioHandler::AudioObserver,
                  public AudioObserver {
 public:
  explicit TrayAudio(SystemTray* system_tray);
  virtual ~TrayAudio();

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual bool ShouldHideArrow() const OVERRIDE;
  virtual bool ShouldShowLauncher() const OVERRIDE;

  // Overridden from AudioObserver.
  virtual void OnVolumeChanged(float percent) OVERRIDE;
  virtual void OnMuteToggled() OVERRIDE;

  // Overridden from chromeos::CrasAudioHandler::AudioObserver.
  virtual void OnOutputVolumeChanged() OVERRIDE;
  virtual void OnOutputMuteChanged() OVERRIDE;
  virtual void OnAudioNodesChanged() OVERRIDE;
  virtual void OnActiveOutputNodeChanged() OVERRIDE;
  virtual void OnActiveInputNodeChanged() OVERRIDE;

  void Update();

  tray::VolumeView* volume_view_;
  tray::AudioDetailedView* audio_detail_;

  // True if VolumeView should be created for accelerator pop up;
  // Otherwise, it should be created for detailed view in ash tray bubble.
  bool pop_up_volume_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayAudio);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_H_
