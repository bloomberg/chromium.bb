// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_CHROMEOS_H_
#define ASH_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_CHROMEOS_H_

#include "ash/ash_export.h"
#include "ash/system/audio/tray_audio.h"
#include "base/memory/scoped_ptr.h"

namespace ash {
namespace tray {
class AudioDetailedView;
}

class ASH_EXPORT TrayAudioChromeOs : public TrayAudio {
 public:
  explicit TrayAudioChromeOs(SystemTray* system_tray);
  ~TrayAudioChromeOs() override;

 protected:
  // Overridden from TrayAudio
  void Update() override;

 private:
  // Overridden from SystemTrayItem.
  views::View* CreateDetailedView(user::LoginStatus status) override;
  void DestroyDetailedView() override;

  tray::AudioDetailedView* audio_detail_view_;

  DISALLOW_COPY_AND_ASSIGN(TrayAudioChromeOs);
};

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_AUDIO_TRAY_AUDIO_CHROMEOS_H_
