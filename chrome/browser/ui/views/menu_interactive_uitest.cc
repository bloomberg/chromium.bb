// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ui/views/controls/menu/menu_controller.h"

#include "base/macros.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/test_browser_window.h"
#include "ui/base/test/ui_controls.h"
#include "ui/gfx/geometry/point.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/test/platform_test_helper.h"
#include "ui/views/widget/widget.h"

#if !defined(OS_CHROMEOS)
#include "ui/views/test/native_widget_factory.h"
#endif

namespace views {
namespace test {

class MenuControllerUITest : public InProcessBrowserTest {
 public:
  MenuControllerUITest() {}

  // This method creates a MenuRunner, MenuItemView, etc, adds two menu
  // items, shows the menu so that it can calculate the position of the first
  // menu item and move the mouse there, and closes the menu.
  void SetupMenu(Widget* widget) {
    menu_delegate_ = std::make_unique<MenuDelegate>();
    MenuItemView* menu_item = new MenuItemView(menu_delegate_.get());
    menu_runner_ = std::make_unique<MenuRunner>(
        +menu_item, views::MenuRunner::CONTEXT_MENU);
    first_item_ =
        menu_item->AppendMenuItemWithLabel(1, base::ASCIIToUTF16("One"));
    menu_item->AppendMenuItemWithLabel(2, base::ASCIIToUTF16("Two"));
    // Run the menu, so that the menu item size will be calculated.
    menu_runner_->RunMenuAt(widget, nullptr, gfx::Rect(),
                            views::MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
    RunPendingMessages();
    // Figure out the middle of the first menu item.
    mouse_pos_.set_x(first_item_->width() / 2);
    mouse_pos_.set_y(first_item_->height() / 2);
    View::ConvertPointToScreen(
        menu_item->GetSubmenu()->GetWidget()->GetRootView(), &mouse_pos_);
    // Move the mouse so that it's where the menu will be shown.
    base::RunLoop run_loop;
    ui_controls::SendMouseMoveNotifyWhenDone(mouse_pos_.x(), mouse_pos_.y(),
                                             run_loop.QuitClosure());
    run_loop.Run();
    EXPECT_TRUE(first_item_->IsSelected());
    menu_runner_->Cancel();
    RunPendingMessages();
  }

  void RunPendingMessages() {
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 protected:
  MenuItemView* first_item_ = nullptr;
  std::unique_ptr<MenuRunner> menu_runner_;
  std::unique_ptr<MenuDelegate> menu_delegate_;
  // Middle of first menu item.
  gfx::Point mouse_pos_;

 private:
  DISALLOW_COPY_AND_ASSIGN(MenuControllerUITest);
};

IN_PROC_BROWSER_TEST_F(MenuControllerUITest, TestMouseOverShownMenu) {
  // Create a parent widget.
  Widget* widget = new views::Widget;
  Widget::InitParams params(Widget::InitParams::TYPE_WINDOW);
#if !defined(OS_CHROMEOS)
  params.native_widget = ::views::test::CreatePlatformDesktopNativeWidgetImpl(
      params, widget, nullptr);
#endif
  params.bounds = {0, 0, 200, 200};
  widget->Init(params);
  widget->Show();
  widget->Activate();
  // SetupMenu leaves the mouse position where the first menu item will be
  // when we run the menu.
  SetupMenu(widget);
  menu_runner_->RunMenuAt(widget, nullptr, gfx::Rect(),
                          views::MENU_ANCHOR_TOPLEFT, ui::MENU_SOURCE_NONE);
  // One or two mouse events are posted by the menu being shown.
  // Process event(s), and check what's selected in the menu.
  RunPendingMessages();
  EXPECT_FALSE(first_item_->IsSelected());
  // Move mouse one pixel to left and verify that the first menu item
  // is selected.
  mouse_pos_.Offset(-1, 0);
  base::RunLoop run_loop2;
  ui_controls::SendMouseMoveNotifyWhenDone(mouse_pos_.x(), mouse_pos_.y(),
                                           run_loop2.QuitClosure());
  run_loop2.Run();
  EXPECT_TRUE(first_item_->IsSelected());
  menu_runner_->Cancel();
  widget->Close();
}

}  // namespace test
}  // namespace views
