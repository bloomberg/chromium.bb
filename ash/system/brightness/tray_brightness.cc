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
#include "base/bind.h"
#include "base/message_loop.h"
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

// We don't let the screen brightness go lower than this when it's being
// adjusted via the slider.  Otherwise, if the user doesn't know about the
// brightness keys, they may turn the backlight off and not know how to turn it
// back on.
const double kMinBrightnessPercent = 5.0;

}  // namespace

class BrightnessView : public views::View,
                       public views::SliderListener {
 public:
  explicit BrightnessView(double initial_percent)
      : dragging_(false),
        last_percent_(initial_percent) {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          kTrayPopupPaddingHorizontal, 0, kTrayPopupPaddingBetweenItems));

    views::ImageView* icon = new FixedSizedImageView(0, kTrayPopupItemHeight);
    gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_BRIGHTNESS);
    icon->SetImage(image.ToImageSkia());
    AddChildView(icon);

    slider_ = new views::Slider(this, views::Slider::HORIZONTAL);
    slider_->set_focus_border_color(kFocusBorderColor);
    slider_->SetValue(static_cast<float>(initial_percent / 100.0));
    slider_->SetAccessibleName(
        ui::ResourceBundle::GetSharedInstance().GetLocalizedString(
            IDS_ASH_STATUS_TRAY_BRIGHTNESS));
    AddChildView(slider_);
  }

  virtual ~BrightnessView() {}

  // |percent| is in the range [0.0, 100.0].
  void SetBrightnessPercent(double percent) {
    last_percent_ = percent;
    if (!dragging_)
      slider_->SetValue(static_cast<float>(percent / 100.0));
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
    DCHECK_EQ(sender, slider_);
    if (reason != views::VALUE_CHANGED_BY_USER)
      return;
#if !defined(OS_MACOSX)
    AcceleratorController* ac = Shell::GetInstance()->accelerator_controller();
    if (ac->brightness_control_delegate()) {
      double percent = std::max(value * 100.0, kMinBrightnessPercent);
      ac->brightness_control_delegate()->SetBrightnessPercent(percent, true);
    }
#endif  // OS_MACOSX
  }

  // Overridden from views:SliderListener.
  virtual void SliderDragStarted(views::Slider* slider) OVERRIDE {
    DCHECK_EQ(slider, slider_);
    dragging_ = true;
  }

  // Overridden from views:SliderListener.
  virtual void SliderDragEnded(views::Slider* slider) OVERRIDE {
    DCHECK_EQ(slider, slider_);
    dragging_ = false;
    slider_->SetValue(static_cast<float>(last_percent_ / 100.0));
  }

  views::Slider* slider_;

  // Is |slider_| currently being dragged?
  bool dragging_;

  // Last brightness level that we observed, in the range [0.0, 100.0].
  double last_percent_;

  DISALLOW_COPY_AND_ASSIGN(BrightnessView);
};

}  // namespace tray

TrayBrightness::TrayBrightness()
    : weak_ptr_factory_(this),
      brightness_view_(NULL),
      is_default_view_(false),
      current_percent_(100.0),
      got_current_percent_(false) {
  // Post a task to get the initial brightness; the BrightnessControlDelegate
  // isn't created yet.
  MessageLoopForUI::current()->PostTask(
      FROM_HERE,
      base::Bind(&TrayBrightness::GetInitialBrightness,
                 weak_ptr_factory_.GetWeakPtr()));
}

TrayBrightness::~TrayBrightness() {}

void TrayBrightness::GetInitialBrightness() {
  BrightnessControlDelegate* delegate =
      Shell::GetInstance()->accelerator_controller()->
      brightness_control_delegate();
  // Worrisome, but happens in unit tests, so don't log anything.
  if (!delegate)
    return;
  delegate->GetBrightnessPercent(
      base::Bind(&TrayBrightness::HandleInitialBrightness,
                 weak_ptr_factory_.GetWeakPtr()));
}

void TrayBrightness::HandleInitialBrightness(double percent) {
  if (!got_current_percent_)
    OnBrightnessChanged(percent, false);
}

views::View* TrayBrightness::CreateTrayView(user::LoginStatus status) {
  return NULL;
}

views::View* TrayBrightness::CreateDefaultView(user::LoginStatus status) {
  CHECK(brightness_view_ == NULL);
  brightness_view_ = new tray::BrightnessView(current_percent_);
  is_default_view_ = true;
  return brightness_view_;
}

views::View* TrayBrightness::CreateDetailedView(user::LoginStatus status) {
  CHECK(brightness_view_ == NULL);
  brightness_view_ = new tray::BrightnessView(current_percent_);
  is_default_view_ = false;
  return brightness_view_;
}

void TrayBrightness::DestroyTrayView() {
}

void TrayBrightness::DestroyDefaultView() {
  if (is_default_view_)
    brightness_view_ = NULL;
}

void TrayBrightness::DestroyDetailedView() {
  if (!is_default_view_)
    brightness_view_ = NULL;
}

void TrayBrightness::UpdateAfterLoginStatusChange(user::LoginStatus status) {
}

void TrayBrightness::OnBrightnessChanged(double percent, bool user_initiated) {
  current_percent_ = percent;
  got_current_percent_ = true;

  if (brightness_view_)
    brightness_view_->SetBrightnessPercent(percent);
  if (!user_initiated)
    return;

  if (brightness_view_)
    SetDetailedViewCloseDelay(kTrayPopupAutoCloseDelayInSeconds);
  else
    PopupDetailedView(kTrayPopupAutoCloseDelayInSeconds, false);
}

}  // namespace internal
}  // namespace ash
