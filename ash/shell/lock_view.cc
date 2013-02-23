// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/shell_window_ids.h"
#include "ash/shell/example_factory.h"
#include "base/utf_string_conversions.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font.h"
#include "ui/views/controls/button/text_button.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using ash::Shell;

namespace ash {
namespace shell {

class LockView : public views::WidgetDelegateView,
                 public views::ButtonListener {
 public:
  LockView() : unlock_button_(ALLOW_THIS_IN_INITIALIZER_LIST(
                   new views::NativeTextButton(this, ASCIIToUTF16("Unlock")))) {
    AddChildView(unlock_button_);
    unlock_button_->set_focusable(true);
  }
  virtual ~LockView() {}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE {
    return gfx::Size(500, 400);
  }

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), SK_ColorYELLOW);
    string16 text = ASCIIToUTF16("LOCKED!");
    int string_width = font_.GetStringWidth(text);
    canvas->DrawStringInt(text, font_, SK_ColorRED, (width() - string_width)/ 2,
                          (height() - font_.GetHeight()) / 2,
                          string_width, font_.GetHeight());
  }
  virtual void Layout() OVERRIDE {
    gfx::Rect bounds = GetLocalBounds();
    gfx::Size ps = unlock_button_->GetPreferredSize();
    bounds.set_y(bounds.bottom() - ps.height() - 5);
    bounds.set_x((bounds.width() - ps.width()) / 2);
    bounds.set_size(ps);
    unlock_button_->SetBoundsRect(bounds);
  }
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE {
    if (is_add && child == this)
      unlock_button_->RequestFocus();
  }

  // Overridden from views::WidgetDelegateView:
  virtual void WindowClosing() OVERRIDE {
    Shell::GetInstance()->delegate()->UnlockScreen();
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK(sender == unlock_button_);
    GetWidget()->Close();
  }

  gfx::Font font_;
  views::NativeTextButton* unlock_button_;

  DISALLOW_COPY_AND_ASSIGN(LockView);
};

void CreateLockScreen() {
  LockView* lock_view = new LockView;
  views::Widget* widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  gfx::Size ps = lock_view->GetPreferredSize();

  gfx::Size root_window_size = Shell::GetPrimaryRootWindow()->bounds().size();
  params.bounds = gfx::Rect((root_window_size.width() - ps.width()) / 2,
                            (root_window_size.height() - ps.height()) / 2,
                            ps.width(), ps.height());
  params.delegate = lock_view;
  params.parent = Shell::GetContainer(
      Shell::GetPrimaryRootWindow(),
      internal::kShellWindowId_LockScreenContainer);
  widget->Init(params);
  widget->SetContentsView(lock_view);
  widget->Show();
  widget->GetNativeView()->SetName("LockView");
  widget->GetNativeView()->Focus();

  // TODO: it shouldn't be necessary to invoke UpdateTooltip() here.
  Shell::GetInstance()->tooltip_controller()->UpdateTooltip(
      widget->GetNativeView());
}

}  // namespace shell
}  // namespace ash
