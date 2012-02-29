// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/tray_volume.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "base/utf_string_conversions.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace tray {

class VolumeView : public views::View,
                   public views::ButtonListener,
                   public views::SliderListener {
 public:
  VolumeView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          0, 0, 5));

    gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME);
    icon_ = new views::ToggleImageButton(this);
    icon_->SetImage(views::CustomButton::BS_NORMAL, image.ToSkBitmap());
    icon_->SetImage(views::CustomButton::BS_HOT, image.ToSkBitmap());
    icon_->SetImage(views::CustomButton::BS_PUSHED, image.ToSkBitmap());

    image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME_MUTE);
    icon_->SetToggledImage(views::CustomButton::BS_NORMAL, image.ToSkBitmap());
    icon_->SetToggledImage(views::CustomButton::BS_HOT, image.ToSkBitmap());
    icon_->SetToggledImage(views::CustomButton::BS_PUSHED, image.ToSkBitmap());

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    icon_->SetToggled(delegate->AudioMuted());
    AddChildView(icon_);

    slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
    slider_->SetValue(delegate->VolumeLevel());
    slider_->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 20));
    AddChildView(slider_);
  }

  void SetVolumeLevel(float percent) {
    slider_->SetValue(percent);
  }

 private:

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    CHECK(sender == icon_);
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    delegate->SetAudioMuted(!delegate->AudioMuted());

    // TODO(sad): Should the icon auto-update its state when mute/unmute happens
    // above?
    icon_->SetToggled(delegate->AudioMuted());
  }

  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE {
    if (reason != views::VALUE_CHANGED_BY_USER)
      return;
    ash::Shell::GetInstance()->tray_delegate()->SetVolumeLevel(value);
  }

  views::ToggleImageButton* icon_;
  views::Slider* slider_;

  DISALLOW_COPY_AND_ASSIGN(VolumeView);
};

}  // namespace tray

namespace ash {
namespace internal {

TrayVolume::TrayVolume() {
}

TrayVolume::~TrayVolume() {
}

views::View* TrayVolume::CreateTrayView() {
  tray_view_.reset(new views::ImageView());
  tray_view_->SetImage(ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME).ToSkBitmap());
  return tray_view_.get();
}

views::View* TrayVolume::CreateDefaultView() {
  volume_view_.reset(new tray::VolumeView);
  return volume_view_.get();
}

views::View* TrayVolume::CreateDetailedView() {
  NOTIMPLEMENTED();
  return NULL;
}

void TrayVolume::DestroyTrayView() {
  tray_view_.reset();
}

void TrayVolume::DestroyDefaultView() {
  volume_view_.reset();
}

void TrayVolume::DestroyDetailedView() {
}

void TrayVolume::OnVolumeChanged(float percent) {
  if (volume_view_.get()) {
    volume_view_->SetVolumeLevel(percent);
    return;
  }

  PopupDetailedView();
}

}  // namespace internal
}  // namespace ash
