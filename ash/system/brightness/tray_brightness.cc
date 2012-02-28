// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/tray_brightness.h"

#include "ash/accelerators/accelerator_controller.h"
#include "ash/shell.h"
#include "ash/system/brightness/brightness_control_delegate.h"
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
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace {

class BrightnessView : public views::View,
                       public views::SliderListener {
 public:
  BrightnessView() {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kHorizontal,
          0, 0, 5));

    views::ImageView* icon = new views::ImageView();
    gfx::Image image = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
        IDR_AURA_UBER_TRAY_BRIGHTNESS);
    icon->SetImage(image.ToSkBitmap());
    AddChildView(icon);

    views::Slider* slider = new views::Slider(this, views::Slider::HORIZONTAL);
    // TODO(sad|davemoore):  There is currently no way to get the brightness
    // level of the system. So start with a random value.
    // http://crosbug.com/26935
    slider->SetValue(0.8f);
    slider->set_border(views::Border::CreateEmptyBorder(0, 0, 0, 20));
    AddChildView(slider);
  }

 private:
  // Overridden from views:SliderListener.
  virtual void SliderValueChanged(views::Slider* sender,
                                  float value,
                                  float old_value) OVERRIDE {
    // TODO(sad|davemoore): This isn't correct, since we are unable to pass on
    // the amount the brightness should be increased/decreased.
    // http://crosbug.com/26935
    ash::Shell* shell = ash::Shell::GetInstance();
    if (value < old_value) {
      shell->accelerator_controller()->brightness_control_delegate()->
          HandleBrightnessDown(ui::Accelerator());
    } else {
      shell->accelerator_controller()->brightness_control_delegate()->
          HandleBrightnessUp(ui::Accelerator());
    }
  }

  DISALLOW_COPY_AND_ASSIGN(BrightnessView);
};

}  // namespace

namespace ash {
namespace internal {

TrayBrightness::TrayBrightness() {}

TrayBrightness::~TrayBrightness() {}

views::View* TrayBrightness::CreateTrayView() {
  return NULL;
}

views::View* TrayBrightness::CreateDefaultView() {
  return new BrightnessView;
}

views::View* TrayBrightness::CreateDetailedView() {
  NOTIMPLEMENTED();
  return NULL;
}

void TrayBrightness::DestroyTrayView() {
}

void TrayBrightness::DestroyDefaultView() {
}

void TrayBrightness::DestroyDetailedView() {
}

}  // namespace internal
}  // namespace ash
