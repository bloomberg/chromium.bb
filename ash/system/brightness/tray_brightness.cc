// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/tray_brightness.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/system/brightness/brightness_control_delegate.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_views.h"
#include "base/utf_string_conversions.h"
#include "grit/ash_strings.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {
namespace internal {

namespace tray {

namespace {

// Avoid asking for the screen brightness to be updated until the slider has
// been moved by more than this much from the last actual brightness level that
// we observed (given a total range of [0.0, 1.0]).  It's roughly based on the
// amount by which the power manager adjusts the brightness for each
// brightness-up or -down request.
const float kMinBrightnessChange = 0.05f;

}  // namespace

class BrightnessView : public views::View,
                       public views::SliderListener {
 public:
  explicit BrightnessView(float initial_fraction)
      : last_fraction_(initial_fraction) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

    views::ImageView* icon = new FixedSizedImageView(0, kTrayPopupItemHeight);
    gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_BRIGHTNESS);
    icon->SetImage(image.ToSkBitmap());
    AddChildView(icon);

    slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
    slider_->SetValue(initial_fraction);
    slider_->set_focusable(true);
    slider_->SetAccessibleName(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BRIGHTNESS));
    AddChildView(slider_);
  }

  virtual ~BrightnessView() {}

  // |fraction| is in the range [0.0, 1.0].
  void SetBrightnessFraction(float fraction) {
    last_fraction_ = fraction;
    slider_->SetValue(fraction);
  }

 private:
  // Overridden from views::View.
  virtual void OnBoundsChanged(const gfx::Rect& old_bounds) OVERRIDE {
    int w = width() - slider_->x();
    slider_->SetSize(gfx::Size(w, slider_->height()));
  }

  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value,
                                  views::SliderChangeReason reason) OVERRIDE {
    if (reason != views::VALUE_CHANGED_BY_USER)
      return;
    // TODO(derat): This isn't correct, since we are unable to pass the level to
    // which the brightness should be set.
    // http://crbug.com/119743
#if !defined(OS_MACOSX)
    AcceleratorController* ac = Shell::GetInstance()->accelerator_controller();
    if (ac->brightness_control_delegate()) {
      BrightnessControlDelegate* delegate = ac->brightness_control_delegate();
      if (value <= last_fraction_ - kMinBrightnessChange || value < 0.001)
        delegate->HandleBrightnessDown(ui::Accelerator());
      else if (value >= last_fraction_ + kMinBrightnessChange || value > 0.999)
        delegate->HandleBrightnessUp(ui::Accelerator());
    }
#endif  // OS_MACOSX
  }

  views::Slider* slider_;

  // Last brightness level that we observed, in the range [0.0, 1.0].
  float last_fraction_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessView);
};

}  // namespace tray

// TODO(derat):  There is currently no way to get the brightness level of the
// system, so start with a random value.  http://crosbug.com/26935
TrayBrightness::TrayBrightness() : current_fraction_(0.8f) {}

TrayBrightness::~TrayBrightness() {}

views::View* TrayBrightness::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayBrightness::CreateDefaultView(user::LoginStatus status) {
  brightness_view_.reset(new tray::BrightnessView(current_fraction_));
  return brightness_view_.get();
}

views::View* TrayBrightness::CreateDetailedView(user::LoginStatus status) {
  brightness_view_.reset(new tray::BrightnessView(current_fraction_));
  return brightness_view_.get();
}

void TrayBrightness::DestroyTrayView() {
}

void TrayBrightness::DestroyDefaultView() {
  brightness_view_.reset();
}

void TrayBrightness::DestroyDetailedView() {
  brightness_view_.reset();
}

void TrayBrightness::OnBrightnessChanged(float fraction, bool user_initiated) {
  current_fraction_ = fraction;
  if (brightness_view_.get())
    brightness_view_->SetBrightnessFraction(fraction);
  if (!user_initiated)
    return;

  if (brightness_view_.get())
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
  else
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

}  // namespace internal
}  // namespace ash
