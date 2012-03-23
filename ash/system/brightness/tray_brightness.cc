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

class BrightnessView : public views::View,
                       public views::SliderListener {
 public:
  BrightnessView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

    views::ImageView* icon = new FixedSizedImageView(0, kTrayPopupItemHeight);
    gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_BRIGHTNESS);
    icon->SetImage(image.ToSkBitmap());
    AddChildView(icon);

    slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
    // TODO(sad|davemoore):  There is currently no way to get the brightness
    // level of the system. So start with a random value.
    // http://crosbug.com/26935
    slider_->SetValue(0.8f);
    slider_->set_focusable(true);
    AddChildView(slider_);
  }

  virtual ~BrightnessView() {}

  void SetBrightnessLevel(float percent) {
    slider_->SetValue(percent);
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
    // TODO(sad|davemoore): This isn't correct, since we are unable to pass on
    // the amount the brightness should be increased/decreased.
    // http://crosbug.com/26935
#if !defined(OS_MACOSX)
    AcceleratorController* ac = Shell::GetInstance()->accelerator_controller();
    if (ac->brightness_control_delegate()) {
      BrightnessControlDelegate* delegate = ac->brightness_control_delegate();
      if (value < old_value)
        delegate->HandleBrightnessDown(ui::Accelerator());
      else
        delegate->HandleBrightnessUp(ui::Accelerator());
    }
#endif  // OS_MACOSX
  }

  views::Slider* slider_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessView);
};

}  // namespace tray

TrayBrightness::TrayBrightness() {}

TrayBrightness::~TrayBrightness() {}

views::View* TrayBrightness::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayBrightness::CreateDefaultView(user::LoginStatus status) {
  brightness_view_.reset(new tray::BrightnessView);
  return brightness_view_.get();
}

views::View* TrayBrightness::CreateDetailedView(user::LoginStatus status) {
  brightness_view_.reset(new tray::BrightnessView);
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

void TrayBrightness::OnBrightnessChanged(float percent, bool user_initiated) {
  if (brightness_view_.get())
    brightness_view_->SetBrightnessLevel(percent);
  if (!user_initiated)
    return;

  if (brightness_view_.get())
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
  else
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

}  // namespace internal
}  // namespace ash
