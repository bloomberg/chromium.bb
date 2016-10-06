// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELL_WINDOW_TYPE_LAUNCHER_H_
#define ASH_SHELL_WINDOW_TYPE_LAUNCHER_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/widget/widget_delegate.h"

namespace views {
class MenuRunner;
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
  ~WindowTypeLauncher() override;

 private:
  typedef std::pair<aura::Window*, gfx::Rect> WindowAndBoundsPair;

  enum MenuCommands {
    COMMAND_NEW_WINDOW = 1,
    COMMAND_TOGGLE_FULLSCREEN = 3,
  };

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // Overridden from views::WidgetDelegate:
  bool CanResize() const override;
  base::string16 GetWindowTitle() const override;
  bool CanMaximize() const override;
  bool CanMinimize() const override;

  // Overridden from views::ButtonListener:
  void ButtonPressed(views::Button* sender, const ui::Event& event) override;

  // Overridden from views::MenuDelegate:
  void ExecuteCommand(int id, int event_flags) override;

  // Override from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  views::Button* create_button_;
  views::Button* panel_button_;
  views::Button* create_nonresizable_button_;
  views::Button* bubble_button_;
  views::Button* lock_button_;
  views::Button* widgets_button_;
  views::Button* system_modal_button_;
  views::Button* window_modal_button_;
  views::Button* child_modal_button_;
  views::Button* transient_button_;
  views::Button* examples_button_;
  views::Button* show_hide_window_button_;
  views::Button* show_web_notification_;
  std::unique_ptr<views::MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(WindowTypeLauncher);
};

}  // namespace shell
}  // namespace ash

#endif  // ASH_SHELL_WINDOW_TYPE_LAUNCHER_H_
