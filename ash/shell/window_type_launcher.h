// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_TYPE_LAUNCHER_H_
#define ASH_SHELL_WINDOW_TYPE_LAUNCHER_H_

#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class MenuRunner;
class LabelButton;
}

namespace ash {
namespace shell {

// The contents view/delegate of a window that shows some buttons that create
// various window types.
class WindowTypeLauncher : public views::WidgetDelegateView,
                           public views::ButtonListener,
                           public views::MenuDelegate,
                           public views::ContextMenuController {
 public:
  WindowTypeLauncher();
  virtual ~WindowTypeLauncher();

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  enum MenuCommands {
    COMMAND_NEW_WINDOW = 1,
    COMMAND_TOGGLE_FULLSCREEN = 3,
  };

  // Overridden from views::View:
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;

  // Overridden from views::WidgetDelegate:
  virtual views::View* GetContentsView() OVERRIDE;
  virtual bool CanResize() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual bool CanMaximize() const OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int id, int event_flags) OVERRIDE;

  // Override from views::ContextMenuController:
  virtual void ShowContextMenuForView(views::View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  views::LabelButton* create_button_;
  views::LabelButton* create_persistant_button_;
  views::LabelButton* panel_button_;
  views::LabelButton* create_nonresizable_button_;
  views::LabelButton* bubble_button_;
  views::LabelButton* lock_button_;
  views::LabelButton* widgets_button_;
  views::LabelButton* system_modal_button_;
  views::LabelButton* window_modal_button_;
  views::LabelButton* child_modal_button_;
  views::LabelButton* transient_button_;
  views::LabelButton* examples_button_;
  views::LabelButton* show_hide_window_button_;
  views::LabelButton* show_screensaver_;
  views::LabelButton* show_web_notification_;
  scoped_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_TYPE_LAUNCHER_H_
