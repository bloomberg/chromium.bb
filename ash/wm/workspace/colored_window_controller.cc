// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/colored_window_controller.h"

#include "ash/shell_window_ids.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

// View implementation responsible for rendering the background.
class ColoredWindowController::View : public views::WidgetDelegateView {
 public:
  explicit View(ColoredWindowController* controller);
  virtual ~View();

  // Closes the hosting widget.
  void Close();

  // WidgetDelegate overrides:
  virtual views::View* GetContentsView() OVERRIDE;

 private:
  ColoredWindowController* controller_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

ColoredWindowController::View::View(ColoredWindowController* controller)
    : controller_(controller) {
}

ColoredWindowController::View::~View() {
  if (controller_)
    controller_->view_ = NULL;
}

void ColoredWindowController::View::Close() {
  controller_ = NULL;
  GetWidget()->Close();
}

views::View* ColoredWindowController::View::GetContentsView() {
  return this;
}

ColoredWindowController::ColoredWindowController(aura::Window* parent,
                                                 const std::string& window_name)
    : ALLOW_THIS_IN_INITIALIZER_LIST(view_(new View(this))) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view_;
  params.parent = parent;
  params.can_activate = false;
  params.accept_events = false;
  params.layer_type = ui::LAYER_SOLID_COLOR;
  widget->Init(params);
  widget->GetNativeView()->SetProperty(aura::client::kAnimationsDisabledKey,
                                       true);
  widget->GetNativeView()->SetName(window_name);
  widget->SetBounds(parent->bounds());
}

ColoredWindowController::~ColoredWindowController() {
  if (view_)
    view_->Close();
}

void ColoredWindowController::SetColor(SkColor color) {
  if (view_)
    view_->GetWidget()->GetNativeView()->layer()->SetColor(color);
}

views::Widget* ColoredWindowController::GetWidget() {
  return view_ ? view_->GetWidget() : NULL;
}

}  // namespace internal
}  // namespace ash
