// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_
#define ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_

#include <stdint.h>

#include <memory>

#include "ash/system/audio/audio_observer.h"
#include "ash/system/tray/tray_image_item.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"

namespace ash {

namespace system {
class TrayAudioDelegate;
}

namespace tray {
class VolumeView;
}

class TrayAudio : public TrayImageItem,
                  public AudioObserver,
                  public display::DisplayObserver {
 public:
  TrayAudio(SystemTray* system_tray,
            std::unique_ptr<system::TrayAudioDelegate> audio_delegate);
  ~TrayAudio() override;

  static bool ShowAudioDeviceMenu();

 protected:
  // Overridden from display::DisplayObserver.
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;

  virtual void Update();

  std::unique_ptr<system::TrayAudioDelegate> audio_delegate_;
  tray::VolumeView* volume_view_;

  // True if VolumeView should be created for accelerator pop up;
  // Otherwise, it should be created for detailed view in ash tray bubble.
  bool pop_up_volume_view_;

 private:
  // Overridden from TrayImageItem.
  bool GetInitialVisibility() override;

  // Overridden from SystemTrayItem.
  views::View* CreateDefaultView(user::LoginStatus status) override;
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyDefaultView() override;
  void DestroyDetailedView() override;
  bool ShouldHideArrow() const override;
  bool ShouldShowShelf() const override;

  // Overridden from AudioObserver.
  void OnOutputNodeVolumeChanged(uint64_t node_id, double volume) override;
  void OnOutputMuteChanged(bool mute_on, bool system_adjust) override;
  void OnAudioNodesChanged() override;
  void OnActiveOutputNodeChanged() override;
  void OnActiveInputNodeChanged() override;

  void ChangeInternalSpeakerChannelMode();

  DISALLOW_COPY_AND_ASSIGN(TrayAudio);
};

}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_TRAY_AUDIO_H_
