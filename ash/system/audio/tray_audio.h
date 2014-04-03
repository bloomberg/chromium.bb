// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_
#define ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_

#include "ash/system/audio/audio_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "base/memory/scoped_ptr.h"

namespace ash {

namespace system {
class TrayAudioDelegate;
}

namespace tray {
class VolumeView;
}

class TrayAudio : public TrayImageItem,
                  public AudioObserver {
 public:
  TrayAudio(SystemTray* system_tray,
            scoped_ptr<system::TrayAudioDelegate> audio_delegate);
  virtual ~TrayAudio();

  static bool ShowAudioDeviceMenu();

 protected:
  virtual void Update();

  scoped_ptr<system::TrayAudioDelegate> audio_delegate_;
  tray::VolumeView* volume_view_;

  // True if VolumeView should be created for accelerator pop up;
  // Otherwise, it should be created for detailed view in ash tray bubble.
  bool pop_up_volume_view_;

 private:
  // Overridden from TrayImageItem.
  virtual bool GetInitialVisibility() OVERRIDE;

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) OVERRIDE;
  virtual views::View* CreateDetailedView(user::LoginStatus status) OVERRIDE;
  virtual void DestroyDefaultView() OVERRIDE;
  virtual void DestroyDetailedView() OVERRIDE;
  virtual bool ShouldHideArrow() const OVERRIDE;
  virtual bool ShouldShowShelf() const OVERRIDE;

  // Overridden from AudioObserver.
  virtual void OnOutputVolumeChanged() OVERRIDE;
  virtual void OnOutputMuteChanged() OVERRIDE;
  virtual void OnAudioNodesChanged() OVERRIDE;
  virtual void OnActiveOutputNodeChanged() OVERRIDE;
  virtual void OnActiveInputNodeChanged() OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(TrayAudio);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_
