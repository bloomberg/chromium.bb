// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
#define ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_

#include "ash/system/audio/audio_observer.h"
#include "ash/system/tray/tray_image_item.h"

namespace ash {
namespace internal {

namespace tray {
class VolumeView;
}

class TrayVolume : public TrayImageItem,
                   public AudioObserver {
 public:
  explicit TrayVolume(SystemTray* system_tray);
  virtual ~TrayVolume();

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

  tray::VolumeView* volume_view_;

  // Was |volume_view_| created for CreateDefaultView() rather than
  // CreateDetailedView()?  Used to avoid resetting |volume_view_|
  // inappropriately in DestroyDefaultView() or DestroyDetailedView().
  bool is_default_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayVolume);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_VOLUME_H_
