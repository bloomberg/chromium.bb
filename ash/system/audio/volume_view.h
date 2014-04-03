// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_AUDIO_VOLUME_VIEW_H_
#define ASH_SYSTEM_AUDIO_VOLUME_VIEW_H_

#include "ash/system/tray/actionable_view.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/slider.h"

namespace views {
class View;
class ImageView;
}

namespace ash {
class HoverHighlightView;
class SystemTrayItem;

namespace system {
class TrayAudioDelegate;
}

namespace tray {
class BarSeparator;
class VolumeButton;
class VolumeSlider;

class VolumeView : public ActionableView,
                   public views::ButtonListener,
                   public views::SliderListener {
 public:
  VolumeView(SystemTrayItem* owner,
             system::TrayAudioDelegate* audio_delegate,
             bool is_default_view);

  virtual ~VolumeView();

  void Update();

  // Sets volume level on slider_, |percent| is ranged from [0.00] to [1.00].
  void SetVolumeLevel(float percent);

 private:
  // Updates bar_, device_type_ icon, and more_ buttons.
  void UpdateDeviceTypeAndMore();
  void HandleVolumeUp(float percent);
  void HandleVolumeDown(float percent);

  // Overridden from views::View.
  virtual void Layout() OVERRIDE;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE;

  // Overriden from ActionableView.
  virtual bool PerformAction(const ui::Event& event) OVERRIDE;

  SystemTrayItem* owner_;
  system::TrayAudioDelegate* audio_delegate_;
  VolumeButton* icon_;
  VolumeSlider* slider_;
  BarSeparator* bar_;
  views::ImageView* device_type_;
  views::ImageView* more_;
  bool is_default_view_;

  DISALLOW_COPY_AND_ASSIGN(VolumeView);
};

}  // namespace tray
}  // namespace ash

#endif  // ASH_SYSTEM_AUDIO_VOLUME_VIEW_H_

