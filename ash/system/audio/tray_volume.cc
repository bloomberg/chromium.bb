// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/audio/tray_volume.h"

#include <cmath>

#include "ash/shell.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "ash/volume_control_delegate.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_resources.h"
#include "grit/ash_strings.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkRect.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_skia_operations.h"
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

// IDR_AURA_UBER_TRAY_VOLUME_LEVELS contains 5 images,
// The one for mute is at the 0 index and the other
// four are used for ascending volume levels.
const int kVolumeLevels = 4;

bool IsAudioMuted() {
  return Shell::GetInstance()->tray_delegate()->
      GetVolumeControlDelegate()->IsAudioMuted();
}

float GetVolumeLevel() {
  return Shell::GetInstance()->tray_delegate()->
      GetVolumeControlDelegate()->GetVolumeLevel();
}

}  // namespace

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
    float level = GetVolumeLevel();
    int image_index = IsAudioMuted() ?
        0 : (level == 1.0 ?
             kVolumeLevels :
             std::max(1, int(std::ceil(level * (kVolumeLevels - 1)))));
    if (image_index != image_index_) {
      gfx::Rect region(0, image_index * kVolumeImageHeight,
                       kVolumeImageWidth, kVolumeImageHeight);
      gfx::ImageSkia image_skia = gfx::ImageSkiaOperations::ExtractSubset(
          *(image_.ToImageSkia()), region);
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

class MuteButton : public ash::internal::TrayBarButtonWithTitle {
 public:
  explicit MuteButton(views::ButtonListener* listener)
      : TrayBarButtonWithTitle(listener,
            -1,    // no title under mute button
            kTrayBarButtonWidth) {
    Update();
  }
  virtual ~MuteButton() {}

  void Update() {
    UpdateButton(IsAudioMuted());
    SchedulePaint();
  }

  DISALLOW_COPY_AND_ASSIGN(MuteButton);
};

class VolumeSlider : public views::Slider {
 public:
  explicit VolumeSlider(views::SliderListener* listener)
      : views::Slider(listener, views::Slider::HORIZONTAL) {
    set_focus_border_color(kFocusBorderColor);
    SetValue(GetVolumeLevel());
    SetAccessibleName(
            ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
                IDS_ASH_STATUS_TRAY_VOLUME));
    Update();
  }
  virtual ~VolumeSlider() {}

  void Update() {
    UpdateState(!IsAudioMuted());
  }

  DISALLOW_COPY_AND_ASSIGN(VolumeSlider);
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

    mute_ = new MuteButton(this);
    AddChildView(mute_);

    slider_ = new VolumeSlider(this);
    AddChildView(slider_);
  }

  virtual ~VolumeView() {}

  void Update() {
    icon_->Update();
    mute_->Update();
    slider_->Update();
  }

  void SetVolumeLevel(float percent) {
    // The change in volume will be reflected via accessibility system events,
    // so we prevent the UI event from being sent here.
    slider_->set_enable_accessibility_events(false);
    slider_->SetValue(percent);
    // It is possible that the volume was (un)muted, but the actual volume level
    // did not change. In that case, setting the value of the slider won't
    // trigger an update. So explicitly trigger an update.
    Update();
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
                             const ui::Event& event) OVERRIDE {
    CHECK(sender == icon_ || sender == mute_);
    ash::Shell::GetInstance()->tray_delegate()->
        GetVolumeControlDelegate()->SetAudioMuted(!IsAudioMuted());
  }

  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE {
    if (reason == views::VALUE_CHANGED_BY_USER) {
      ash::Shell::GetInstance()->tray_delegate()->
          GetVolumeControlDelegate()->SetVolumeLevel(value);
    }
    icon_->Update();
  }

  VolumeButton* icon_;
  MuteButton* mute_;
  VolumeSlider* slider_;

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
  return IsAudioMuted();
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
    if (IsAudioMuted())
      percent = 0.0;
    volume_view_->SetVolumeLevel(percent);
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
    return;
  }
  PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

void TrayVolume::OnMuteToggled() {
  if (tray_view())
      tray_view()->SetVisible(GetInitialVisibility());

  if (volume_view_)
    volume_view_->Update();
  else
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

}  // namespace internal
}  // namespace ash
