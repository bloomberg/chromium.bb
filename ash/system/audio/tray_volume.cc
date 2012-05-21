// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/tray_volume.h"

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

namespace {
const int kVolumeImageWidth = 25;
const int kVolumeImageHeight = 25;
const int kVolumeLevel = 4;
}

namespace tray {

class VolumeButton : public views::ToggleImageButton {
 public:
  explicit VolumeButton(views::ButtonListener* listener)
      : views::ToggleImageButton(listener),
        image_index_(-1) {
    SetImageAlignment(ALIGN_CENTER, ALIGN_MIDDLE);
    image_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_VOLUME_LEVELS);
    SetPreferredSize(gfx::Size(kTrayPopupItemHeight, kTrayPopupItemHeight));
    Update();
  }

  virtual ~VolumeButton() {}

  void Update() {
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    int level = static_cast<int>(delegate->GetVolumeLevel() * 100);
    int image_index = level / (100 / kVolumeLevel);
    if (level > 0 && image_index == 0)
      ++image_index;
    if (level == 100)
      image_index = kVolumeLevel - 1;
    else if (image_index == kVolumeLevel - 1)
      --image_index;
    // Index 0 is reserved for mute.
    if (delegate->IsAudioMuted())
      image_index = 0;
    else
      ++image_index;
    if (image_index != image_index_) {
      SkIRect region = SkIRect::MakeXYWH(0, image_index * kVolumeImageHeight,
          kVolumeImageWidth, kVolumeImageHeight);
      gfx::ImageSkia image_skia;
      image_.ToImageSkia()->extractSubset(&image_skia, region);
      SetImage(views::CustomButton::BS_NORMAL, &image_skia);
      image_index_ = image_index;
    }
    SchedulePaint();
  }

 private:
  // Overridden from views::View.
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::ToggleImageButton::GetPreferredSize();
    size.set_height(kTrayPopupItemHeight);
    return size;
  }

  gfx::Image image_;
  int image_index_;

  DISALLOW_COPY_AND_ASSIGN(VolumeButton);
};

class VolumeView : public views::View,
                   public views::ButtonListener,
                   public views::SliderListener {
 public:
  VolumeView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

    icon_ = new VolumeButton(this);
    AddChildView(icon_);

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
    slider_->set_focus_border_color(kFocusBorderColor);
    slider_->SetValue(delegate->GetVolumeLevel());
    slider_->SetAccessibleName(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_VOLUME));
    AddChildView(slider_);
  }

  virtual ~VolumeView() {}

  void SetVolumeLevel(float percent) {
    // The change in volume will be reflected via accessibility system events,
    // so we prevent the UI event from being sent here.
    slider_->set_enable_accessibility_events(false);
    slider_->SetValue(percent);
    // It is possible that the volume was (un)muted, but the actual volume level
    // did not change. In that case, setting the value of the slider won't
    // trigger an update. So explicitly trigger an update.
    icon_->Update();
    slider_->set_enable_accessibility_events(true);
  }

 private:
  // Overridden from views::View.
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds) OVERRIDE {
    int w = width() - slider_->x();
    slider_->SetSize(gfx::Size(w, slider_->height()));
  }

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE {
    CHECK(sender == icon_);
    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    delegate->SetAudioMuted(!delegate->IsAudioMuted());
  }

  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE {
    if (reason == views::VALUE_CHANGED_BY_USER)
      ash::Shell::GetInstance()->tray_delegate()->SetVolumeLevel(value);
    icon_->Update();
  }

  VolumeButton* icon_;
  views::Slider* slider_;

  DISALLOW_COPY_AND_ASSIGN(VolumeView);
};

}  // namespace tray

TrayVolume::TrayVolume()
    : TrayImageItem(IDR_AURA_UBER_TRAY_VOLUME_MUTE),
      volume_view_(NULL),
      is_default_view_(false) {
}

TrayVolume::~TrayVolume() {
}

bool TrayVolume::GetInitialVisibility() {
  return ash::Shell::GetInstance()->tray_delegate()->IsAudioMuted();
}

views::View* TrayVolume::CreateDefaultView(user::LoginStatus status) {
  volume_view_ = new tray::VolumeView;
  is_default_view_ = true;
  return volume_view_;
}

views::View* TrayVolume::CreateDetailedView(user::LoginStatus status) {
  volume_view_ = new tray::VolumeView;
  is_default_view_ = false;
  return volume_view_;
}

void TrayVolume::DestroyDefaultView() {
  if (is_default_view_)
    volume_view_ = NULL;
}

void TrayVolume::DestroyDetailedView() {
  if (!is_default_view_)
    volume_view_ = NULL;
}

void TrayVolume::OnVolumeChanged(float percent) {
  if (tray_view())
    tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_) {
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }
  PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

}  // namespace internal
}  // namespace ash
