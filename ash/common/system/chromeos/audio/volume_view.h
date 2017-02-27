// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_VOLUME_VIEW_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_VOLUME_VIEW_H_

#include "base/macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slider.h"
#include "ui/views/view.h"

namespace views {
class CustomButton;
class ImageView;
class Slider;
}

namespace ash {
class SystemTrayItem;
class TriView;

namespace system {
class TrayAudioDelegate;
}

namespace tray {
class VolumeButton;

class VolumeView : public views::View,
                   public views::SliderListener,
                   public views::ButtonListener {
 public:
  VolumeView(SystemTrayItem* owner,
             system::TrayAudioDelegate* audio_delegate,
             bool is_default_view);

  ~VolumeView() override;

  void Update();

  // Sets volume level on slider_, |percent| is ranged from [0.00] to [1.00].
  void SetVolumeLevel(float percent);

 private:
  // Updates device_type_ icon and more_ button.
  void UpdateDeviceTypeAndMore();
  void HandleVolumeUp(int percent);
  void HandleVolumeDown(int percent);

  // SliderListener:
  void SliderValueChanged(views::Slider* sender,
                          float value,
                          float old_value,
                          views::SliderChangeReason reason) override;

  // views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  SystemTrayItem* owner_;
  // The only immediate child view of |this|. All other view elements are added
  // to the |tri_view_| to handle layout.
  TriView* tri_view_;
  system::TrayAudioDelegate* audio_delegate_;
  views::CustomButton* more_button_;
  VolumeButton* icon_;
  views::Slider* slider_;
  views::ImageView* device_type_;
  bool is_default_view_;

  DISALLOW_COPY_AND_ASSIGN(VolumeView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_AUDIO_VOLUME_VIEW_H_
