// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_
#define ASH_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_

#include "ash/system/tray/tray_details_view.h"
#include "ash/system/tray/view_click_listener.h"
#include "ash/system/user/login_status.h"
#include "chromeos/audio/audio_device.h"

#include "ui/gfx/font.h"

namespace views {
class View;
}

namespace ash {
class HoverHighlightView;

namespace tray {

class AudioDetailedView : public TrayDetailsView,
                          public ViewClickListener {
 public:
  AudioDetailedView(SystemTrayItem* owner, user::LoginStatus login);

  virtual ~AudioDetailedView();

  void Update();

 private:
  void AddScrollListInfoItem(const base::string16& text);

  HoverHighlightView* AddScrollListItem(const base::string16& text,
                                        gfx::Font::FontStyle style,
                                        bool checked);

  void CreateHeaderEntry();
  void CreateItems();

  void UpdateScrollableList();
  void UpdateAudioDevices();

  // Overridden from ViewClickListener.
  virtual void OnViewClicked(views::View* sender) OVERRIDE;

  user::LoginStatus login_;

  typedef std::map<views::View*, chromeos::AudioDevice> AudioDeviceMap;

  chromeos::AudioDeviceList output_devices_;
  chromeos::AudioDeviceList input_devices_;
  AudioDeviceMap device_map_;

  DISALLOW_COPY_AND_ASSIGN(AudioDetailedView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_AUDIO_AUDIO_DETAILED_VIEW_H_
