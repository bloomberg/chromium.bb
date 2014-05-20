// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/tray_bar_button_with_title.h"

#include "ash/system/tray/tray_constants.h"
#include "base/memory/scoped_ptr.h"
#include "grit/ui_resources.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/painter.h"

namespace ash {
namespace {

const int kBarImagesActive[] = {
    IDR_SLIDER_ACTIVE_LEFT,
    IDR_SLIDER_ACTIVE_CENTER,
    IDR_SLIDER_ACTIVE_RIGHT,
};

const int kBarImagesDisabled[] = {
    IDR_SLIDER_DISABLED_LEFT,
    IDR_SLIDER_DISABLED_CENTER,
    IDR_SLIDER_DISABLED_RIGHT,
};

}  // namespace

class TrayBarButtonWithTitle::TrayBarButton : public views::View {
 public:
  TrayBarButton(const int bar_active_images[], const int bar_disabled_images[])
    : views::View(),
      bar_active_images_(bar_active_images),
      bar_disabled_images_(bar_disabled_images),
      painter_(new views::HorizontalPainter(bar_active_images_)){
  }
  virtual ~TrayBarButton() {}

  // Overriden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    painter_->Paint(canvas, size());
  }

  void Update(bool control_on) {
    painter_.reset(new views::HorizontalPainter(
        control_on ? bar_active_images_ : bar_disabled_images_));
    SchedulePaint();
  }

 private:
  const int* bar_active_images_;
  const int* bar_disabled_images_;
  scoped_ptr<views::HorizontalPainter> painter_;

  DISALLOW_COPY_AND_ASSIGN(TrayBarButton);
};

TrayBarButtonWithTitle::TrayBarButtonWithTitle(views::ButtonListener* listener,
                                               int title_id,
                                               int width)
    : views::CustomButton(listener),
      image_(new TrayBarButton(kBarImagesActive, kBarImagesDisabled)),
      title_(NULL),
      width_(width) {
  AddChildView(image_);
  if (title_id != -1) {
    title_ = new views::Label;
    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
    base::string16 text = rb.GetLocalizedString(title_id);
    title_->SetText(text);
    AddChildView(title_);
  }

  image_height_ = ui::ResourceBundle::GetSharedInstance().GetImageNamed(
      kBarImagesActive[0]).ToImageSkia()->height();
}

TrayBarButtonWithTitle::~TrayBarButtonWithTitle() {}

void TrayBarButtonWithTitle::UpdateButton(bool control_on) {
  image_->Update(control_on);
}

gfx::Size TrayBarButtonWithTitle::GetPreferredSize() const {
  return gfx::Size(width_, kTrayPopupItemHeight);
}

void TrayBarButtonWithTitle::Layout() {
  gfx::Rect rect(GetContentsBounds());
  int bar_image_y = rect.height() / 2 - image_height_ / 2;
  gfx::Rect bar_image_rect(rect.x(),
                           bar_image_y,
                           rect.width(),
                           image_height_);
  image_->SetBoundsRect(bar_image_rect);
  if (title_) {
    // The image_ has some empty space below the bar image, move the title
    // a little bit up to look closer to the bar.
    gfx::Size title_size = title_->GetPreferredSize();
    title_->SetBounds(rect.x(),
                      bar_image_y + image_height_ - 3,
                      rect.width(),
                      title_size.height());
  }
}

}  // namespace ash
