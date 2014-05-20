// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/session/session_state_delegate.h"
#include "ash/shell.h"
#include "ash/shell/example_factory.h"
#include "ash/shell_window_ids.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_utils.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/corewm/tooltip_controller.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

using ash::Shell;

namespace ash {
namespace shell {

class LockView : public views::WidgetDelegateView,
                 public views::ButtonListener {
 public:
  LockView() : unlock_button_(new views::LabelButton(
                                  this, base::ASCIIToUTF16("Unlock"))) {
    unlock_button_->SetStyle(views::Button::STYLE_BUTTON);
    AddChildView(unlock_button_);
    unlock_button_->SetFocusable(true);
  }
  virtual ~LockView() {}

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE {
    return gfx::Size(500, 400);
  }

 private:
  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE {
    canvas->FillRect(GetLocalBounds(), SK_ColorYELLOW);
    base::string16 text = base::ASCIIToUTF16("LOCKED!");
    int string_width = gfx::GetStringWidth(text, font_list_);
    canvas->DrawStringRect(text, font_list_, SK_ColorRED,
                           gfx::Rect((width() - string_width)/ 2,
                                     (height() - font_list_.GetHeight()) / 2,
                                     string_width, font_list_.GetHeight()));
  }
  virtual void Layout() OVERRIDE {
    gfx::Rect bounds = GetLocalBounds();
    gfx::Size ps = unlock_button_->GetPreferredSize();
    bounds.set_y(bounds.bottom() - ps.height() - 5);
    bounds.set_x((bounds.width() - ps.width()) / 2);
    bounds.set_size(ps);
    unlock_button_->SetBoundsRect(bounds);
  }
  virtual void ViewHierarchyChanged(
      const ViewHierarchyChangedDetails& details) OVERRIDE {
    if (details.is_add && details.child == this)
      unlock_button_->RequestFocus();
  }

  // Overridden from views::WidgetDelegateView:
  virtual void WindowClosing() OVERRIDE {
    Shell::GetInstance()->session_state_delegate()->UnlockScreen();
  }

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE {
    DCHECK(sender == unlock_button_);
    GetWidget()->Close();
  }

  gfx::FontList font_list_;
  views::LabelButton* unlock_button_;

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
  params.parent = Shell::GetContainer(Shell::GetPrimaryRootWindow(),
                                      kShellWindowId_LockScreenContainer);
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
