// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/system_background_controller.h"

#include "ash/shell_window_ids.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/canvas.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace internal {

// View implementation responsible for rendering the background.
class SystemBackgroundController::View : public views::WidgetDelegateView {
 public:
  explicit View(SystemBackgroundController* controller);
  virtual ~View();

  // Closes the hosting widget.
  void Close();

  // WidgetDelegate overrides:
  virtual views::View* GetContentsView() OVERRIDE;

  // View overrides:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

 private:
  SystemBackgroundController* controller_;

  DISALLOW_COPY_AND_ASSIGN(View);
};

SystemBackgroundController::View::View(SystemBackgroundController* controller)
    : controller_(controller) {
}

SystemBackgroundController::View::~View() {
  if (controller_)
    controller_->view_ = NULL;
}

void SystemBackgroundController::View::Close() {
  controller_ = NULL;
  GetWidget()->Close();
}

views::View* SystemBackgroundController::View::GetContentsView() {
  return this;
}

void SystemBackgroundController::View::OnPaint(gfx::Canvas* canvas) {
  // TODO: get image!
  canvas->DrawColor(SK_ColorBLACK);
}

SystemBackgroundController::SystemBackgroundController(aura::RootWindow* root)
    : ALLOW_THIS_IN_INITIALIZER_LIST(view_(new View(this))) {
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.delegate = view_;
  params.parent = root->GetChildById(kShellWindowId_SystemBackgroundContainer);
  widget->Init(params);
  widget->SetBounds(params.parent->bounds());
  widget->Show();
  widget->GetNativeView()->SetName("SystemBackground");
}

SystemBackgroundController::~SystemBackgroundController() {
  if (view_)
    view_->Close();
}

}  // namespace internal
}  // namespace ash
