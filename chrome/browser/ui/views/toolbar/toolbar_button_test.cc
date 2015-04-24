// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/toolbar/toolbar_button.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/base/view_event_test_base.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/test/ui_controls.h"

class ToolbarButtonDragTest : public ViewEventTestBase,
                                     ui::SimpleMenuModel::Delegate {
 public:
  ToolbarButtonDragTest()
      : button_(NULL),
        menu_shown_(false),
        menu_closed_(false) {
  }

  ~ToolbarButtonDragTest() override {
  }

  // ViewEventTestBase implementation.
  void SetUp() override {
    button_ = new ToolbarButton(NULL, new ui::SimpleMenuModel(this));

    ViewEventTestBase::SetUp();
  }

  void TearDown() override {
    ViewEventTestBase::TearDown();
  }

  views::View* CreateContentsView() override {
    return button_;
  }

  gfx::Size GetPreferredSize() const override {
    return button_->GetPreferredSize();
  }

  // ui::SimpleMenuModel::Delegate implementation.
  bool IsCommandIdChecked(int id) const override {
    return false;
  }

  bool IsCommandIdEnabled(int id) const override {
    return true;
  }

  bool GetAcceleratorForCommandId(
      int id,
      ui::Accelerator* accelerator) override {
    return false;
  }

  void ExecuteCommand(int id, int event_flags) override {
  }

  void MenuWillShow(ui::SimpleMenuModel* /*source*/) override {
    menu_shown_ = true;
  }

  void MenuClosed(ui::SimpleMenuModel* /*source*/) override {
    menu_closed_ = true;
  }

  // ViewEventTestBase implementation.
  void DoTestOnMessageLoop() override {
    // Click on the ToolbarButton.
    ui_test_utils::MoveMouseToCenterAndPress(
        button_,
        ui_controls::LEFT,
        ui_controls::DOWN,
        CreateEventTask(this, &ToolbarButtonDragTest::Step1));
  }

  void Step1() {
    // Drag to invoke the menu.
    gfx::Point view_center;
    views::View::ConvertPointToScreen(button_, &view_center);
    // The 50 is a bit arbitrary. We just need a value greater than the drag
    // threshold.
    ui_controls::SendMouseMoveNotifyWhenDone(
        view_center.x(), view_center.y() + 50,
        CreateEventTask(this, &ToolbarButtonDragTest::Step2));
  }

  void Step2() {
    ASSERT_TRUE(menu_shown_);

    // Release.
    ui_controls::SendMouseEventsNotifyWhenDone(
        ui_controls::LEFT,
        ui_controls::UP,
        CreateEventTask(this, &ToolbarButtonDragTest::Step3));
  }

  void Step3() {
    // Click mouse to dismiss menu.  The views menu does not dismiss the
    // menu on click-drag-release unless an item is selected.
    ui_test_utils::MoveMouseToCenterAndPress(
        button_,
        ui_controls::LEFT,
        ui_controls::DOWN | ui_controls::UP,
        CreateEventTask(this, &ToolbarButtonDragTest::Step4));
  }

  void Step4() {
    // One more hop is required because ui::SimpleMenuModel calls
    // ui::SimpleMenuModel::Delegate::MenuClosed() via a posted
    // task.
    base::MessageLoopForUI::current()->PostTask(
        FROM_HERE, CreateEventTask(this, &ToolbarButtonDragTest::Step5));
  }

  void Step5() {
    ASSERT_TRUE(menu_closed_);
    Done();
  }

 private:
  ToolbarButton* button_;
  bool menu_shown_;
  bool menu_closed_;
};

// Broken since landed. crbug.com/325055
VIEW_TEST(ToolbarButtonDragTest, DISABLED_DragActivation)
