// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/system/background_controller.h"

#include "athena/system/public/system_ui.h"
#include "ui/aura/window.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace athena {

class BackgroundView : public views::View {
 public:
  BackgroundView()
      : time_view_(SystemUI::Get()->CreateTimeView()),
        status_icon_view_(SystemUI::Get()->CreateStatusIconView()) {
    AddChildView(time_view_);
    AddChildView(status_icon_view_);
  }
  virtual ~BackgroundView() {}

  void SetImage(const gfx::ImageSkia& image) {
    image_ = image;
    SchedulePaint();
  }

  // views::View:
  virtual void Layout() OVERRIDE {
    time_view_->SetBoundsRect(gfx::Rect(time_view_->GetPreferredSize()));
    gfx::Size status_icon_preferred_size =
        status_icon_view_->GetPreferredSize();
    status_icon_view_->SetBounds(width() - status_icon_preferred_size.width(),
                                 0,
                                 status_icon_preferred_size.width(),
                                 status_icon_preferred_size.height());
  }

  virtual void ChildPreferredSizeChanged(views::View* child) OVERRIDE {
    // Relayout when |status_icon_view_| changes its preferred size.
    Layout();
  }

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
  views::View* time_view_;
  views::View* status_icon_view_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

BackgroundController::BackgroundController(aura::Window* background_container) {
  views::Widget* background_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.parent = background_container;
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
