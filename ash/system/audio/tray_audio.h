// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_
#define ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_

#include "ash/system/audio/audio_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/display_observer.h"

namespace ash {

namespace system {
class TrayAudioDelegate;
}

namespace tray {
class VolumeView;
}

class TrayAudio : public TrayImageItem,
                  public AudioObserver,
                  public gfx::DisplayObserver {
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
  virtual bool GetInitialVisibility() override;

  // Overridden from SystemTrayItem.
  virtual views::View* CreateDefaultView(user::LoginStatus status) override;
  virtual views::View* CreateDetailedView(user::LoginStatus status) override;
  virtual void DestroyDefaultView() override;
  virtual void DestroyDetailedView() override;
  virtual bool ShouldHideArrow() const override;
  virtual bool ShouldShowShelf() const override;

  // Overridden from AudioObserver.
  virtual void OnOutputVolumeChanged() override;
  virtual void OnOutputMuteChanged() override;
  virtual void OnAudioNodesChanged() override;
  virtual void OnActiveOutputNodeChanged() override;
  virtual void OnActiveInputNodeChanged() override;

  // Overridden from gfx::DisplayObserver.
  virtual void OnDisplayAdded(const gfx::Display& new_display) override;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) override;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t changed_metrics) override;

  void ChangeInternalSpeakerChannelMode();

  DISALLOW_COPY_AND_ASSIGN(TrayAudio);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_
