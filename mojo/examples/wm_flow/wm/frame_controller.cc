// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mojo/examples/wm_flow/wm/frame_controller.h"

#include "base/strings/utf_string_conversions.h"
#include "mojo/services/public/cpp/view_manager/view.h"
#include "mojo/views/native_widget_view_manager.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/layout/layout_manager.h"
#include "ui/views/widget/widget.h"

class FrameController::LayoutManager : public views::LayoutManager,
                                       public views::ButtonListener {
 public:
  explicit LayoutManager(FrameController* controller)
      : controller_(controller),
        close_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Begone"))),
        maximize_button_(
            new views::LabelButton(this, base::ASCIIToUTF16("Embiggen"))) {}
  virtual ~LayoutManager() {}

 private:
  static const int kButtonFrameMargin = 5;
  static const int kButtonFrameSpacing = 2;
  static const int kFrameSize = 10;

  // Overridden from views::LayoutManager:
  virtual void Installed(views::View* host) OVERRIDE {
    host->AddChildView(close_button_);
    host->AddChildView(maximize_button_);
  }
  virtual void Layout(views::View* host) OVERRIDE {
    gfx::Size ps = close_button_->GetPreferredSize();
    gfx::Rect bounds = host->GetLocalBounds();
    close_button_->SetBounds(bounds.right() - kButtonFrameMargin - ps.width(),
                             kButtonFrameMargin, ps.width(), ps.height());

    ps = maximize_button_->GetPreferredSize();
    maximize_button_->SetBounds(
        close_button_->x() - kButtonFrameSpacing - ps.width(),
        kButtonFrameMargin, ps.width(), ps.height());

    bounds.Inset(kFrameSize,
                 close_button_->bounds().bottom() + kButtonFrameMargin,
                 kFrameSize, kFrameSize);
    controller_->app_view_->SetBounds(bounds);
  }
  virtual gfx::Size GetPreferredSize(const views::View* host) const OVERRIDE {
    return gfx::Size();
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    if (sender == close_button_)
      controller_->CloseWindow();
    else if (sender == maximize_button_)
      controller_->ToggleMaximize();
  }

  FrameController* controller_;
  views::Button* close_button_;
  views::Button* maximize_button_;

  DISALLOW_COPY_AND_ASSIGN(LayoutManager);
};

////////////////////////////////////////////////////////////////////////////////
// FrameController, public:

FrameController::FrameController(mojo::View* view, mojo::View** app_view)
    : view_(view),
      app_view_(mojo::View::Create(view->view_manager())),
      frame_view_(new views::View),
      frame_view_layout_manager_(new LayoutManager(this)),
      widget_(new views::Widget),
      maximized_(false) {
  view_->AddChild(app_view_);
  view_->AddObserver(this);
  *app_view = app_view_;
  frame_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorBLUE));
  frame_view_->SetLayoutManager(frame_view_layout_manager_);
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.native_widget = new mojo::NativeWidgetViewManager(widget_, view_);
  params.bounds = gfx::Rect(view_->bounds().size());
  widget_->Init(params);
  widget_->SetContentsView(frame_view_);
  widget_->Show();
}

FrameController::~FrameController() {}

void FrameController::CloseWindow() {
  app_view_->Destroy();
  view_->Destroy();
}

void FrameController::ToggleMaximize() {
  if (!maximized_)
    restored_bounds_ = view_->bounds();
  maximized_ = !maximized_;
  if (maximized_)
    view_->SetBounds(view_->parent()->bounds());
  else
    view_->SetBounds(restored_bounds_);
}

////////////////////////////////////////////////////////////////////////////////
// FrameController, mojo::ViewObserver implementation:

void FrameController::OnViewDestroyed(mojo::View* view) {
  view_->RemoveObserver(this);
  delete this;
}
