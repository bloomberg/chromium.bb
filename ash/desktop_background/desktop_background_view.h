// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
#define ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_

#include <memory>

#include "ui/views/context_menu_controller.h"
#include "ui/views/view.h"

namespace ash {

class PreEventDispatchHandler;
class WmWindow;

class DesktopBackgroundView : public views::View,
                              public views::ContextMenuController {
 public:
  DesktopBackgroundView();
  ~DesktopBackgroundView() override;

 private:
  friend class DesktopBackgroundControllerTest;

  // Overridden from views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;

  // Overridden from views::ContextMenuController:
  void ShowContextMenuForView(views::View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;
  std::unique_ptr<PreEventDispatchHandler> pre_dispatch_handler_;

  DISALLOW_COPY_AND_ASSIGN(DesktopBackgroundView);
};

views::Widget* CreateDesktopBackground(WmWindow* root_window, int container_id);

}  // namespace ash

#endif  // ASH_DESKTOP_BACKGROUND_DESKTOP_BACKGROUND_VIEW_H_
