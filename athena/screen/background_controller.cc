// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/screen/background_controller.h"

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "extensions/shell/common/version.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/controls/label.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

namespace {

const SkColor kVersionColor = SK_ColorWHITE;
const SkColor kVersionBackground = SK_ColorTRANSPARENT;
const SkColor kVersionShadow = 0xB0000000;
const int kVersionShadowBlur = 10;

class VersionView : public views::Label {
 public:
  VersionView() {
    SetEnabledColor(kVersionColor);
    SetBackgroundColor(kVersionBackground);
    SetShadows(gfx::ShadowValues(1, gfx::ShadowValue(gfx::Point(0, 1),
                                                     kVersionShadowBlur,
                                                     kVersionShadow)));
    SetText(base::UTF8ToUTF16(base::StringPrintf("%s (Build %s)",
                                                 PRODUCT_VERSION,
                                                 LAST_CHANGE)));
    SetBoundsRect(gfx::Rect(gfx::Point(), GetPreferredSize()));
  }
  virtual ~VersionView() {
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(VersionView);
};

}  // namespace

class BackgroundView : public views::View {
 public:
  BackgroundView() {
    AddChildView(new VersionView);
  }
  virtual ~BackgroundView() {}

  void SetImage(const gfx::ImageSkia& image) {
    image_ = image;
    SchedulePaint();
  }

  // views::View
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->DrawImageInt(image_,
                         0,
                         0,
                         image_.width(),
                         image_.height(),
                         0,
                         0,
                         width(),
                         height(),
                         true);
  }

 private:
  gfx::ImageSkia image_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

BackgroundController::BackgroundController(aura::Window* container) {
  // TODO(oshima): Using widget to just draw an image is probably
  // overkill. Just use WindowDelegate to draw the background and
  // remove dependency to ui/views.

  views::Widget* background_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.accept_events = false;
  params.parent = container;
  background_widget->Init(params);
  background_widget->GetNativeWindow()->layer()->SetMasksToBounds(true);
  background_view_ = new BackgroundView;
  background_widget->SetContentsView(background_view_);
  background_widget->GetNativeView()->SetName("BackgroundWidget");
  background_widget->Show();
}

BackgroundController::~BackgroundController() {
  // background_widget is owned by the container and will be deleted
  // when the container is deleted.
}

void BackgroundController::SetImage(const gfx::ImageSkia& image) {
  // TODO(oshima): implement cross fede animation.
  background_view_->SetImage(image);
}

}  // namespace athena
